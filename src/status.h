#ifndef STATUS_H
#define STATUS_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define BAR_HEIGHT 20  // Status bar height in pixels
#define UPDATE_INTERVAL 1  // Update every second

typedef struct {
    Display *display;
    Window root;
    GC gc;
    int screen;
    XFontStruct *font;
    char window_title[128];
} StatusBar;

/* Function prototypes */
int status_init(Display *display, int screen);
void status_free();

#endif /* STATUS_H */

