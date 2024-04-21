#include <stdlib.h>
#include <string.h>
#include <ui_window.h>
#include <ui_manager.h>
#include <wchar.h>
#include <debug.h>
#include <sys_memory.h>

#define MANAGER_BROWSE_KEY '\t'

void ui_manager_draw_cb(struct ui_window *win, enum ui_window_actions action, void *component) {
    int i;
    struct ui_manager *mgr = component;

    for (i = 0; i < mgr->n_windows; i++) {
        ui_window_draw(mgr->windows[i], 1, 0);
    }
}

void ui_manager_select_cb(struct ui_window *win, enum ui_window_actions action, void *component) {
    struct ui_manager *mgr = component;

    if (mgr->last_selected_id != UI_MANAGER_NONE_SELECTED)
        ui_manager_select(mgr, mgr->last_selected_id);
}

void ui_manager_unselect_cb(struct ui_window *win, enum ui_window_actions action, void *component) {
    struct ui_manager *mgr = component;
    mgr->last_selected_id = mgr->selected_id;
    ui_manager_select(mgr, UI_MANAGER_NONE_SELECTED);
}

void ui_manager_input_cb(struct ui_window *win, wchar_t ch, int is_special_key, void *component) {
    int i;
    struct ui_manager *mgr = component;

    if (ch == MANAGER_BROWSE_KEY && !is_special_key) {
        int new_index, new_id;
        
        new_index = (mgr->selected_index + 1) % mgr->n_windows;
        new_id = mgr->window_ids[new_index];

        ui_manager_select(mgr, new_id);
        return;
    }

    if (mgr->selected_id != UI_MANAGER_NONE_SELECTED) {
        ui_window_handle_input(mgr->windows[mgr->selected_index], ch, is_special_key);
    }
}

struct ui_manager * ui_manager_new(struct ui_window *win) {
    struct ui_manager *mgr;

    if (!win) return NULL;

    mgr = safe_malloc(sizeof(struct ui_manager), MANAGER_MEMORY_ERROR);
    memset(mgr, 0, sizeof(struct ui_manager));

    mgr->win = win;
    mgr->win->refresh_after_handle_draw = 0;
    ui_window_attach(mgr->win, mgr, ui_manager_input_cb);
    ui_window_subscribe(mgr->win, UI_WINDOW_AFTER_DRAW, ui_manager_draw_cb);
    ui_window_subscribe(mgr->win, UI_WINDOW_SELECT, ui_manager_select_cb);
    ui_window_subscribe(mgr->win, UI_WINDOW_UNSELECT, ui_manager_unselect_cb);

    return mgr;
}

void ui_manager_free(struct ui_manager *mgr) {
    ui_window_detach(mgr->win);
    free(mgr);
}

int ui_manager_add_window(struct ui_manager *mgr, struct ui_window *win, int window_id) {
    int i;

    if (mgr->n_windows == UI_MANAGER_MAX_WINS)
        return 1;

    i = mgr->n_windows++;
    mgr->windows[i] = win;
    mgr->window_ids[i] = window_id;

    win->manager = mgr;

    return 0;
}

void ui_manager_select(struct ui_manager *mgr, int window_id) {
    int i;

    if (mgr->selected_id != UI_MANAGER_NONE_SELECTED) {
        ui_window_set_selected(mgr->windows[mgr->selected_index], 0);
        ui_window_draw(mgr->windows[mgr->selected_index], 1, 0);
    }

    for (i = 0; i < mgr->n_windows; i++)
        if (mgr->window_ids[i] == window_id)
            break;

    if (i == mgr->n_windows) {
        mgr->selected_id = UI_MANAGER_NONE_SELECTED;
        return;
    }

    mgr->selected_id = window_id;
    mgr->selected_index = i;

    ui_window_set_selected(mgr->windows[i], 1);
    ui_window_draw(mgr->windows[i], 1, 0);

    doupdate();
}