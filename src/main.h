#ifndef MAIN_H
#define MAIN_H

#define MAX_WINDOWS 256
#define MAX_MINIMIZED 64
#define MAX_HIDDEN 64

// Window states
typedef enum {
    WIN_NORMAL,
    WIN_MINIMIZED,
    WIN_MAXIMIZED,
    WIN_HIDDEN
} WindowState;

// Window node structure
typedef struct WindowNode {
    Window window;
    WindowState state;
    int x, y, width, height;  // Original dimensions for restore
    struct WindowNode *next;
    struct WindowNode *prev;
} WindowNode;

extern WindowNode *current_window;

#endif // MAIN_H
