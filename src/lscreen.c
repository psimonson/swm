#include "lscreen.h"
#include "util.h"

static lscreen_t lscreen;

/* Initialize the lock screen */
int lscreen_init(Display *display, int screen) {
	lscreen.display = display;
	lscreen.screen = screen;

	/* Get the hash if not default to a hash */
	const char *defhash = "password";
	char filename[512] = {0};
	snprintf(filename, sizeof(filename)-1, "%s/.swmhash", getenv("HOME"));
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		SHA512((unsigned char *)defhash, strlen(defhash), lscreen.hash);
	} else {
		fseek(fp, 0, SEEK_END);
		size_t size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (size != SHA512_DIGEST_LENGTH) {
			SHA512((unsigned char *)defhash, strlen(defhash), lscreen.hash);
		} else {
			fread(lscreen.hash, sizeof(uint8_t), SHA512_DIGEST_LENGTH, fp);
		}
		fclose(fp);
	}

	/* Store the current focused window before locking */
	XGetInputFocus(display, &lscreen.prev_focused_win, &lscreen.prev_revert_to);

	/* Create the simple window */
	XWindowAttributes wa;
	if (!XGetWindowAttributes(display, RootWindow(display, screen), &wa)) return 0;
	lscreen.window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, wa.width, wa.height, 1, 0, BlackPixel(display, screen));
	if (lscreen.window == None) return 0;
	XSelectInput(display, lscreen.window, ExposureMask | KeyPressMask);

	/* Create the graphics context for window */
	lscreen.gc = XCreateGC(lscreen.display, lscreen.window, 0, NULL);
	if (!lscreen.gc) {
		XDestroyWindow(display, lscreen.window);
		return 0;
	}

	/* Create the font */
	lscreen.font = XLoadQueryFont(lscreen.display, "fixed");
	if (lscreen.font) {
		XSetFont(display, lscreen.gc, lscreen.font->fid);
	}

	lscreen.input_field = XCreateSimpleWindow(display, lscreen.window, wa.width / 2 - 50, wa.height / 2 - 10, 100, 20, 1, 0, WhitePixel(display, screen));
	if (!lscreen.input_field) {
		XDestroyWindow(display, lscreen.window);
		return 0;
	}
	XSelectInput(display, lscreen.input_field, KeyPressMask);

	/* Set override redirect */
	XSetWindowAttributes swa;
	swa.override_redirect = True;
	XChangeWindowAttributes(lscreen.display, lscreen.window, CWOverrideRedirect, &swa);

	memset(lscreen.input_text, 0, sizeof(lscreen.input_text)-1);
	lscreen.input_len = 0;
	return 1;
}

/* Run the main loop */
void lscreen_show() {
	char mask[MAXPASS] = {0};

	/* Grab input */
	XGrabKeyboard(lscreen.display, RootWindow(lscreen.display, lscreen.screen), True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(lscreen.display, RootWindow(lscreen.display, lscreen.screen), True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

	/* Display the window */
	XMapRaised(lscreen.display, lscreen.window);
	XSetInputFocus(lscreen.display, lscreen.window, RevertToParent, CurrentTime);
	XSync(lscreen.display, False);

	hide_cursor(lscreen.display, lscreen.window);
	XFlush(lscreen.display);

	XEvent ev;
	int showing = 0;
	for (;;) {
		XNextEvent(lscreen.display, &ev);
		if (ev.type == Expose) {
			XClearWindow(lscreen.display, lscreen.input_field);
			XDrawString(lscreen.display, lscreen.input_field, lscreen.gc, 5, 12, mask, lscreen.input_len);
			XFlush(lscreen.display);
		} else if (ev.type == KeyPress) {
			KeySym key = XLookupKeysym(&ev.xkey, 0);
			if (key == XK_Escape) {
				if (!showing) {
					showing = 1;
					XMapRaised(lscreen.display, lscreen.input_field);
					XSetInputFocus(lscreen.display, lscreen.input_field, RevertToParent, CurrentTime);
					XSync(lscreen.display, False);
					memset(lscreen.input_text, 0, sizeof(lscreen.input_text));
					memset(mask, 0, sizeof(mask));
					lscreen.input_len = 0;
				} else {
					showing = 0;
					XUnmapWindow(lscreen.display, lscreen.input_field);
					XSetInputFocus(lscreen.display, lscreen.window, RevertToParent, CurrentTime);
					XSync(lscreen.display, False);
					memset(lscreen.input_text, 0, sizeof(lscreen.input_text));
					memset(mask, 0, sizeof(mask));
					lscreen.input_len = 0;
				}
			} else if (key == XK_Return) {
				/* Accept input field entry */
				if (showing) {
					unsigned char hash[SHA512_DIGEST_LENGTH] = {0};
					SHA512((unsigned char *)lscreen.input_text, lscreen.input_len, hash);
					if (!memcmp(lscreen.hash, hash, SHA512_DIGEST_LENGTH)) {
						break;
					} else {
						memset(lscreen.input_text, 0, sizeof(lscreen.input_text)-1);
						lscreen.input_len = 0;
					}
				}
			} else if (key == XK_BackSpace && lscreen.input_len > 0) {
				lscreen.input_text[--lscreen.input_len] = '\0';
			} else if (lscreen.input_len < (int)(sizeof(lscreen.input_text)-1) && key >= XK_space && key <= XK_asciitilde) {
				/* Handle input field */
				if (showing) {
					char buffer[10];
					XComposeStatus compose;
					int len = XLookupString(&ev.xkey, buffer, sizeof(buffer)-1, &key, &compose);
					buffer[len] = '\0';
					mask[lscreen.input_len] = '*';
					lscreen.input_text[lscreen.input_len++] = buffer[0];
					mask[lscreen.input_len] = '\0';
					lscreen.input_text[lscreen.input_len] = '\0';
				}
			}

			XClearWindow(lscreen.display, lscreen.input_field);
			XDrawString(lscreen.display, lscreen.input_field, lscreen.gc, 5, 12, mask, lscreen.input_len);
			XFlush(lscreen.display);
		}
	}

	/* Ungrab input before restoring focus */
	XUngrabKeyboard(lscreen.display, CurrentTime);
	XUngrabPointer(lscreen.display, CurrentTime);

	/* Set override redirect */
	XSetWindowAttributes swa;
	swa.override_redirect = False;
	XChangeWindowAttributes(lscreen.display, lscreen.window, CWOverrideRedirect, &swa);

	show_cursor(lscreen.display, lscreen.window);
	XFlush(lscreen.display);

	/* Restore hotkeys and focus */
	if (lscreen.prev_focused_win) {
		XSetInputFocus(lscreen.display, lscreen.prev_focused_win, lscreen.prev_revert_to, CurrentTime);
	} else {
		XSetInputFocus(lscreen.display, RootWindow(lscreen.display, lscreen.screen), PointerRoot, CurrentTime);
	}
	XFlush(lscreen.display);
}

/* Free the lock screen */
void lscreen_free() {
	XFreeGC(lscreen.display, lscreen.gc);
	XFreeFont(lscreen.display, lscreen.font);
	XDestroyWindow(lscreen.display, lscreen.input_field);
	XDestroyWindow(lscreen.display, lscreen.window);
}

