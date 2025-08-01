#include "xlib_widgets.h"
#include <ctype.h>

// Color definitions
#define COLOR_WHITE 0xFFFFFF
#define COLOR_BLACK 0x000000
#define COLOR_LIGHT_GRAY 0xE0E0E0
#define COLOR_DARK_GRAY 0x808080
#define COLOR_BLUE 0x0080FF

// Widget manager implementation
WidgetManager* widget_manager_create(Display* display) {
    WidgetManager* wm = malloc(sizeof(WidgetManager));
    if (!wm) return NULL;
    
    wm->display = display;
    wm->screen = DefaultScreen(display);
    wm->root = RootWindow(display, wm->screen);
    wm->widgets = NULL;
    wm->focused_widget = NULL;
    
    // Create graphics context
    wm->gc = XCreateGC(display, wm->root, 0, NULL);
    
    // Load font
    wm->font = XLoadQueryFont(display, "fixed");
    if (!wm->font) {
        wm->font = XLoadQueryFont(display, "*");
    }
    XSetFont(display, wm->gc, wm->font->fid);
    
    // Set up colors
    Colormap colormap = DefaultColormap(display, wm->screen);
    XColor color;
    
    XParseColor(display, colormap, "#FFFFFF", &color);
    XAllocColor(display, colormap, &color);
    wm->bg_color = color.pixel;
    
    XParseColor(display, colormap, "#000000", &color);
    XAllocColor(display, colormap, &color);
    wm->fg_color = color.pixel;
    
    XParseColor(display, colormap, "#E0E0E0", &color);
    XAllocColor(display, colormap, &color);
    wm->focus_color = color.pixel;
    
    XParseColor(display, colormap, "#C0C0C0", &color);
    XAllocColor(display, colormap, &color);
    wm->press_color = color.pixel;
    
    return wm;
}

void widget_manager_destroy(WidgetManager* wm) {
    if (!wm) return;
    
    Widget* widget = wm->widgets;
    while (widget) {
        Widget* next = widget->next;
        widget_destroy(wm, widget);
        widget = next;
    }
    
    if (wm->font) XFreeFont(wm->display, wm->font);
    XFreeGC(wm->display, wm->gc);
    free(wm);
}

void widget_add_to_manager(WidgetManager* wm, Widget* widget) {
    widget->next = wm->widgets;
    wm->widgets = widget;
}

Widget* widget_find_by_window(WidgetManager* wm, Window window) {
    Widget* widget = wm->widgets;
    while (widget) {
        if (widget->window == window) return widget;
        widget = widget->next;
    }
    return NULL;
}

// Button creation
Widget* widget_create_button(WidgetManager* wm, Window parent, int x, int y, 
                           int width, int height, const char* text, 
                           WidgetCallback callback, void* user_data) {
    Widget* widget = malloc(sizeof(Widget));
    if (!widget) return NULL;
    
    widget->type = WIDGET_BUTTON;
    widget->x = x;
    widget->y = y;
    widget->width = width;
    widget->height = height;
    widget->text = strdup(text);
    widget->text_len = strlen(text);
    widget->max_text_len = 0;
    widget->cursor_pos = 0;
    widget->focused = 0;
    widget->pressed = 0;
    widget->callback = callback;
    widget->user_data = user_data;
    widget->next = NULL;
    
    widget->window = XCreateSimpleWindow(wm->display, parent, x, y, width, height,
                                       1, wm->fg_color, wm->bg_color);
    
    XSelectInput(wm->display, widget->window, 
                ExposureMask | ButtonPressMask | ButtonReleaseMask | 
                EnterWindowMask | LeaveWindowMask);
    
    XMapWindow(wm->display, widget->window);
    widget_add_to_manager(wm, widget);
    
    return widget;
}

// Label creation
Widget* widget_create_label(WidgetManager* wm, Window parent, int x, int y, 
                          int width, int height, const char* text) {
    Widget* widget = malloc(sizeof(Widget));
    if (!widget) return NULL;
    
    widget->type = WIDGET_LABEL;
    widget->x = x;
    widget->y = y;
    widget->width = width;
    widget->height = height;
    widget->text = strdup(text);
    widget->text_len = strlen(text);
    widget->max_text_len = 0;
    widget->cursor_pos = 0;
    widget->focused = 0;
    widget->pressed = 0;
    widget->callback = NULL;
    widget->user_data = NULL;
    widget->next = NULL;
    
    widget->window = XCreateSimpleWindow(wm->display, parent, x, y, width, height,
                                       1, wm->fg_color, wm->bg_color);
    
    XSelectInput(wm->display, widget->window, ExposureMask);
    XMapWindow(wm->display, widget->window);
    widget_add_to_manager(wm, widget);
    
    return widget;
}

// Textbox creation
Widget* widget_create_textbox(WidgetManager* wm, Window parent, int x, int y, 
                            int width, int height, int max_len) {
    Widget* widget = malloc(sizeof(Widget));
    if (!widget) return NULL;
    
    widget->type = WIDGET_TEXTBOX;
    widget->x = x;
    widget->y = y;
    widget->width = width;
    widget->height = height;
    widget->text = malloc(max_len + 1);
    widget->text[0] = '\0';
    widget->text_len = 0;
    widget->max_text_len = max_len;
    widget->cursor_pos = 0;
    widget->focused = 0;
    widget->pressed = 0;
    widget->callback = NULL;
    widget->user_data = NULL;
    widget->next = NULL;
    
    widget->window = XCreateSimpleWindow(wm->display, parent, x, y, width, height,
                                       2, wm->fg_color, wm->bg_color);
    
    XSelectInput(wm->display, widget->window, 
                ExposureMask | ButtonPressMask | KeyPressMask | FocusChangeMask);
    
    XMapWindow(wm->display, widget->window);
    widget_add_to_manager(wm, widget);
    
    return widget;
}

// Widget destruction
void widget_destroy(WidgetManager* wm, Widget* widget) {
    if (!widget) return;
    
    // Remove from linked list
    if (wm->widgets == widget) {
        wm->widgets = widget->next;
    } else {
        Widget* current = wm->widgets;
        while (current && current->next != widget) {
            current = current->next;
        }
        if (current) {
            current->next = widget->next;
        }
    }
    
    if (wm->focused_widget == widget) {
        wm->focused_widget = NULL;
    }
    
    XDestroyWindow(wm->display, widget->window);
    free(widget->text);
    free(widget);
}

// Text operations
void widget_set_text(Widget* widget, const char* text) {
    if (!widget || !text) return;
    
    if (widget->type == WIDGET_TEXTBOX) {
        int len = strlen(text);
        if (len > widget->max_text_len) {
            len = widget->max_text_len;
        }
        strncpy(widget->text, text, len);
        widget->text[len] = '\0';
        widget->text_len = len;
        widget->cursor_pos = len;
    } else {
        free(widget->text);
        widget->text = strdup(text);
        widget->text_len = strlen(text);
    }
}

const char* widget_get_text(Widget* widget) {
    return widget ? widget->text : NULL;
}

// Focus management
void widget_set_focus(WidgetManager* wm, Widget* widget) {
    if (wm->focused_widget) {
        wm->focused_widget->focused = 0;
        widget_draw(wm, wm->focused_widget);
    }
    
    wm->focused_widget = widget;
    if (widget && widget->type == WIDGET_TEXTBOX) {
        widget->focused = 1;
        XSetInputFocus(wm->display, widget->window, RevertToParent, CurrentTime);
        widget_draw(wm, widget);
    } else {
        wm->focused_widget = NULL;
    }
}

// Textbox character operations
void widget_textbox_insert_char(Widget* widget, char c) {
    if (!widget || widget->type != WIDGET_TEXTBOX) return;
    if (widget->text_len >= widget->max_text_len) return;
    if (!isprint(c)) return;
    
    // Shift characters to the right
    for (int i = widget->text_len; i >= widget->cursor_pos; i--) {
        widget->text[i + 1] = widget->text[i];
    }
    
    widget->text[widget->cursor_pos] = c;
    widget->cursor_pos++;
    widget->text_len++;
}

void widget_textbox_delete_char(Widget* widget) {
    if (!widget || widget->type != WIDGET_TEXTBOX) return;
    if (widget->cursor_pos == 0) return;
    
    // Shift characters to the left
    for (int i = widget->cursor_pos - 1; i < widget->text_len; i++) {
        widget->text[i] = widget->text[i + 1];
    }
    
    widget->cursor_pos--;
    widget->text_len--;
}

void widget_textbox_move_cursor(Widget* widget, int direction) {
    if (!widget || widget->type != WIDGET_TEXTBOX) return;
    
    if (direction < 0 && widget->cursor_pos > 0) {
        widget->cursor_pos--;
    } else if (direction > 0 && widget->cursor_pos < widget->text_len) {
        widget->cursor_pos++;
    }
}

// Drawing functions
void widget_draw(WidgetManager* wm, Widget* widget) {
    if (!widget) return;
    
    unsigned long bg_color = wm->bg_color;
    
    // Determine background color based on state
    if (widget->type == WIDGET_BUTTON && widget->pressed) {
        bg_color = wm->press_color;
    } else if (widget->type == WIDGET_TEXTBOX && widget->focused) {
        bg_color = wm->focus_color;
    }
    
    // Clear the window with background color
    XSetForeground(wm->display, wm->gc, bg_color);
    XFillRectangle(wm->display, widget->window, wm->gc, 0, 0, 
                   widget->width, widget->height);
    
    // Draw border for textbox
    if (widget->type == WIDGET_TEXTBOX) {
        XSetForeground(wm->display, wm->gc, wm->fg_color);
        XDrawRectangle(wm->display, widget->window, wm->gc, 0, 0, 
                      widget->width - 1, widget->height - 1);
        if (widget->focused) {
            XDrawRectangle(wm->display, widget->window, wm->gc, 1, 1, 
                          widget->width - 3, widget->height - 3);
        }
    }
    
    // Draw text
    if (widget->text && widget->text_len > 0) {
        XSetForeground(wm->display, wm->gc, wm->fg_color);
        
        int text_x = 5;
        int text_y = (widget->height + wm->font->ascent - wm->font->descent) / 2;
        
        if (widget->type == WIDGET_BUTTON) {
            // Center text in button
            int text_width = XTextWidth(wm->font, widget->text, widget->text_len);
            text_x = (widget->width - text_width) / 2;
        }
        
        XDrawString(wm->display, widget->window, wm->gc, text_x, text_y,
                   widget->text, widget->text_len);
    }
    
    // Draw cursor for focused textbox
    if (widget->type == WIDGET_TEXTBOX && widget->focused) {
        XSetForeground(wm->display, wm->gc, wm->fg_color);
        
        int cursor_x = 5;
        if (widget->cursor_pos > 0) {
            cursor_x += XTextWidth(wm->font, widget->text, widget->cursor_pos);
        }
        
        int cursor_y1 = 3;
        int cursor_y2 = widget->height - 3;
        
        XDrawLine(wm->display, widget->window, wm->gc, 
                 cursor_x, cursor_y1, cursor_x, cursor_y2);
    }
}

void widget_manager_draw_all(WidgetManager* wm) {
    Widget* widget = wm->widgets;
    while (widget) {
        widget_draw(wm, widget);
        widget = widget->next;
    }
}

// Event handling
void widget_manager_handle_event(WidgetManager* wm, XEvent* event) {
    Widget* widget = widget_find_by_window(wm, event->xany.window);
    if (!widget) return;
    
    switch (event->type) {
        case Expose:
            if (event->xexpose.count == 0) {
                widget_draw(wm, widget);
            }
            break;
            
        case ButtonPress:
            if (event->xbutton.button == Button1) {
                if (widget->type == WIDGET_BUTTON) {
                    widget->pressed = 1;
                    widget_draw(wm, widget);
                } else if (widget->type == WIDGET_TEXTBOX) {
                    widget_set_focus(wm, widget);
                    
                    // Calculate cursor position from click
                    int click_x = event->xbutton.x - 5;
                    int pos = 0;
                    
                    for (int i = 0; i <= widget->text_len; i++) {
                        int text_width = XTextWidth(wm->font, widget->text, i);
                        if (click_x <= text_width) {
                            pos = i;
                            break;
                        }
                        pos = i;
                    }
                    
                    widget->cursor_pos = pos;
                    widget_draw(wm, widget);
                }
            }
            break;
            
        case ButtonRelease:
            if (event->xbutton.button == Button1 && widget->type == WIDGET_BUTTON) {
                widget->pressed = 0;
                widget_draw(wm, widget);
                
                if (widget->callback) {
                    widget->callback(widget, widget->user_data);
                }
            }
            break;
            
        case KeyPress:
            if (widget->type == WIDGET_TEXTBOX && widget->focused) {
                KeySym keysym;
                char buffer[32];
                int len = XLookupString(&event->xkey, buffer, sizeof(buffer), 
                                      &keysym, NULL);
                
                switch (keysym) {
                    case XK_BackSpace:
                        widget_textbox_delete_char(widget);
                        break;
                    case XK_Left:
                        widget_textbox_move_cursor(widget, -1);
                        break;
                    case XK_Right:
                        widget_textbox_move_cursor(widget, 1);
                        break;
                    case XK_Home:
                        widget->cursor_pos = 0;
                        break;
                    case XK_End:
                        widget->cursor_pos = widget->text_len;
                        break;
                    default:
                        if (len > 0 && buffer[0] >= 32 && buffer[0] < 127) {
                            widget_textbox_insert_char(widget, buffer[0]);
                        }
                        break;
                }
                widget_draw(wm, widget);
            }
            break;
            
        case FocusOut:
            if (widget->type == WIDGET_TEXTBOX) {
                widget->focused = 0;
                if (wm->focused_widget == widget) {
                    wm->focused_widget = NULL;
                }
                widget_draw(wm, widget);
            }
            break;
    }
}