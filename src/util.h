#ifndef UTIL_H
#define UTIL_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <unistd.h>

void spawn(const char *cmd);
void make_cursor(Display *display, Window win);
void hide_cursor(Display *display, Window win);
void show_cursor(Display *display, Window win);
void free_cursor(Display *display, Window win);

#endif /* UTIL_H */

