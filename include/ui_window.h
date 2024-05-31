#ifndef _INCLUDE_UI_WINDOW_H_
#define _INCLUDE_UI_WINDOW_H_

#include <ncurses.h>
#include <ui_manager.h>
#include <wchar.h>

#define WINDOW_MEMORY_ERROR "Failed to allocate memory for UI window"

/* Some window enums */

// Alignment of title in the window
enum ui_window_align {
    UI_WINDOW_LEFT,
    UI_WINDOW_RIGHT,
    UI_WINDOW_CENTER
};

enum ui_window_actions {
    UI_WINDOW_SELECT,
    UI_WINDOW_UNSELECT,
    UI_WINDOW_ATTACH,
    UI_WINDOW_DETACH,
    UI_WINDOW_BEFORE_DRAW,
    UI_WINDOW_AFTER_DRAW,
    UI_WINDOW_BEFORE_DEFINE,
    UI_WINDOW_AFTER_DEFINE,
    UI_WINDOW_UNDEFINE,
    UI_WINDOW_MAX_ACTIONS,
};

/* Window callback functions */

struct ui_window;

// This type of function is called when input is received and is
// passed to the window by the UI manager
typedef void (*ui_window_input_cb)(
    struct ui_window *win, 
    wchar_t ch, 
    int is_special_key, 
    void *component
);

// A function type called when action is detected on the window
typedef void (*ui_window_action_cb)(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
);

// Call window callback if defined
#define ui_window_call_action(win, action) \
    if ((win)->handle_action[action]) { \
        (win)->handle_action[action]((win), (action), (win)->component); \
    }

/* Window structures */

struct ui_window {
    WINDOW *win;
    WINDOW *content;
    WINDOW *titlewin;

    // Parent manager component (if present)
    struct ui_manager *manager;

    // Ponter to component attached to window, send when
    // calling callback functions
    void *component;

    int selected;
    int title_active;
    int border_active;
    int refresh_after_handle_draw;
    int colors_supported;
    int color_support_changed;
    int _last_color_support;

    wchar_t *title;

    int y, x;       // Window coordinates (content window)
    int rows, cols; // Window dimensions (content window)

    int real_y, real_x; // Actual window coordinates

    // Virtual methods called by container
    ui_window_input_cb handle_input;                           // When input is received
    ui_window_action_cb handle_action[UI_WINDOW_MAX_ACTIONS];  // When window is selected/unselected
};

/* Window methods */

// Allocate new window
struct ui_window *ui_window_new(void);

// Check if colors are supported and create color pairs
void ui_window_start_colors(void);

// Disable colors for all windows, this function should NOT be used
// since there is no way to disable ncurses colors once they are started anyway,
// if you use this function you need to redefine all windows for it to take effect
void ui_window_stop_colors(void);

// Check if window is defined
#define ui_window_is_defined(window) \
    ((window) && (window)->win && (window)->content)

// Set the window size and allocate ncurses WINDOW
int ui_window_define(struct ui_window *win, int y, int x, int rows, int cols);

// Free ncurses windows
void ui_window_undefine(struct ui_window *win);

// Free the window memory
void ui_window_free(struct ui_window *win);

// Turn on title support
int ui_window_use_title(struct ui_window *win, int use_it);

// Clear title of the window
void ui_window_clear_title(struct ui_window *win);

// Write text in window title area
void ui_window_write_title(struct ui_window *win, char *str, enum ui_window_align align);

// Turn on border support and draw border later
int ui_window_use_border(struct ui_window *win, int use_it);

// Draw given window to the screen
void ui_window_draw(struct ui_window *win, int touch, int update);

// Attach component to the window
void ui_window_attach(struct ui_window *win, void *component, ui_window_input_cb input_cb);

// Subscribe to window action
void ui_window_subscribe(
    struct ui_window *win,
    enum ui_window_actions action,
    ui_window_action_cb cb
);

// Set if window is selected
void ui_window_set_selected(struct ui_window *win, int selected);

// Let window handle input (component handles)
void ui_window_handle_input(struct ui_window *win, wchar_t ch, int is_special_key);

// Tell the attached component to detach from the window
void ui_window_detach(struct ui_window *win);

/* Definition of all window colors */

// Content subwindow colors (not used, I think...)
#define UI_WINDOW_TXT_PAIR_ID 10
#define UI_WINDOW_COLOR_TXT_BKG -1  /* Default */
#define UI_WINDOW_COLOR_TXT_TXT -1  /* Default */

// Title subwindow colors
#define UI_WINDOW_TITLE_PAIR_ID 11
#define UI_WINDOW_COLOR_TITLE_BKG COLOR_BLUE
#define UI_WINDOW_COLOR_TITLE_TXT COLOR_WHITE

// Title subwindow colors (when selected)
#define UI_WINDOW_TITLE_SELECTED_PAIR_ID 12
#define UI_WINDOW_COLOR_TITLE_SELECTED_BKG COLOR_GREEN
#define UI_WINDOW_COLOR_TITLE_SELECTED_TXT COLOR_BLACK

// Window border colors
#define UI_WINDOW_BORDER_PAIR_ID 13
#define UI_WINDOW_COLOR_BORDER_BKG -1  /* Default */
#define UI_WINDOW_COLOR_BORDER_TXT -1  /* Default */

// Window border colors (when selected)
#define UI_WINDOW_BORDER_SELECTED_PAIR_ID 14
#define UI_WINDOW_COLOR_BORDER_SELECTED_BKG -1           /* Default */
#define UI_WINDOW_COLOR_BORDER_SELECTED_TXT COLOR_GREEN

#endif