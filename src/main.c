#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "util.h"
#include "lscreen.h"
#include "status.h"
#include "rundlg.h"
#include "main.h"

// Global variables
Display *dpy;
Window root;
int screen;
WindowNode *window_list = NULL;
WindowNode *current_window = NULL;
Window hidden_windows[MAX_HIDDEN];
Window minimized_windows[MAX_MINIMIZED];
int minimized_count = 0;
int hidden_count = 0;
int running = 1;

// EWMH atoms
Atom net_supported, net_client_list, net_active_window, net_wm_name;
Atom net_wm_state, net_wm_state_maximized_vert, net_wm_state_maximized_horz;
Atom net_wm_state_hidden, net_wm_desktop, net_current_desktop;
Atom wm_protocols, wm_delete_window;

// Function prototypes
void init_ewmh();
void update_client_list();
void update_active_window(Window win);
WindowNode* find_window(Window win);
WindowNode* add_window(Window win);
void remove_window(Window win);
void focus_window(WindowNode *node);
void next_window();
void close_window();
void minimize_window();
void maximize_window();
void hide_window();
void unhide_last_window();
void handle_keypress(XKeyEvent *e);
void handle_map_request(XMapRequestEvent *e);
void handle_unmap_notify(XUnmapEvent *e);
void handle_destroy_notify(XDestroyWindowEvent *e);
void handle_configure_request(XConfigureRequestEvent *e);
void cleanup();
void signal_handler(int sig);

// Initialize EWMH support
void init_ewmh() {
    // Create atoms
    net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    net_wm_state_maximized_vert = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    net_wm_state_maximized_horz = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    net_wm_state_hidden = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
    net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    // Set supported atoms
    Atom supported[] = {
        net_supported, net_client_list, net_active_window, net_wm_name,
        net_wm_state, net_wm_state_maximized_vert, net_wm_state_maximized_horz,
        net_wm_state_hidden, net_wm_desktop, net_current_desktop
    };
    
    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32,
                   PropModeReplace, (unsigned char*)supported,
                   sizeof(supported) / sizeof(Atom));

    // Set current desktop to 0
    long desktop = 0;
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char*)&desktop, 1);
}

// Update client list for EWMH compliance
void update_client_list() {
    Window *windows = malloc(MAX_WINDOWS * sizeof(Window));
    int count = 0;
    
    WindowNode *node = window_list;
    while (node && count < MAX_WINDOWS) {
        windows[count++] = node->window;
        node = node->next;
    }
    
    XChangeProperty(dpy, root, net_client_list, XA_WINDOW, 32,
                   PropModeReplace, (unsigned char*)windows, count);
    free(windows);
}

// Update active window for EWMH compliance
void update_active_window(Window win) {
    XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32,
                   PropModeReplace, (unsigned char*)&win, 1);
}

// Find window in linked list
WindowNode* find_window(Window win) {
    WindowNode *node = window_list;
    while (node) {
        if (node->window == win) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

// Add window to linked list
WindowNode* add_window(Window win) {
    WindowNode *node = malloc(sizeof(WindowNode));
    if (!node) return NULL;
    
    node->window = win;
    node->state = WIN_NORMAL;
    node->next = window_list;
    node->prev = NULL;
    
    if (window_list) {
        window_list->prev = node;
    }
    window_list = node;
    
    // Get window geometry
    XWindowAttributes attrs;
    XGetWindowAttributes(dpy, win, &attrs);
    node->x = attrs.x;
    node->y = attrs.y;
    node->width = attrs.width;
    node->height = attrs.height;
    
    // Set desktop property
    long desktop = 0;
    XChangeProperty(dpy, win, net_wm_desktop, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char*)&desktop, 1);
    
    update_client_list();
    return node;
}

// Remove window from linked list
void remove_window(Window win) {
    WindowNode *node = find_window(win);
    if (!node) return;
    
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        window_list = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    }
    
    if (current_window == node) {
        current_window = window_list;
    }
    
    free(node);
    update_client_list();
}

// Focus a window
void focus_window(WindowNode *node) {
    if (!node) return;
    
    // Check if window still exists
    XWindowAttributes attrs;
    if (XGetWindowAttributes(dpy, node->window, &attrs) == 0) {
        return; // Window no longer exists
    }
    
    current_window = node;
    XRaiseWindow(dpy, node->window);
    XSetInputFocus(dpy, node->window, RevertToPointerRoot, CurrentTime);
    update_active_window(node->window);
    
    // Set window border to indicate focus
    XSetWindowBorder(dpy, node->window, WhitePixel(dpy, screen));
    
    // Remove focus from other windows
    WindowNode *n = window_list;
    while (n) {
        if (n != node && XGetWindowAttributes(dpy, n->window, &attrs)) {
            XSetWindowBorder(dpy, n->window, BlackPixel(dpy, screen));
        }
        n = n->next;
    }
}

// Alt+Tab functionality
void next_window() {
    if (!current_window || !window_list) {
        // If no current window, focus first available window
        WindowNode *node = window_list;
        while (node && (node->state == WIN_HIDDEN || node->state == WIN_MINIMIZED)) {
            node = node->next;
        }
        if (node) {
            focus_window(node);
        }
        return;
    }
    
    WindowNode *next = current_window->next;
    if (!next) {
        next = window_list;
    }
    
    // Skip hidden/minimized windows
    WindowNode *start = next;
    while (next && (next->state == WIN_HIDDEN || next->state == WIN_MINIMIZED)) {
        next = next->next;
        if (!next) next = window_list;
        if (next == start) break; // Avoid infinite loop
    }
    
    if (next && next != current_window && 
        next->state != WIN_HIDDEN && next->state != WIN_MINIMIZED) {
        focus_window(next);
    }
}

// Close current window
void close_window() {
    if (!current_window) return;
    
    // Try to close gracefully first
    Atom *protocols;
    int n;
    if (XGetWMProtocols(dpy, current_window->window, &protocols, &n)) {
        for (int i = 0; i < n; i++) {
            if (protocols[i] == wm_delete_window) {
                XEvent e;
                e.type = ClientMessage;
                e.xclient.window = current_window->window;
                e.xclient.message_type = wm_protocols;
                e.xclient.format = 32;
                e.xclient.data.l[0] = wm_delete_window;
                e.xclient.data.l[1] = CurrentTime;
                XSendEvent(dpy, current_window->window, False, NoEventMask, &e);
                XFree(protocols);
                return;
            }
        }
        XFree(protocols);
    }
    
    // Force kill if graceful close failed
    XKillClient(dpy, current_window->window);
}

// Minimize current window
void minimize_window() {
    if (!current_window || current_window->state == WIN_MINIMIZED || minimized_count >= MAX_MINIMIZED) return;
    
    // Add to minimized stack
    if (minimized_count < MAX_MINIMIZED) {
        minimized_windows[minimized_count++] = current_window->window;
    }
    
    current_window->state = WIN_MINIMIZED;
    XUnmapWindow(dpy, current_window->window);
    
    // Set EWMH state
    Atom state = net_wm_state_hidden;
    XChangeProperty(dpy, current_window->window, net_wm_state, XA_ATOM, 32,
                   PropModeReplace, (unsigned char*)&state, 1);
    
    // Focus next available window
    WindowNode *minimized = current_window;
    current_window = NULL;
    
    WindowNode *next = minimized->next;
    if (!next) next = window_list;
    
    while (next && (next->state == WIN_HIDDEN || next->state == WIN_MINIMIZED)) {
        next = next->next;
        if (!next) next = window_list;
        if (next == minimized) break;
    }
    
    if (next && next != minimized && 
        next->state != WIN_HIDDEN && next->state != WIN_MINIMIZED) {
        focus_window(next);
    } else {
        update_active_window(None);
    }
}

// Restore minimized window
void restore_window() {
    if (minimized_count == 0) return;
    
    Window win = minimized_windows[--minimized_count];
    WindowNode *node = find_window(win);
    
    if (node && node->state == WIN_MINIMIZED) {
        node->state = WIN_NORMAL;
        XMapWindow(dpy, node->window);
        
        // Remove EWMH state
        XDeleteProperty(dpy, node->window, net_wm_state);
        
        focus_window(node);
    }
}

// Maximize current window
void maximize_window() {
    if (!current_window) return;
    
    if (current_window->state == WIN_MAXIMIZED) {
        // Restore
        current_window->state = WIN_NORMAL;
        XMoveResizeWindow(dpy, current_window->window,
                         current_window->x, current_window->y,
                         current_window->width, current_window->height);
        
        // Remove EWMH state
        XDeleteProperty(dpy, current_window->window, net_wm_state);
    } else {
        // Save current geometry
        XWindowAttributes attrs;
        XGetWindowAttributes(dpy, current_window->window, &attrs);
        current_window->x = attrs.x;
        current_window->y = attrs.y;
        current_window->width = attrs.width;
        current_window->height = attrs.height;
        
        // Maximize
        current_window->state = WIN_MAXIMIZED;
        int screen_width = DisplayWidth(dpy, screen);
        int screen_height = DisplayHeight(dpy, screen);
        XMoveResizeWindow(dpy, current_window->window, 0, 0,
                         screen_width, screen_height);
        
        // Set EWMH state
        Atom states[] = {net_wm_state_maximized_vert, net_wm_state_maximized_horz};
        XChangeProperty(dpy, current_window->window, net_wm_state, XA_ATOM, 32,
                       PropModeReplace, (unsigned char*)states, 2);
    }
}

// Hide current window
void hide_window() {
    if (!current_window || current_window->state == WIN_HIDDEN || hidden_count >= MAX_HIDDEN) return;
    
    // Add to hidden stack
    if (hidden_count < MAX_HIDDEN) {
        hidden_windows[hidden_count++] = current_window->window;
    }
    
    current_window->state = WIN_HIDDEN;
    XUnmapWindow(dpy, current_window->window);
    
    // Set EWMH state
    Atom state = net_wm_state_hidden;
    XChangeProperty(dpy, current_window->window, net_wm_state, XA_ATOM, 32,
                   PropModeReplace, (unsigned char*)&state, 1);
    
    // Focus next available window
    WindowNode *hidden = current_window;
    current_window = NULL;
    
    WindowNode *next = hidden->next;
    if (!next) next = window_list;
    
    while (next && (next->state == WIN_HIDDEN || next->state == WIN_MINIMIZED)) {
        next = next->next;
        if (!next) next = window_list;
        if (next == hidden) break;
    }
    
    if (next && next != hidden && 
        next->state != WIN_HIDDEN && next->state != WIN_MINIMIZED) {
        focus_window(next);
    } else {
        update_active_window(None);
    }
}

// Unhide last hidden window (LIFO)
void unhide_last_window() {
    if (hidden_count == 0) return;
    
    Window win = hidden_windows[--hidden_count];
    WindowNode *node = find_window(win);
    
    if (node && node->state == WIN_HIDDEN) {
        node->state = WIN_NORMAL;
        XMapWindow(dpy, node->window);
        
        // Remove EWMH state
        XDeleteProperty(dpy, node->window, net_wm_state);
        
        focus_window(node);
    }
}

// Handle key press events
void handle_keypress(XKeyEvent *e) {
    KeySym key = XLookupKeysym(e, 0);
    
    // Alt+Tab
    if ((e->state & Mod1Mask) && key == XK_Tab) {
        next_window();
    }
    // Alt+F4 (close window)
    else if ((e->state & Mod1Mask) && key == XK_F4) {
        close_window();
    }
    // Super+Q (quit window manager)
    else if ((e->state & Mod4Mask) && key == XK_q) {
        running = 0;
    }
    // Super+D (run dialog)
    else if ((e->state & Mod4Mask) && key == XK_d) {
        if (rundlg_init(dpy, screen)) {
            rundlg_show();
            rundlg_free();
        }
    }
    // Super+N (minimize)
    else if ((e->state & Mod4Mask) && key == XK_n) {
        minimize_window();
    }
    // Super+L (lock screen)
    else if ((e->state & Mod4Mask) && key == XK_l) {
        if (lscreen_init(dpy, screen)) {
            lscreen_show();
            lscreen_free();
        }
    }
    // Super+M (maximize)
    else if ((e->state & Mod4Mask) && key == XK_m) {
        maximize_window();
    }
    // Super+R (restore)
    else if ((e->state & Mod4Mask) && key == XK_r) {
        restore_window();
    }
    // Super+X (hide)
    else if ((e->state & Mod4Mask) && key == XK_x) {
        hide_window();
    }
    // Super+Z (unhide last)
    else if ((e->state & Mod4Mask) && key == XK_z) {
        unhide_last_window();
    }
}

// Handle map request
void handle_map_request(XMapRequestEvent *e) {
    WindowNode *node = find_window(e->window);
    if (!node) {
        node = add_window(e->window);
    }
    
    if (node) {
        XMapWindow(dpy, e->window);
        focus_window(node);
        
        // Add window border
        XSetWindowBorderWidth(dpy, e->window, 2);
        XSetWindowBorder(dpy, e->window, WhitePixel(dpy, screen));
    }
}

// Handle unmap notify
void handle_unmap_notify(XUnmapEvent *e) {
    WindowNode *node = find_window(e->window);
    if (node && node->state != WIN_MINIMIZED && node->state != WIN_HIDDEN) {
        // Remove from hidden windows array if present
        for (int i = 0; i < hidden_count; i++) {
            if (hidden_windows[i] == e->window) {
                // Shift array elements
                for (int j = i; j < hidden_count - 1; j++) {
                    hidden_windows[j] = hidden_windows[j + 1];
                }
                hidden_count--;
                break;
            }
        }
        
        remove_window(e->window);
        
        // Focus next window if this was current
        if (current_window == node) {
            current_window = NULL;
            if (window_list) {
                // Find first visible window
                WindowNode *next = window_list;
                while (next && (next->state == WIN_HIDDEN || next->state == WIN_MINIMIZED)) {
                    next = next->next;
                }
                if (next) {
                    focus_window(next);
                } else {
                    update_active_window(None);
                }
            } else {
                update_active_window(None);
            }
        }
    }
}

// Handle destroy notify
void handle_destroy_notify(XDestroyWindowEvent *e) {
    WindowNode *node = find_window(e->window);
    if (node) {
        // Remove from hidden windows array if present
        for (int i = 0; i < hidden_count; i++) {
            if (hidden_windows[i] == e->window) {
                // Shift array elements
                for (int j = i; j < hidden_count - 1; j++) {
                    hidden_windows[j] = hidden_windows[j + 1];
                }
                hidden_count--;
                break;
            }
        }
        
        // Update current_window if this was it
        if (current_window == node) {
            current_window = NULL;
        }
        
        remove_window(e->window);
        
        // Focus next available window
        if (window_list) {
            WindowNode *next = window_list;
            while (next && (next->state == WIN_HIDDEN || next->state == WIN_MINIMIZED)) {
                next = next->next;
            }
            if (next) {
                focus_window(next);
            } else {
                update_active_window(None);
            }
        } else {
            update_active_window(None);
        }
    }
}

// Handle configure request
void handle_configure_request(XConfigureRequestEvent *e) {
    XWindowChanges changes;
    changes.x = e->x;
    changes.y = e->y;
    changes.width = e->width;
    changes.height = e->height;
    changes.border_width = e->border_width;
    changes.sibling = e->above;
    changes.stack_mode = e->detail;
    
    XConfigureWindow(dpy, e->window, e->value_mask, &changes);
}

// Cleanup function
void cleanup() {
    WindowNode *node = window_list;
    while (node) {
        WindowNode *next = node->next;
        free(node);
        node = next;
    }
    
    status_free();

    if (dpy) {
        XCloseDisplay(dpy);
    }
}

// Signal handler
void signal_handler(int sig) {
    cleanup();
    exit(0);
}

// Xlib error handler
int xerror(Display *dpy, XErrorEvent *e) {
    char error_text[256];
    XGetErrorText(dpy, e->error_code, error_text, sizeof(error_text));
    fprintf(stderr, "X Error: %s (code %d)\n", error_text, e->error_code);
    return 0; // Continue execution
}

int main() {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Open display
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    
    // Initialize EWMH
    init_ewmh();
    
    // Select events on root window
    XSelectInput(dpy, root, 
                SubstructureRedirectMask | SubstructureNotifyMask |
                KeyPressMask | KeyReleaseMask);
    
    // Set error handler to catch X errors gracefully
    XSetErrorHandler(xerror);
    
    // Grab key combinations
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod1Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F4), Mod1Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_d), Mod4Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_n), Mod4Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_l), Mod4Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_m), Mod4Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_r), Mod4Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_x), Mod4Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_z), Mod4Mask, root, True,
             GrabModeAsync, GrabModeAsync);
    
    printf("Stacking Window Manager started\n");
    printf("Shortcuts:\n");
    printf("  Alt+Tab: Switch windows\n");
    printf("  Alt+F4: Close window\n");
    printf("  Super+Q: Quit window manager\n");
    printf("  Super+D: Run dialog\n");
    printf("  Super+N: Minimize window\n");
    printf("  Super+L: Lock screen\n");
    printf("  Super+M: Maximize/restore window\n");
    printf("  Super+R: Restore minimized window\n");
    printf("  Super+X: Hide window\n");
    printf("  Super+Z: Unhide last hidden window\n");
 
    if (status_init(dpy, screen)) {
        printf("Cannot initialize status bar.\n");
        cleanup();
        return 1;
    }

    // Main event loop
    XEvent e;
    while (running) {
        XNextEvent(dpy, &e);
        
        switch (e.type) {
            case KeyPress:
                handle_keypress(&e.xkey);
                break;
            case MapRequest:
                handle_map_request(&e.xmaprequest);
                break;
            case UnmapNotify:
                handle_unmap_notify(&e.xunmap);
                break;
            case DestroyNotify:
                handle_destroy_notify(&e.xdestroywindow);
                break;
            case ConfigureRequest:
                handle_configure_request(&e.xconfigurerequest);
                break;
        }
    }
    
    cleanup();
    return 0;
}
