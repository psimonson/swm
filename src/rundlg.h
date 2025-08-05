#ifndef RUNDLG_H
#define RUNDLG_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXLEN 64

typedef struct _rundlg {
	Display *display;
	int screen;
	GC gc;
	XFontStruct *font;
	Window window;
	Window input_field;
	Window prev_focused_win;
	char input_text[MAXLEN];
	int input_len;
	int prev_revert_to;
} rundlg_t;

int rundlg_init(Display *d, int screen);
void rundlg_show();
void rundlg_free();

#endif // RUNDLG_H