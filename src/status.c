#include <X11/Xlib.h>
#include "main.h"
#include "status.h"
#include <pthread.h>

static StatusBar status_bar;
static pthread_t status_thread;
static volatile int status_running = 1;  // Made volatile for proper visibility
static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t focused_mutex = PTHREAD_MUTEX_INITIALIZER;  // Protect focused access
static WindowNode *focused = NULL; // For displaying title

/* Function to safely get window title */
static void get_window_title_safe(char *buffer, size_t buffer_size) {
    pthread_mutex_lock(&focused_mutex);

    focused = current_window;
    if (focused && focused->window) {
        char *window_title;
        if (XFetchName(status_bar.display, focused->window, &window_title) == Success && window_title) {
            strncpy(buffer, window_title, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';  // Ensure null termination
            XFree(window_title);
        } else {
            buffer[0] = '\0';
        }
    } else {
        buffer[0] = '\0';
    }
    
    pthread_mutex_unlock(&focused_mutex);
}

/* Function to draw the status bar */
void draw_status_bar() {
    char window_title_local[128];  // Local copy to avoid race conditions
    
    // Get window title safely
    get_window_title_safe(window_title_local, sizeof(window_title_local));
    
    // X11 operations with display locking
    XLockDisplay(status_bar.display);
    
    int screen_width = XDisplayWidth(status_bar.display, status_bar.screen);
    int screen_height = XDisplayHeight(status_bar.display, status_bar.screen);
    int bar_y = screen_height - BAR_HEIGHT;
    
    // Get time
    char time_buffer[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Get text widths
    int title_width = XTextWidth(status_bar.font, window_title_local, strlen(window_title_local));
    int time_width = XTextWidth(status_bar.font, time_buffer, strlen(time_buffer));
    
    // Clear status bar
    XSetForeground(status_bar.display, status_bar.gc, BlackPixel(status_bar.display, status_bar.screen));
    XFillRectangle(status_bar.display, status_bar.root, status_bar.gc, 0, bar_y, screen_width, BAR_HEIGHT);
    
    // Draw title (centered)
    int title_x = (screen_width - title_width) / 2;
    int title_y = bar_y + (BAR_HEIGHT + status_bar.font->ascent) / 2;
    XSetForeground(status_bar.display, status_bar.gc, WhitePixel(status_bar.display, status_bar.screen));
    XDrawString(status_bar.display, status_bar.root, status_bar.gc, title_x, title_y, 
                window_title_local, strlen(window_title_local));
    
    // Draw time (right-aligned)
    int time_x = screen_width - time_width - 10; // 10px padding from right
    XDrawString(status_bar.display, status_bar.root, status_bar.gc, time_x, title_y, 
                time_buffer, strlen(time_buffer));
    
    // Flush changes
    XFlush(status_bar.display);
    XUnlockDisplay(status_bar.display);
}

/* Function to continuously show the status bar */
void *status_loop(void *p) {
    (void)p;
    
    while (status_running) {  // volatile ensures proper visibility
        draw_status_bar();
        
        // Use nanosleep instead of sleep for better interruption handling
        struct timespec sleep_time = {UPDATE_INTERVAL, 0};
        nanosleep(&sleep_time, NULL);
        
        // Check status_running more frequently if UPDATE_INTERVAL is large
        if (UPDATE_INTERVAL > 1) {
            for (int i = 0; i < UPDATE_INTERVAL && status_running; i++) {
                sleep(1);
            }
        }
    }
    
    return NULL;
}

/* Function to initialize the status bar */
int status_init(Display *display, int screen) {
    pthread_mutex_lock(&status_mutex);
    
    // Open display
    status_bar.display = display;
    if (!status_bar.display) {
        fprintf(stderr, "No display given\n");
        pthread_mutex_unlock(&status_mutex);
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
        pthread_mutex_unlock(&status_mutex);
        return 1;
    }
    
    XSetFont(status_bar.display, status_bar.gc, status_bar.font->fid);
    
    // Reset status_running in case of restart
    status_running = 1;
    
    // Start status bar thread
    if (pthread_create(&status_thread, NULL, status_loop, NULL) != 0) {
        fprintf(stderr, "Failed to create status bar thread\n");
        XFreeFont(status_bar.display, status_bar.font);
        XFreeGC(status_bar.display, status_bar.gc);
        pthread_mutex_unlock(&status_mutex);
        return 1;
    }
    
    pthread_mutex_unlock(&status_mutex);
    return 0;
}

/* Function to clean up the status bar resources */
void status_free() {
    pthread_mutex_lock(&status_mutex);
    
    // Signal thread to stop
    status_running = 0;
    
    pthread_mutex_unlock(&status_mutex);
    
    // Wait for thread to finish
    pthread_join(status_thread, NULL);
    // Remove the invalid pthread_wait() call
    
    pthread_mutex_lock(&status_mutex);
    
    if (status_bar.font) {
        XFreeFont(status_bar.display, status_bar.font);
        status_bar.font = NULL;
    }
    
    if (status_bar.gc) {
        XFreeGC(status_bar.display, status_bar.gc);
        status_bar.gc = NULL;
    }
    
    // Note: Don't close display if it's shared with main thread
    // Only close if you own the display connection
    // if (status_bar.display) {
    //     XCloseDisplay(status_bar.display);
    //     status_bar.display = NULL;
    // }
    
    pthread_mutex_unlock(&status_mutex);
}
