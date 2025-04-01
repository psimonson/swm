// swm - Reparenting stacking window manager.
// Coded by Philip R. Simonson.

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"
#include "lscreen.h"
#include "status.h"
#include "wm.h"

Display *dpy;
Window root;
int screen;
int running;

WindowEntry windows[MAX_WINDOWS];
int window_count = 0;
int focused_index = -1;  // Index of currently focused window

void setup_hotkeys(Window w) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask | ShiftMask, w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod4Mask, w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Return), Mod4Mask, w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask, w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_r), Mod4Mask, w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_l), Mod4Mask, w, True, GrabModeAsync, GrabModeAsync);
    XGrabButton(dpy, Button1, Mod1Mask, w, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeSync, GrabModeAsync, None, None);
}

int find_client(Window w) {
    if (window_count > 0) {
        for (int i = 0; i < window_count; i++) {
            if (windows[i].client == w) {
                return i;
            }
        }
    }
    return -1;
}

void add_client(Window w, Window f) {
    if (window_count < MAX_WINDOWS) {
        windows[window_count].frame = f;
        windows[window_count].client = w;
        focused_index = window_count;
        window_count++;
    }
}

void del_client(Window w) {
    int index = find_client(w);
    if (index != -1) {
        windows[index] = windows[window_count - 1];
        window_count--;  // Reduce count first
        if (window_count <= 0) {
            focused_index = -1;
        } else {
            focused_index = (focused_index >= window_count) ? window_count - 1 : focused_index - 1; // Prevent invalid index
        }
    }
}

void update_focus(int index) {
    if (window_count <= 0) {
        focused_index = -1;
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        setup_hotkeys(root);
        return;
    }
    if (index < 0 || index >= window_count) {
        index = 0;
    }
    focused_index = index;
    XRaiseWindow(dpy, windows[focused_index].frame);
    XRaiseWindow(dpy, windows[focused_index].client);
    XSetInputFocus(dpy, windows[focused_index].client, RevertToParent, CurrentTime);
}

void cycle_focus(int dir) {
    focused_index = (window_count > 0) ? (focused_index + dir) % window_count : -1;
    update_focus(focused_index);
}

void frame_window(Window w) {
    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, w, &wa)) {
        return;
    }

    Window f = XCreateSimpleWindow(dpy, root, wa.x, wa.y, wa.width, wa.height, BORDER_WIDTH, FRAME_COLOR, BG_COLOR);
    if (!f) {
        return;
    }

    XSelectInput(dpy, f, SubstructureRedirectMask | SubstructureNotifyMask);
    XReparentWindow(dpy, w, f, 0, 0);
    XMapWindow(dpy, f);
    XMapWindow(dpy, w);
    add_client(w, f);
    update_focus(focused_index);
    setup_hotkeys(w);
    XSync(dpy, False);
}

void unframe_window(Window w) {
    int index = find_client(w);
    if (index == -1) return;

    if (windows[index].frame != None && windows[index].client != None) {
        XReparentWindow(dpy, windows[index].client, root, 0, 0);
        XDestroyWindow(dpy, windows[index].frame);
        XSync(dpy, False);
        windows[index].frame = None;
    }
}

void on_destroy_notify(XEvent *ev) {
    if (ev->xdestroywindow.event == root) return;
    unframe_window(ev->xdestroywindow.window);
    del_client(ev->xdestroywindow.window);
    cycle_focus(-1);
}

void on_unmap_notify(XEvent *ev) {
    if (ev->xunmap.event == root) return;
    int index = find_client(ev->xunmap.window);
    if (index == -1) return;

    Window client = windows[index].client;
    if (client != None) {
        unframe_window(client);
        del_client(client);
    }
    cycle_focus(-1);
}

void on_map_request(XEvent *ev) {
    frame_window(ev->xmaprequest.window);
}

void on_configure_request(XEvent *ev) {
    int index = find_client(ev->xconfigurerequest.window);
    if (index == -1) return;
    XConfigureRequestEvent *e = &ev->xconfigurerequest;
    XWindowChanges changes;
    changes.x = e->x;
    changes.y = e->y;
    changes.width = e->width;
    changes.height = e->height;
    changes.border_width = e->border_width;
    changes.sibling = e->above;
    changes.stack_mode = e->detail;
    XConfigureWindow(dpy, windows[index].frame, e->value_mask, &changes);
    XMoveResizeWindow(dpy, windows[index].frame, e->x, e->y, e->width, e->height);
    XConfigureWindow(dpy, windows[index].client, e->value_mask, &changes);
    XMoveResizeWindow(dpy, windows[index].client, e->x, e->y, e->width, e->height);
}

void on_key_press(XEvent *ev) {
    XKeyEvent *xkey = &ev->xkey;
    KeySym key = XLookupKeysym(xkey, 0);

    if (key == XK_q && (xkey->state & (Mod4Mask | ShiftMask))) {
        running = 0;
    } else if (key == XK_Tab && (xkey->state & Mod4Mask)) {
        cycle_focus(1);
    } else if (key == XK_Return && (xkey->state & Mod4Mask)) {
        spawn("mate-terminal");
    } else if (key == XK_q && (xkey->state & Mod4Mask)) {
        if (focused_index != -1 && focused_index < window_count) {
            Window client = windows[focused_index].client;
            if (client != None) {
                unframe_window(client);
                XKillClient(dpy, client);
                del_client(client);
            }
        }
    } else if (key == XK_r && (xkey->state & Mod4Mask)) {
        spawn("dmenu_run");
    } else if (key == XK_l && (xkey->state & Mod4Mask)) {
        if (lscreen_init(dpy, screen)) {
            lscreen_show();
            lscreen_free();
        }
    }
}

void on_button_press(XEvent *ev) {
}

void on_button_release(XEvent *ev) {
}

void on_motion_notify(XEvent *ev) {
}

static int xerror(Display *d, XErrorEvent *e) {
    char err[1024] = {0};
    XGetErrorText(d, e->error_code, err, sizeof(err));
    fprintf(stderr, "X11 Error: %s\n", err);
    return 0;
}

int main() {
    XSetErrorHandler(xerror);
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        exit(EXIT_FAILURE);
    }

    screen = DefaultScreen(dpy);
    root = DefaultRootWindow(dpy);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(dpy, False);
    setup_hotkeys(root);

    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);
    XDefineCursor(dpy, root, cursor);
    XSync(dpy, False);

    status_init(dpy, screen);

    XEvent ev;
    running = 1;
    while (running) {
        XNextEvent(dpy, &ev);
        if (ev.type == DestroyNotify) on_destroy_notify(&ev);
        if (ev.type == UnmapNotify) on_unmap_notify(&ev);
        if (ev.type == MapRequest) on_map_request(&ev);
        if (ev.type == ConfigureRequest) on_configure_request(&ev);
        if (ev.type == KeyPress) on_key_press(&ev);
        if (ev.type == ButtonPress) on_button_press(&ev);
        if (ev.type == ButtonRelease) on_button_release(&ev);
        if (ev.type == MotionNotify) on_motion_notify(&ev);
    }

    status_free();
    XFreeCursor(dpy, cursor);
    XCloseDisplay(dpy);
    return 0;
}