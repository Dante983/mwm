/* mwm - minimal window manager for macOS
 *
 * A DWM-inspired tiling window manager following suckless philosophy.
 * Uses macOS Accessibility API for window management.
 *
 * (c) 2024 - MIT License
 */

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "statusbar.h"
#include "cJSON.h"

/* PID file for single instance */
#define PIDFILE "/tmp/mwm.pid"
static int pidfd = -1;

/* State file for window persistence */
#define STATEFILE "/tmp/mwm-state.json"

/* macros */
#define LENGTH(X)       (sizeof(X) / sizeof(X[0]))
#ifndef MAX
#define MAX(A, B)       ((A) > (B) ? (A) : (B))
#endif
#ifndef MIN
#define MIN(A, B)       ((A) < (B) ? (A) : (B))
#endif
#define TAGMASK         ((1 << LENGTH(tags)) - 1)
#define ISVISIBLE(C)    ((C->tags & tagset[seltags]))

#define TAGKEYS(KEY,TAG) \
    { MODKEY,           KEY, view,      {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask, KEY, tag,       {.ui = 1 << TAG} }, \
    { MODKEY|CtrlMask,  KEY, toggleview,{.ui = 1 << TAG} },

/* modifier masks */
#define Mod1        (1 << 0)  /* Option/Alt */
#define Mod4        (1 << 1)  /* Command */
#define ShiftMask   (1 << 2)
#define CtrlMask    (1 << 3)

/* types */
typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

typedef struct {
    unsigned int mod;
    unsigned int keycode;
    void (*func)(const Arg *);
    Arg arg;
} Key;

typedef struct {
    const char *app;
    unsigned int tags;
    int isfloating;
} Rule;

typedef struct Client Client;
struct Client {
    char name[256];
    CGRect frame;
    AXUIElementRef win;
    pid_t pid;
    unsigned int tags;
    int isfloating;
    int isfullscreen;
    Client *next;
    Client *prev;
};

typedef struct {
    const char *symbol;
    void (*arrange)(void);
} Layout;

/* function declarations */
static void arrange(void);
static int canmanage(AXUIElementRef win);
static void cleanup(void);
static void cyclelayout(const Arg *arg);
static void detach(Client *c);
static void die(const char *fmt, ...);
static void focus(Client *c);
static void focuslast(const Arg *arg);
static void focusnext(const Arg *arg);
static void focusprev(const Arg *arg);
static CGRect getframe(AXUIElementRef win);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void killclient(const Arg *arg);
static void manage(AXUIElementRef win, pid_t pid);
static void monocle(void);
static void movewindow(AXUIElementRef win, CGPoint pos);
static void quit(const Arg *arg);
static void resizewindow(AXUIElementRef win, CGSize size);
static void run(void);
static void loadstate(void);
static int restorestate(const char *appname, unsigned int *tags, int *floating);
static void savestate(void);
static void scan(void);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void sighandler(int sig);
static void spawn(const Arg *arg);
static void swapnext(const Arg *arg);
static void swapprev(const Arg *arg);
static void tag(const Arg *arg);
static void tile(void);
static void togglefloat(const Arg *arg);
static void toggleview(const Arg *arg);
static void unmanage(Client *c);
static void updateclients(void);
static void updatestatusbar(void);
static void view(const Arg *arg);

/* configuration - include first for constants */
#include "config.h"

/* global variables */
static int running = 1;
static int windowschanged = 0;
static Client *clients = NULL;
static Client *sel = NULL;
static Client *lastsel = NULL;
static CGRect screen;
static float g_mfact;
static int g_nmaster;
static unsigned int seltags = 0;
static unsigned int tagset[] = {1, 1};
static int sellay = 0;
static CFMachPortRef evtap = NULL;
static CFRunLoopSourceRef rlsrc = NULL;

/* function implementations */
static void
die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

static void
updatestatusbar(void) {
    /* calculate current tag number (1-based) from bitmask */
    int tag = 1;
    unsigned int t = tagset[seltags];
    while (t > 1) {
        t >>= 1;
        tag++;
    }

    /* get current layout symbol */
    const char *layout = layouts[sellay].symbol;

    /* get current window name */
    const char *window = sel ? sel->name : NULL;

    statusbar_update(tag, layout, window);
}

static int
canmanage(AXUIElementRef win) {
    CFBooleanRef minimized = NULL;
    CFStringRef subrole = NULL;
    int result = 0;

    /* skip minimized windows */
    if (AXUIElementCopyAttributeValue(win, kAXMinimizedAttribute,
                                       (CFTypeRef *)&minimized) == kAXErrorSuccess) {
        if (CFBooleanGetValue(minimized)) {
            CFRelease(minimized);
            return 0;
        }
        CFRelease(minimized);
    }

    /* check subrole - we want standard windows */
    if (AXUIElementCopyAttributeValue(win, kAXSubroleAttribute,
                                       (CFTypeRef *)&subrole) == kAXErrorSuccess) {
        if (CFStringCompare(subrole, kAXStandardWindowSubrole, 0) == kCFCompareEqualTo) {
            result = 1;
        }
        CFRelease(subrole);
    }

    return result;
}

static CGRect
getframe(AXUIElementRef win) {
    CGRect frame = CGRectZero;
    AXValueRef posval = NULL, sizeval = NULL;
    CGPoint pos;
    CGSize size;

    if (AXUIElementCopyAttributeValue(win, kAXPositionAttribute,
                                       (CFTypeRef *)&posval) == kAXErrorSuccess) {
        AXValueGetValue(posval, kAXValueCGPointType, &pos);
        CFRelease(posval);
        frame.origin = pos;
    }

    if (AXUIElementCopyAttributeValue(win, kAXSizeAttribute,
                                       (CFTypeRef *)&sizeval) == kAXErrorSuccess) {
        AXValueGetValue(sizeval, kAXValueCGSizeType, &size);
        CFRelease(sizeval);
        frame.size = size;
    }

    return frame;
}

static void
movewindow(AXUIElementRef win, CGPoint pos) {
    AXValueRef posval = AXValueCreate(kAXValueCGPointType, &pos);
    if (posval) {
        AXUIElementSetAttributeValue(win, kAXPositionAttribute, posval);
        CFRelease(posval);
    }
}

static void
resizewindow(AXUIElementRef win, CGSize size) {
    AXValueRef sizeval = AXValueCreate(kAXValueCGSizeType, &size);
    if (sizeval) {
        AXUIElementSetAttributeValue(win, kAXSizeAttribute, sizeval);
        CFRelease(sizeval);
    }
}

static void
manage(AXUIElementRef win, pid_t pid) {
    Client *c;
    CFStringRef titleref = NULL;
    ProcessSerialNumber psn;
    CFStringRef appname = NULL;

    c = calloc(1, sizeof(Client));
    if (!c)
        die("mwm: cannot allocate memory\n");

    c->win = win;
    CFRetain(win);
    c->pid = pid;
    c->tags = tagset[seltags];
    c->isfloating = 0;
    c->frame = getframe(win);
    c->name[0] = '\0';

    /* get window title */
    if (AXUIElementCopyAttributeValue(win, kAXTitleAttribute,
                                       (CFTypeRef *)&titleref) == kAXErrorSuccess) {
        CFStringGetCString(titleref, c->name, sizeof(c->name), kCFStringEncodingUTF8);
        CFRelease(titleref);
    }

    /* apply rules */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    GetProcessForPID(pid, &psn);
    CopyProcessName(&psn, &appname);
#pragma clang diagnostic pop

    if (appname) {
        char app[256];
        CFStringGetCString(appname, app, sizeof(app), kCFStringEncodingUTF8);
        CFRelease(appname);

        /* apply rules from config.h */
        for (size_t i = 0; i < LENGTH(rules); i++) {
            if (strstr(app, rules[i].app)) {
                if (rules[i].tags)
                    c->tags = rules[i].tags;
                c->isfloating = rules[i].isfloating;
                break;
            }
        }

        /* restore saved state (overrides rules) */
        unsigned int saved_tags = 0;
        int saved_floating = 0;
        if (restorestate(app, &saved_tags, &saved_floating)) {
            if (saved_tags)
                c->tags = saved_tags;
            c->isfloating = saved_floating;
#ifdef DEBUG
            printf("mwm: restored state for '%s' -> tags=%u, floating=%d\n",
                   app, c->tags, c->isfloating);
            fflush(stdout);
#endif
        }
    }

    /* attach to client list */
    c->next = clients;
    if (clients)
        clients->prev = c;
    clients = c;

    focus(c);
}

static void
unmanage(Client *c) {
    detach(c);
    if (c->win)
        CFRelease(c->win);
    if (sel == c) {
        sel = clients;
        if (sel)
            focus(sel);
    }
    free(c);
}

static void
detach(Client *c) {
    if (c->prev)
        c->prev->next = c->next;
    if (c->next)
        c->next->prev = c->prev;
    if (c == clients)
        clients = c->next;
    c->next = c->prev = NULL;
}

static void
focus(Client *c) {
    if (sel && sel != c)
        lastsel = sel;

    sel = c;

    if (!c) {
        updatestatusbar();
        return;
    }

    /* raise and focus the window */
    AXUIElementSetAttributeValue(c->win, kAXMainAttribute, kCFBooleanTrue);
    AXUIElementSetAttributeValue(c->win, kAXFocusedAttribute, kCFBooleanTrue);

    /* bring app to front */
    AXUIElementRef app = AXUIElementCreateApplication(c->pid);
    if (app) {
        AXUIElementSetAttributeValue(app, kAXFrontmostAttribute, kCFBooleanTrue);
        CFRelease(app);
    }

    updatestatusbar();
}

static void
focusnext(const Arg *arg) {
    Client *c;

    if (!sel)
        return;

    /* find next visible client */
    for (c = sel->next; c; c = c->next)
        if (ISVISIBLE(c))
            break;

    if (!c) {
        for (c = clients; c && c != sel; c = c->next)
            if (ISVISIBLE(c))
                break;
    }

    if (c && c != sel)
        focus(c);
}

static void
focusprev(const Arg *arg) {
    Client *c, *last = NULL;

    if (!sel)
        return;

    /* find previous visible client */
    for (c = clients; c != sel; c = c->next)
        if (ISVISIBLE(c))
            last = c;

    if (!last) {
        for (c = sel->next; c; c = c->next)
            if (ISVISIBLE(c))
                last = c;
    }

    if (last && last != sel)
        focus(last);
}

static void
focuslast(const Arg *arg) {
    if (lastsel && ISVISIBLE(lastsel))
        focus(lastsel);
}

static void
swapnext(const Arg *arg) {
    Client *c;

    if (!sel || sel->isfloating)
        return;

    for (c = sel->next; c; c = c->next)
        if (ISVISIBLE(c) && !c->isfloating)
            break;

    if (!c)
        return;

    /* trigger re-tile */
    windowschanged = 1;
    arrange();
}

static void
swapprev(const Arg *arg) {
    Client *c, *last = NULL;

    if (!sel || sel->isfloating)
        return;

    for (c = clients; c != sel; c = c->next)
        if (ISVISIBLE(c) && !c->isfloating)
            last = c;

    if (!last)
        return;

    windowschanged = 1;
    arrange();
}

static void
killclient(const Arg *arg) {
    if (!sel)
        return;

    /* try graceful close first */
    AXUIElementRef closebutton = NULL;
    if (AXUIElementCopyAttributeValue(sel->win, kAXCloseButtonAttribute,
                                       (CFTypeRef *)&closebutton) == kAXErrorSuccess) {
        AXUIElementPerformAction(closebutton, kAXPressAction);
        CFRelease(closebutton);
    }
}

static void
setmfact(const Arg *arg) {
    float f = g_mfact + arg->f;
    if (f < 0.1 || f > 0.9)
        return;
    g_mfact = f;
    windowschanged = 1;
    arrange();
}

static void
incnmaster(const Arg *arg) {
    g_nmaster = MAX(0, g_nmaster + arg->i);
    windowschanged = 1;
    arrange();
}

static void
setlayout(const Arg *arg) {
    if (arg->i >= 0 && arg->i < LayoutLast)
        sellay = arg->i;
    windowschanged = 1;
    arrange();
}

static void
cyclelayout(const Arg *arg) {
    sellay = (sellay + 1) % LayoutLast;
    windowschanged = 1;
    arrange();
}

static void
togglefloat(const Arg *arg) {
    if (!sel)
        return;
    sel->isfloating = !sel->isfloating;
    windowschanged = 1;
    arrange();
    savestate();  /* save window state after float toggle */
}

static void
view(const Arg *arg) {
    if ((arg->ui & TAGMASK) == tagset[seltags])
        return;
    seltags ^= 1;
    tagset[seltags] = arg->ui & TAGMASK;

#ifdef DEBUG
    printf("mwm: switching to tag %u\n", tagset[seltags]);
    fflush(stdout);
#endif

    windowschanged = 1;
    arrange();

    /* focus first visible client */
    Client *c;
    for (c = clients; c; c = c->next) {
        if (ISVISIBLE(c)) {
            focus(c);
            break;
        }
    }
}

static void
toggleview(const Arg *arg) {
    unsigned int newtagset = tagset[seltags] ^ (arg->ui & TAGMASK);
    if (newtagset) {
        tagset[seltags] = newtagset;
        windowschanged = 1;
        arrange();
    }
}

static void
tag(const Arg *arg) {
    if (sel && arg->ui & TAGMASK) {
#ifdef DEBUG
        printf("mwm: moving window '%s' to tag %u\n", sel->name, arg->ui);
        fflush(stdout);
#endif

        sel->tags = arg->ui & TAGMASK;
        windowschanged = 1;
        arrange();
        savestate();  /* save window state after tag change */

        /* focus next visible client */
        Client *c;
        for (c = clients; c; c = c->next) {
            if (ISVISIBLE(c)) {
                focus(c);
                break;
            }
        }
    }
}

static void
tile(void) {
    unsigned int n = 0, i = 0;
    Client *c;
    int mx, my, mw, mh;
    int sx, sy, sw, sh;
    int gap = gappx;

    /* count tiled clients */
    for (c = clients; c; c = c->next)
        if (ISVISIBLE(c) && !c->isfloating)
            n++;

    if (n == 0)
        return;

    /* calculate areas */
    mx = screen.origin.x + gap;
    my = screen.origin.y + gap;

    if (n <= (unsigned int)g_nmaster) {
        /* all in master */
        mw = screen.size.width - 2 * gap;
        mh = (screen.size.height - (n + 1) * gap) / n;
    } else {
        /* master + stack */
        mw = (screen.size.width - 3 * gap) * g_mfact;
        mh = (screen.size.height - (g_nmaster + 1) * gap) / g_nmaster;
        sx = mx + mw + gap;
        sy = my;
        sw = screen.size.width - mw - 3 * gap;
        sh = (screen.size.height - (n - g_nmaster + 1) * gap) / (n - g_nmaster);
    }

    /* arrange clients */
    for (c = clients; c; c = c->next) {
        if (!ISVISIBLE(c))
            continue;

        if (c->isfloating)
            continue;

        if (i < (unsigned int)g_nmaster) {
            /* master area */
            CGPoint pos = CGPointMake(mx, my + i * (mh + gap));
            CGSize size = CGSizeMake(n <= (unsigned int)g_nmaster ? mw : mw, mh);
            movewindow(c->win, pos);
            resizewindow(c->win, size);
            c->frame = CGRectMake(pos.x, pos.y, size.width, size.height);
        } else {
            /* stack area */
            CGPoint pos = CGPointMake(sx, sy + (i - g_nmaster) * (sh + gap));
            CGSize size = CGSizeMake(sw, sh);
            movewindow(c->win, pos);
            resizewindow(c->win, size);
            c->frame = CGRectMake(pos.x, pos.y, size.width, size.height);
        }
        i++;
    }
}

static void
monocle(void) {
    Client *c;
    int gap = gappx;

    for (c = clients; c; c = c->next) {
        if (!ISVISIBLE(c) || c->isfloating)
            continue;

        CGPoint pos = CGPointMake(screen.origin.x + gap, screen.origin.y + gap);
        CGSize size = CGSizeMake(screen.size.width - 2 * gap, screen.size.height - 2 * gap);
        movewindow(c->win, pos);
        resizewindow(c->win, size);
        c->frame = CGRectMake(pos.x, pos.y, size.width, size.height);
    }
}

static void
hidewindow(Client *c) {
    if (!c || !c->win)
        return;
    /* move window way off screen to "hide" it */
    CGPoint offscreen = CGPointMake(-10000, -10000);
    movewindow(c->win, offscreen);
}

static void
arrange(void) {
    Client *c;

    /* hide non-visible windows first */
    for (c = clients; c; c = c->next) {
        if (!ISVISIBLE(c)) {
            hidewindow(c);
        }
    }

    /* apply layout to visible windows */
    if (layouts[sellay].arrange)
        layouts[sellay].arrange();

    /* focus selected */
    if (sel && ISVISIBLE(sel))
        focus(sel);
    else {
        /* find first visible client to focus */
        for (c = clients; c; c = c->next) {
            if (ISVISIBLE(c)) {
                focus(c);
                break;
            }
        }
    }

    updatestatusbar();
}

static void
spawn(const Arg *arg) {
    const char **cmd = (const char **)arg->v;
    if (!cmd || !cmd[0])
        return;

#ifdef DEBUG
    printf("mwm: spawning %s\n", cmd[0]);
#endif
    fflush(stdout);

    /* use open command for .app bundles */
    if (strstr(cmd[0], ".app")) {
        char buf[512];
        snprintf(buf, sizeof(buf), "open \"%s\"", cmd[0]);
        int ret = system(buf);
        if (ret != 0) {
            fprintf(stderr, "mwm: spawn failed with %d\n", ret);
        }
    } else {
        if (fork() == 0) {
            setsid();
            execvp(cmd[0], (char *const *)cmd);
            die("mwm: execvp %s failed\n", cmd[0]);
        }
    }
}

static void
quit(const Arg *arg) {
    running = 0;
}

static void
updateclients(void) {
    CFArrayRef windowList = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID
    );

    if (!windowList)
        return;

    /* mark all clients as stale */
    Client *c, *next;
    for (c = clients; c; c = c->next)
        c->isfullscreen = -1;  /* using as "stale" marker */

    CFIndex count = CFArrayGetCount(windowList);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef wininfo = CFArrayGetValueAtIndex(windowList, i);

        CFNumberRef pidref = CFDictionaryGetValue(wininfo, kCGWindowOwnerPID);
        CFNumberRef layerref = CFDictionaryGetValue(wininfo, kCGWindowLayer);

        if (!pidref || !layerref)
            continue;

        pid_t pid;
        int layer;
        CFNumberGetValue(pidref, kCFNumberIntType, &pid);
        CFNumberGetValue(layerref, kCFNumberIntType, &layer);

        /* skip non-standard layers */
        if (layer != 0)
            continue;

        /* get AX element for window */
        AXUIElementRef app = AXUIElementCreateApplication(pid);
        if (!app)
            continue;

        CFArrayRef appwindows = NULL;
        if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute,
                                          (CFTypeRef *)&appwindows) != kAXErrorSuccess) {
            CFRelease(app);
            continue;
        }

        CFIndex wcount = CFArrayGetCount(appwindows);
        for (CFIndex j = 0; j < wcount; j++) {
            AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(appwindows, j);

            if (!canmanage(win))
                continue;

            /* check if already managed */
            int found = 0;
            for (c = clients; c; c = c->next) {
                if (c->pid == pid) {
                    CGRect f1 = getframe(win);
                    CGRect f2 = getframe(c->win);
                    if (CGRectEqualToRect(f1, f2)) {
                        c->isfullscreen = 0;  /* mark as active */
                        found = 1;
                        break;
                    }
                }
            }

            if (!found)
                manage(win, pid);
        }

        CFRelease(appwindows);
        CFRelease(app);
    }

    CFRelease(windowList);

    /* remove stale clients */
    for (c = clients; c; c = next) {
        next = c->next;
        if (c->isfullscreen == -1)
            unmanage(c);
        else
            c->isfullscreen = 0;
    }
}

static void
scan(void) {
    int oldcount = 0, newcount = 0;
    Client *c;

    /* count current clients */
    for (c = clients; c; c = c->next)
        oldcount++;

    updateclients();

    /* count after update */
    for (c = clients; c; c = c->next)
        newcount++;

    /* only arrange if window count changed or flag set */
    if (oldcount != newcount || windowschanged) {
#ifdef DEBUG
        printf("mwm: windows changed (%d -> %d), re-arranging\n", oldcount, newcount);
        fflush(stdout);
#endif
        arrange();
        windowschanged = 0;
    }
}

static CGEventRef
eventcallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
#ifdef DEBUG
    static int eventcount = 0;
#endif

    if (type == kCGEventKeyDown) {
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        CGEventFlags flags = CGEventGetFlags(event);

        unsigned int mod = 0;
        if (flags & kCGEventFlagMaskAlternate)  mod |= Mod1;
        if (flags & kCGEventFlagMaskCommand)    mod |= Mod4;
        if (flags & kCGEventFlagMaskShift)      mod |= ShiftMask;
        if (flags & kCGEventFlagMaskControl)    mod |= CtrlMask;

#ifdef DEBUG
        /* debug: show all key events with Option held */
        if (mod & Mod1) {
            printf("mwm: Option+key detected - keycode=%d (0x%02X) mod=%u\n",
                   keycode, keycode, mod);
            fflush(stdout);
        }
#endif

        for (size_t i = 0; i < LENGTH(keys); i++) {
            if (keys[i].keycode == keycode && keys[i].mod == mod) {
#ifdef DEBUG
                printf("mwm: executing binding for keycode=%d\n", keycode);
                fflush(stdout);
#endif
                keys[i].func(&keys[i].arg);
                return NULL;  /* consume event */
            }
        }
    } else if (type == kCGEventTapDisabledByTimeout ||
               type == kCGEventTapDisabledByUserInput) {
#ifdef DEBUG
        printf("mwm: event tap was disabled, re-enabling\n");
        fflush(stdout);
#endif
        CGEventTapEnable(evtap, true);
    }

#ifdef DEBUG
    /* periodic status every 100 events */
    if (++eventcount % 100 == 0) {
        printf("mwm: processed %d events\n", eventcount);
        fflush(stdout);
    }
#endif

    return event;
}

static void
grabkeys(void) {
    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown);

    evtap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        mask,
        eventcallback,
        NULL
    );

    if (!evtap)
        die("mwm: failed to create event tap. Check accessibility permissions.\n");

    rlsrc = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, evtap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlsrc, kCFRunLoopCommonModes);
    CGEventTapEnable(evtap, true);
}

static void
sighandler(int sig) {
    running = 0;
}

static int
acquirelock(void) {
    pidfd = open(PIDFILE, O_CREAT | O_RDWR, 0644);
    if (pidfd < 0) {
        fprintf(stderr, "mwm: cannot open pid file: %s\n", strerror(errno));
        return 0;
    }

    if (flock(pidfd, LOCK_EX | LOCK_NB) < 0) {
        if (errno == EWOULDBLOCK) {
            fprintf(stderr, "mwm: another instance is already running\n");
        } else {
            fprintf(stderr, "mwm: cannot lock pid file: %s\n", strerror(errno));
        }
        close(pidfd);
        pidfd = -1;
        return 0;
    }

    /* write our PID */
    ftruncate(pidfd, 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\n", getpid());
    write(pidfd, buf, strlen(buf));

    return 1;
}

static void
releaselock(void) {
    if (pidfd >= 0) {
        flock(pidfd, LOCK_UN);
        close(pidfd);
        unlink(PIDFILE);
        pidfd = -1;
    }
}

static void
savestate(void) {
#ifdef DEBUG
    printf("mwm: savestate() called\n");
    fflush(stdout);
#endif

    cJSON *root = cJSON_CreateObject();
    cJSON *windows = cJSON_CreateArray();

    /* iterate through all clients and save their state */
    for (Client *c = clients; c; c = c->next) {
        /* get application name from window */
        char app[256] = {0};
        CFStringRef appname = NULL;
        pid_t pid = c->pid;

        ProcessSerialNumber psn;
        if (GetProcessForPID(pid, &psn) == noErr) {
            CopyProcessName(&psn, &appname);
            if (appname) {
                CFStringGetCString(appname, app, sizeof(app), kCFStringEncodingUTF8);
                CFRelease(appname);
            }
        }

        if (app[0] == '\0')
            continue;

        /* create window entry */
        cJSON *window = cJSON_CreateObject();
        cJSON_AddStringToObject(window, "app", app);
        cJSON_AddNumberToObject(window, "tags", c->tags);
        cJSON_AddNumberToObject(window, "floating", c->isfloating);
        cJSON_AddItemToArray(windows, window);

#ifdef DEBUG
        printf("mwm: saving state for '%s' -> tags=%u, floating=%d\n",
               app, c->tags, c->isfloating);
        fflush(stdout);
#endif
    }

    cJSON_AddItemToObject(root, "windows", windows);

    /* write to file */
    char *json_str = cJSON_Print(root);
    if (json_str) {
        FILE *f = fopen(STATEFILE, "w");
        if (f) {
            fprintf(f, "%s", json_str);
            fclose(f);
#ifdef DEBUG
            printf("mwm: state written to %s\n", STATEFILE);
            fflush(stdout);
#endif
        } else {
#ifdef DEBUG
            printf("mwm: failed to open %s for writing: %s\n", STATEFILE, strerror(errno));
            fflush(stdout);
#endif
        }
        free(json_str);
    }

    cJSON_Delete(root);
}

static void
loadstate(void) {
    /* state is loaded on-demand in manage() */
    /* this function ensures the state file exists */
    struct stat st;
    if (stat(STATEFILE, &st) != 0) {
        /* create empty state file */
        FILE *f = fopen(STATEFILE, "w");
        if (f) {
            fprintf(f, "{\"windows\":[]}\n");
            fclose(f);
        }
    }
}

static int
restorestate(const char *appname, unsigned int *tags, int *floating) {
    FILE *f = fopen(STATEFILE, "r");
    if (!f)
        return 0;

    /* read entire file */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json_str = malloc(fsize + 1);
    if (!json_str) {
        fclose(f);
        return 0;
    }

    fread(json_str, 1, fsize, f);
    json_str[fsize] = '\0';
    fclose(f);

    /* parse JSON */
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root)
        return 0;

    cJSON *windows = cJSON_GetObjectItem(root, "windows");
    if (!windows || !cJSON_IsArray(windows)) {
        cJSON_Delete(root);
        return 0;
    }

    /* search for matching app */
    int found = 0;
    cJSON *window = NULL;
    cJSON_ArrayForEach(window, windows) {
        cJSON *app = cJSON_GetObjectItem(window, "app");
        if (app && cJSON_IsString(app)) {
            if (strcmp(app->valuestring, appname) == 0) {
                cJSON *tags_item = cJSON_GetObjectItem(window, "tags");
                cJSON *floating_item = cJSON_GetObjectItem(window, "floating");

                if (tags_item && cJSON_IsNumber(tags_item))
                    *tags = (unsigned int)tags_item->valueint;
                if (floating_item && cJSON_IsNumber(floating_item))
                    *floating = floating_item->valueint;

                found = 1;
                break;
            }
        }
    }

    cJSON_Delete(root);
    return found;
}

static void
setup(void) {
    /* single instance check */
    if (!acquirelock()) {
        exit(1);
    }

    /* check accessibility permissions - pure C approach */
    CFStringRef keys[] = { kAXTrustedCheckOptionPrompt };
    CFTypeRef values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void **)keys,
        (const void **)values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    Boolean trusted = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);

    if (!trusted) {
        fprintf(stderr, "mwm: Accessibility permissions required.\n");
        fprintf(stderr, "     Go to System Settings → Privacy & Security → Accessibility\n");
        fprintf(stderr, "     and add mwm to the allowed apps.\n");
    }

    /* get screen size */
    CGDirectDisplayID mainDisplay = CGMainDisplayID();
    screen = CGDisplayBounds(mainDisplay);

    /* account for menu bar (approximate) */
    screen.origin.y += 25;
    screen.size.height -= 25;

    /* get dock info and adjust */
    /* TODO: properly detect dock position and size */
    screen.size.height -= 70;  /* approximate dock height */

    /* initialize state */
    g_mfact = DEFAULT_MFACT;
    g_nmaster = DEFAULT_NMASTER;
    tagset[0] = tagset[1] = 1;
    sellay = 0;

    /* load saved window state */
    loadstate();

    /* signal handlers */
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    /* grab keys */
    grabkeys();

    /* initialize status bar */
    statusbar_init();

    printf("mwm: started\n");
    printf("     screen: %.0fx%.0f @ (%.0f,%.0f)\n",
           screen.size.width, screen.size.height,
           screen.origin.x, screen.origin.y);
}

static void
cleanup(void) {
    Client *c, *next;

    for (c = clients; c; c = next) {
        next = c->next;
        if (c->win)
            CFRelease(c->win);
        free(c);
    }

    if (rlsrc) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rlsrc, kCFRunLoopCommonModes);
        CFRelease(rlsrc);
    }
    if (evtap)
        CFRelease(evtap);

    statusbar_cleanup();
    releaselock();
    printf("mwm: stopped\n");
}

/* periodic timer callback to scan for windows */
static void
timercallback(CFRunLoopTimerRef timer, void *info) {
    scan();
}

static void
run(void) {
    /* create timer to periodically scan windows */
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
        kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent(),
        1.0,  /* scan every second */
        0, 0,
        timercallback,
        NULL
    );
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);

    /* initial scan */
    scan();

    /* main event loop */
    while (running) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    }

    CFRunLoopTimerInvalidate(timer);
    CFRelease(timer);
}

int
main(int argc, char *argv[]) {
    if (argc > 1) {
        if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
            printf("mwm-0.1\n");
            return 0;
        }
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            printf("usage: mwm [-v] [-h]\n");
            return 0;
        }
    }

    setup();
    run();
    cleanup();

    return 0;
}

