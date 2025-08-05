#ifndef LSCREEN_H
#define LSCREEN_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXPASS 16

typedef struct _lscreen {
	Display *display;
	int screen;
	GC gc;
	XFontStruct *font;
	Window window;
	Window input_field;
	Window prev_focused_win;
	unsigned char hash[SHA512_DIGEST_LENGTH];
	char input_text[MAXPASS];
	int input_len;
	int prev_revert_to;
} lscreen_t;

int lscreen_init(Display *d, int screen);
void lscreen_show();
void lscreen_free();

#endif /* LSCREEN_H */

