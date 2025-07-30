#include "status.h"

static StatusBar status_bar;
static pthread_t status_thread;
static int status_running = 1;

/* Function to draw the status bar */
void draw_status_bar() {
	XLockDisplay(status_bar.display);

    int screen_width = XDisplayWidth(status_bar.display, status_bar.screen);
    int screen_height = XDisplayHeight(status_bar.display, status_bar.screen);
    int bar_y = screen_height - BAR_HEIGHT;

/*
    // Get window title
	if (focused && focused->win) {
		char *window_title;
	    XFetchName(status_bar.display, focused->win, &window_title);
		if (!window_title) {
			strncpy(status_bar.window_title, "No Title", 127);
		} else {
			strncpy(status_bar.window_title, window_title, 127);
			XFree(window_title);
		}
	} else {
		strncpy(status_bar.window_title, "", 127);
	}
*/

    // Get time
    char time_buffer[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    // Get text widths
    int title_width = XTextWidth(status_bar.font, status_bar.window_title, strlen(status_bar.window_title));
    int time_width = XTextWidth(status_bar.font, time_buffer, strlen(time_buffer));

    // Clear status bar
    XSetForeground(status_bar.display, status_bar.gc, BlackPixel(status_bar.display, status_bar.screen));
    XFillRectangle(status_bar.display, status_bar.root, status_bar.gc, 0, bar_y, screen_width, BAR_HEIGHT);

    // Draw title (centered)
    int title_x = (screen_width - title_width) / 2;
    int title_y = bar_y + (BAR_HEIGHT + status_bar.font->ascent) / 2;
    XSetForeground(status_bar.display, status_bar.gc, WhitePixel(status_bar.display, status_bar.screen));
    XDrawString(status_bar.display, status_bar.root, status_bar.gc, title_x, title_y, status_bar.window_title, strlen(status_bar.window_title));

    // Draw time (right-aligned)
    int time_x = screen_width - time_width - 10; // 10px padding from right
    XDrawString(status_bar.display, status_bar.root, status_bar.gc, time_x, title_y, time_buffer, strlen(time_buffer));

    // Flush changes
    XFlush(status_bar.display);
	XUnlockDisplay(status_bar.display);
}

/* Function to continuously show the status bar */
void *status_loop(void *p) {
	(void)p;

	while (status_running) {
	    draw_status_bar();
	    sleep(UPDATE_INTERVAL);
	}

	return NULL;
}

/* Function to initialize the status bar */
int status_init(Display *display, int screen) {
    // Open display
    status_bar.display = display;
    if (!status_bar.display) {
        fprintf(stderr, "No display given\n");
        return 1;
    }

    status_bar.screen = screen;
    status_bar.root = RootWindow(status_bar.display, status_bar.screen);
    status_bar.gc = XCreateGC(status_bar.display, status_bar.root, 0, NULL);

    // Load font
    status_bar.font = XLoadQueryFont(status_bar.display, "fixed");
    if (!status_bar.font) {
        fprintf(stderr, "Failed to load font\n");
		XFreeGC(status_bar.display, status_bar.gc);
		return 1;
    }
    XSetFont(status_bar.display, status_bar.gc, status_bar.font->fid);

	// Start status bar thread
    if (pthread_create(&status_thread, NULL, status_loop, NULL) != 0) {
        fprintf(stderr, "Failed to create status bar thread\n");
        return 1;
    }

	return 0;
}

/* Function to clean up the status bar resources */
void status_free() {
	status_running = 0;
	pthread_join(status_thread, NULL);

    if (status_bar.font) {
        XFreeFont(status_bar.display, status_bar.font);
    }
    if (status_bar.gc) {
        XFreeGC(status_bar.display, status_bar.gc);
    }
    if (status_bar.display) {
        XCloseDisplay(status_bar.display);
    }
}

