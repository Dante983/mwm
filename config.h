/* mwm - minimal window manager for macOS
 * Configuration file - edit and recompile
 */

#ifndef CONFIG_H
#define CONFIG_H

/* debug output (comment out to disable) */
// #define DEBUG 1

/* appearance */
#define DEFAULT_MFACT   0.55f  /* master area size [0.05..0.95] */
#define DEFAULT_NMASTER 1      /* number of clients in master area */
static const unsigned int gappx = 10;    /* gap pixel between windows */
static const unsigned int borderpx = 2;  /* border pixel (visual only) */

/* apps */
static const char *termcmd[] = { "/Applications/Ghostty.app", NULL };

/*
 * Modifier key:
 * Mod1 = Option (Alt)
 * Mod4 = Command
 */
#define MODKEY Mod1

/*
 * Key definitions
 * Standard macOS virtual key codes
 */
#define Key_Return  0x24
#define Key_Space   0x31
#define Key_Tab     0x30
#define Key_Q       0x0C
#define Key_W       0x0D
#define Key_E       0x0E
#define Key_R       0x0F
#define Key_T       0x11
#define Key_Y       0x10
#define Key_U       0x20
#define Key_I       0x22
#define Key_O       0x1F
#define Key_P       0x23
#define Key_A       0x00
#define Key_S       0x01
#define Key_D       0x02
#define Key_F       0x03
#define Key_G       0x05
#define Key_H       0x04
#define Key_J       0x26
#define Key_K       0x28
#define Key_L       0x25
#define Key_Z       0x06
#define Key_X       0x07
#define Key_C       0x08
#define Key_V       0x09
#define Key_B       0x0B
#define Key_N       0x2D
#define Key_M       0x2E
#define Key_1       0x12
#define Key_2       0x13
#define Key_3       0x14
#define Key_4       0x15
#define Key_5       0x17
#define Key_6       0x16
#define Key_7       0x1A
#define Key_8       0x1C
#define Key_9       0x19
#define Key_0       0x1D

/* tags/workspaces */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* rules: app name, tag mask (0 = current), floating */
static const Rule rules[] = {
    /* app                          tag     floating */
    { "System Preferences",         0,      1 },
    { "System Settings",            0,      1 },
    { "Calculator",                 0,      1 },
    { "Preview",                    0,      1 },
};

/* layouts */
enum { LayoutTile, LayoutMonocle, LayoutFloat, LayoutLast };

static const Layout layouts[] = {
    /* symbol   arrange function */
    { "[]=",    tile },     /* tiled (default) */
    { "[M]",    monocle },  /* monocle */
    { "><>",    NULL },     /* floating */
};

/* key bindings */
static Key keys[] = {
    /* modifier         key         function        argument */
    { MODKEY,           Key_Return, spawn,          {.v = termcmd} },
    { MODKEY,           Key_J,      focusnext,      {0} },
    { MODKEY,           Key_K,      focusprev,      {0} },
    { MODKEY|ShiftMask, Key_J,      swapnext,       {0} },
    { MODKEY|ShiftMask, Key_K,      swapprev,       {0} },
    { MODKEY,           Key_H,      setmfact,       {.f = -0.05} },
    { MODKEY,           Key_L,      setmfact,       {.f = +0.05} },
    { MODKEY,           Key_I,      incnmaster,     {.i = +1} },
    { MODKEY,           Key_D,      incnmaster,     {.i = -1} },
    { MODKEY|ShiftMask, Key_C,      killclient,     {0} },
    { MODKEY,           Key_T,      setlayout,      {.i = LayoutTile} },
    { MODKEY,           Key_M,      setlayout,      {.i = LayoutMonocle} },
    { MODKEY,           Key_F,      setlayout,      {.i = LayoutFloat} },
    { MODKEY,           Key_Space,  cyclelayout,    {0} },
    { MODKEY|ShiftMask, Key_Space,  togglefloat,    {0} },
    { MODKEY,           Key_Tab,    focuslast,      {0} },
    TAGKEYS(            Key_1,                      0)
    TAGKEYS(            Key_2,                      1)
    TAGKEYS(            Key_3,                      2)
    TAGKEYS(            Key_4,                      3)
    TAGKEYS(            Key_5,                      4)
    TAGKEYS(            Key_6,                      5)
    TAGKEYS(            Key_7,                      6)
    TAGKEYS(            Key_8,                      7)
    TAGKEYS(            Key_9,                      8)
    { MODKEY|ShiftMask, Key_Q,      quit,           {0} },
};

#endif /* CONFIG_H */

