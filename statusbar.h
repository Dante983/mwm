/* mwm statusbar - macOS menu bar integration */

#ifndef STATUSBAR_H
#define STATUSBAR_H

/* Initialize the status bar item */
void statusbar_init(void);

/* Update the status bar display
 * tag: current tag number (1-9)
 * layout: layout symbol (e.g., "[]=", "[M]", "><>")
 * window: current window name (can be NULL)
 */
void statusbar_update(int tag, const char *layout, const char *window);

/* Cleanup the status bar */
void statusbar_cleanup(void);

#endif /* STATUSBAR_H */

