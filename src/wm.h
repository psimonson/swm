#ifndef WM_H
#define WM_H

#define BORDER_WIDTH 2
#define FRAME_COLOR  0xFF0000
#define FOCUS_COLOR  0x00FF00
#define BG_COLOR 0x000000
#define TITLE_HEIGHT 20
#define BUTTON_SIZE  16
#define MAX_WINDOWS 256

typedef struct {
    Window frame;
    Window client;
    Window close_button;
    GC frame_gc;
    XColor frame_color;
} WindowEntry;

#endif // WM_H
