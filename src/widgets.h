#ifndef XLIB_WIDGETS_H
#define XLIB_WIDGETS_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Widget types
typedef enum {
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_TEXTBOX
} WidgetType;

// Forward declarations
typedef struct Widget Widget;
typedef struct WidgetManager WidgetManager;

// Callback function type
typedef void (*WidgetCallback)(Widget* widget, void* user_data);

// Widget structure
struct Widget {
    WidgetType type;
    Window window;
    int x, y, width, height;
    char* text;
    int text_len;
    int max_text_len;
    int cursor_pos;
    int focused;
    int pressed;
    WidgetCallback callback;
    void* user_data;
    Widget* next;
};

// Widget manager structure
struct WidgetManager {
    Display* display;
    int screen;
    Window root;
    GC gc;
    XFontStruct* font;
    Widget* widgets;
    Widget* focused_widget;
    unsigned long bg_color;
    unsigned long fg_color;
    unsigned long focus_color;
    unsigned long press_color;
};

// Widget manager functions
WidgetManager* widget_manager_create(Display* display);
void widget_manager_destroy(WidgetManager* wm);
void widget_manager_handle_event(WidgetManager* wm, XEvent* event);
void widget_manager_draw_all(WidgetManager* wm);

// Widget creation functions
Widget* widget_create_button(WidgetManager* wm, Window parent, int x, int y, 
                           int width, int height, const char* text, 
                           WidgetCallback callback, void* user_data);
Widget* widget_create_label(WidgetManager* wm, Window parent, int x, int y, 
                          int width, int height, const char* text);
Widget* widget_create_textbox(WidgetManager* wm, Window parent, int x, int y, 
                            int width, int height, int max_len);

// Widget utility functions
void widget_destroy(WidgetManager* wm, Widget* widget);
void widget_set_text(Widget* widget, const char* text);
const char* widget_get_text(Widget* widget);
void widget_set_focus(WidgetManager* wm, Widget* widget);
void widget_draw(WidgetManager* wm, Widget* widget);

// Internal helper functions
void widget_add_to_manager(WidgetManager* wm, Widget* widget);
Widget* widget_find_by_window(WidgetManager* wm, Window window);
void widget_textbox_insert_char(Widget* widget, char c);
void widget_textbox_delete_char(Widget* widget);
void widget_textbox_move_cursor(Widget* widget, int direction);

#endif // XLIB_WIDGETS_H