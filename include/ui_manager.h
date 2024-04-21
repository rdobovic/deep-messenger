#ifndef _INCLUDE_UI_MANAGER_H_
#define _INCLUDE_UI_MANAGER_H_
#include <ui_window.h>

#define UI_MANAGER_MAX_WINS 32
#define UI_MANAGER_NONE_SELECTED -1

// Printed on memory error
#define MANAGER_MEMORY_ERROR "Failed to allocate memory for UI manager"

#define ui_manager_draw(mgr, upd) \
    ui_window_draw((mgr)->win, 1, (upd))

#define ui_manager_handle_input(mgr, ch, is_special_key) \
    ui_window_handle_input((mgr)->win, (ch), (is_special_key))

struct ui_manager {
    struct ui_window *win;

    int n_windows;
    int window_ids[UI_MANAGER_MAX_WINS];
    struct ui_window *windows[UI_MANAGER_MAX_WINS];

    int selected_id;
    int selected_index;
    int last_selected_id;
};

// Create new manager instance for given window
struct ui_manager * ui_manager_new(struct ui_window *win);

// Free all manager memory (window is not freed)
void ui_manager_free(struct ui_manager *mgr);

// Add window to the manager
int ui_manager_add_window(struct ui_manager *mgr, struct ui_window *win, int window_id);

// Select layout node
void ui_manager_select(struct ui_manager *mgr, int window_id);

#endif