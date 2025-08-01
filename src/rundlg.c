#include "rundlg.h"
#include "util.h"

static rundlg_t rundlg;

/* Initialize the lock screen */
int rundlg_init(Display *display, int screen) {
	rundlg.display = display;
	rundlg.screen = screen;

	/* Store the current focused window before locking */
	XGetInputFocus(display, &rundlg.prev_focused_win, &rundlg.prev_revert_to);

	/* Create the simple window */
	rundlg.window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 400, 200, 1, 0, WhitePixel(display, screen));
	if (rundlg.window == None) return 0;
	XSelectInput(display, rundlg.window, ExposureMask | KeyPressMask);

	/* Create the graphics context for window */
	rundlg.gc = XCreateGC(rundlg.display, rundlg.window, 0, NULL);
	if (!rundlg.gc) {
		XDestroyWindow(display, rundlg.window);
		return 0;
	}

	/* Create the font */
	rundlg.font = XLoadQueryFont(rundlg.display, "fixed");
	if (rundlg.font) {
		XSetFont(display, rundlg.gc, rundlg.font->fid);
	}

	rundlg.input_field = XCreateSimpleWindow(display, rundlg.window, (400 - 300) / 2, 200 / 2 - 10, 300, 20, 1, 0, WhitePixel(display, screen));
	if (!rundlg.input_field) {
		XDestroyWindow(display, rundlg.window);
		return 0;
	}
	XSelectInput(display, rundlg.input_field, KeyPressMask);

	memset(rundlg.input_text, 0, sizeof(rundlg.input_text)-1);
	rundlg.input_len = 0;
	return 1;
}

/* Run the main loop */
void rundlg_show() {
	/* Grab input */
	XGrabKeyboard(rundlg.display, RootWindow(rundlg.display, rundlg.screen), True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(rundlg.display, RootWindow(rundlg.display, rundlg.screen), True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

	/* Display the window */
	XMapRaised(rundlg.display, rundlg.window);
	XMapWindow(rundlg.display, rundlg.input_field);
	XSetInputFocus(rundlg.display, rundlg.window, RevertToParent, CurrentTime);
	XSync(rundlg.display, False);
	XFlush(rundlg.display);

	XEvent ev;
	for (;;) {
		XNextEvent(rundlg.display, &ev);
		if (ev.type == Expose) {
			XClearWindow(rundlg.display, rundlg.input_field);
			XDrawString(rundlg.display, rundlg.input_field, rundlg.gc, 5, 12, rundlg.input_text, rundlg.input_len);
			XFlush(rundlg.display);
		} else if (ev.type == KeyPress) {
			KeySym key = XLookupKeysym(&ev.xkey, 0);
			if (key == XK_Escape) {
                memset(rundlg.input_text, 0, MAXLEN);
                rundlg.input_len = 0;
			} else if (key == XK_Return) {
				/* Accept input field entry */
				spawn(rundlg.input_text);
                memset(rundlg.input_text, 0, MAXLEN);
                rundlg.input_len = 0;
				XUnmapWindow(rundlg.display, rundlg.input_field);
				XUnmapWindow(rundlg.display, rundlg.window);
                break;
			} else if (key == XK_BackSpace && rundlg.input_len > 0) {
				rundlg.input_text[--rundlg.input_len] = '\0';
			} else if (rundlg.input_len < (int)(sizeof(rundlg.input_text)-1) && key >= XK_space && key <= XK_asciitilde) {
				/* Handle input field */
                char buffer[10];
                XComposeStatus compose;
                int len = XLookupString(&ev.xkey, buffer, sizeof(buffer)-1, &key, &compose);
                buffer[len] = '\0';
                rundlg.input_text[rundlg.input_len++] = buffer[0];
                rundlg.input_text[rundlg.input_len] = '\0';
			}

			XClearWindow(rundlg.display, rundlg.input_field);
			XDrawString(rundlg.display, rundlg.input_field, rundlg.gc, 5, 12, rundlg.input_text, rundlg.input_len);
			XFlush(rundlg.display);
		}
	}

	/* Ungrab input before restoring focus */
	XUngrabKeyboard(rundlg.display, CurrentTime);
	XUngrabPointer(rundlg.display, CurrentTime);
	XFlush(rundlg.display);

	/* Restore hotkeys and focus */
	if (rundlg.prev_focused_win) {
		XSetInputFocus(rundlg.display, rundlg.prev_focused_win, rundlg.prev_revert_to, CurrentTime);
	} else {
		XSetInputFocus(rundlg.display, RootWindow(rundlg.display, rundlg.screen), PointerRoot, CurrentTime);
	}

    XFlush(rundlg.display);
}

/* Free the lock screen */
void rundlg_free() {
	XFreeGC(rundlg.display, rundlg.gc);
	XFreeFont(rundlg.display, rundlg.font);
	XDestroyWindow(rundlg.display, rundlg.input_field);
	XDestroyWindow(rundlg.display, rundlg.window);
}
