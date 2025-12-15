/* mwm statusbar - macOS menu bar integration
 *
 * Creates a minimal NSStatusItem in the system menu bar
 * showing current tag and layout mode.
 */

#import <Cocoa/Cocoa.h>
#include "statusbar.h"

static NSStatusItem *statusItem = nil;

void statusbar_init(void) {
    @autoreleasepool {
        /* Ensure we have an NSApplication instance */
        [NSApplication sharedApplication];

        /* Create status bar item with variable length */
        statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        [statusItem retain];

        /* Configure appearance */
        statusItem.button.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightMedium];

        /* Set initial title */
        statusItem.button.title = @"mwm";
    }
}

void statusbar_update(int tag, const char *layout, const char *window) {
    if (!statusItem) return;

    @autoreleasepool {
        NSString *layoutStr = layout ? [NSString stringWithUTF8String:layout] : @"";
        NSString *windowStr = @"";

        /* Truncate window name if too long */
        if (window && strlen(window) > 0) {
            NSString *fullName = [NSString stringWithUTF8String:window];
            if (fullName.length > 20) {
                windowStr = [[fullName substringToIndex:17] stringByAppendingString:@"..."];
            } else {
                windowStr = fullName;
            }
        }

        /* Format: [tag] [layout] window */
        NSString *title;
        if (windowStr.length > 0) {
            title = [NSString stringWithFormat:@"%d %@ %@", tag, layoutStr, windowStr];
        } else {
            title = [NSString stringWithFormat:@"%d %@", tag, layoutStr];
        }

        statusItem.button.title = title;
    }
}

void statusbar_cleanup(void) {
    if (statusItem) {
        @autoreleasepool {
            [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
            [statusItem release];
            statusItem = nil;
        }
    }
}

