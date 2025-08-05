#include "util.h"

static Cursor cursor = None;
static Cursor hidden_cursor = None;

void spawn(const char *cmd) {
	if (fork() == 0) {
		setsid();
		execl("/bin/sh", "sh", "-c", cmd, NULL);
		exit(EXIT_FAILURE);
	}
}

void free_cursor(Display *display, Window win) {
	XUndefineCursor(display, win);
	if (hidden_cursor != None) {
		XFreeCursor(display, hidden_cursor);
		hidden_cursor = None;
	}
	if (cursor != None) {
		XFreeCursor(display, cursor);
		cursor = None;
	}
}

void hide_cursor(Display *display, Window win) {
	if (hidden_cursor == None) {
		Pixmap blank;
		XColor dummy;
		char data[1] = {0};

		blank = XCreateBitmapFromData(display, win, data, 1, 1);
		hidden_cursor = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
		XFreePixmap(display, blank);
	}

	XDefineCursor(display, win, hidden_cursor);
}

void show_cursor(Display *display, Window win) {
	XUndefineCursor(display, win);
}

void make_cursor(Display *display, Window win) {
	cursor = XCreateFontCursor(display, XC_left_ptr);
	XDefineCursor(display, win, cursor);
}

