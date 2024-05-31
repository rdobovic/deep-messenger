#include <wchar.h>
#include <ncurses.h>
#include <ui_menu.h>
#include <ui_window.h>
#include <sys_memory.h>
#include <debug.h>
#include <array.h>

struct ui_menu * ui_menu_new(void) {
    struct ui_menu *menu;

    menu = safe_malloc(sizeof(struct ui_menu), MENU_MEMORY_ERROR);
    zero_chunk(menu, struct ui_menu);
    return menu;
}

void ui_menu_free(struct ui_menu *menu) {
    if (menu->items)
        free(menu->items);
    free(menu);
}

void ui_menu_add(
    struct ui_menu *menu,
    int id,
    char *label,
    ui_menu_select_cb cb,
    void *attr
) {
    int i;
    struct ui_menu_item *item;

    if (menu->items_alloc == 0) {
        menu->items = safe_malloc(
            sizeof(struct ui_menu_item) * UI_MENU_ALLOC_CHUNK,
            MENU_MEMORY_ERROR
        );
        menu->items_alloc = UI_MENU_ALLOC_CHUNK;
    }

    if (menu->items_alloc == menu->n_items) {
        menu->items = safe_realloc(
            menu->items,
            sizeof(struct ui_menu_item) * (menu->items_alloc + UI_MENU_ALLOC_CHUNK),
            MENU_MEMORY_ERROR
        );
        menu->items_alloc += UI_MENU_ALLOC_CHUNK;
    }

    item = menu->items + (menu->n_items++);
    item->id = id;
    item->cb = cb;
    item->attr = attr;
    
    // Copy label
    mbstowcs(item->label, label, UI_MENU_LABEL_SIZE);
    item->label[UI_MENU_LABEL_SIZE - 1] = '\0';
}

void ui_menu_item_update(struct ui_menu *menu, int id, char *label) {
    int i;

    for (i = 0; menu->items[i].id != id && i < menu->n_items; i++)
        /* do nothing */;
    if (i == menu->n_items)
        return;

    mbstowcs(menu->items[i].label, label, UI_MENU_LABEL_SIZE);
    menu->items[i].label[UI_MENU_LABEL_SIZE - 1] = '\0';
}

void ui_menu_remove(struct ui_menu *menu, int id) {
    int rm_i;

    for (rm_i = 0; menu->items[rm_i].id != id && rm_i < menu->n_items; rm_i++)
        /* do nothing */;
    if (rm_i == menu->n_items)
        return;

    for (; rm_i + 1 < menu->n_items; rm_i++) {
        menu->items[rm_i] = menu->items[rm_i + 1];
    }
    --menu->n_items;
    menu->cursor = 0;
    menu->display_start = 0;
}

void ui_menu_input_cb(
    struct ui_window *win, 
    wchar_t ch, 
    int is_special_key, 
    void *component
) {
    struct ui_menu *menu = component;

    if (!is_special_key) {
        if (ch == '\n') {
            if (menu->items[menu->cursor].cb) {
                menu->items[menu->cursor].cb(menu, menu->items[menu->cursor].attr);
            }
        }
        return;
    }

    switch (ch) {
        case KEY_UP:
            debug("KEY UP MENU");
            if (menu->cursor > 0) {
                --menu->cursor;
                if (menu->display_start > menu->cursor)
                    --menu->display_start;
            }
            break;
        case KEY_DOWN:
            debug("KEY DOWN MENU");
            if (menu->cursor < menu->n_items - 1) {
                ++menu->cursor;
                if (
                    menu->display_start + menu->win->rows <= menu->cursor
                ) {
                    ++menu->display_start;
                }
            }
            break;
    }

    ui_window_draw(win, 1, 1);
}

void ui_menu_draw_cb(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
) {
    int i;
    struct ui_menu *menu = component;

    // Fix viewport if neeeded
    if (menu->display_start > 0 && menu->n_items - menu->display_start < win->rows) {
        menu->display_start = 0;
    }
    if (menu->cursor - menu->display_start > win->rows - 1) {
        menu->display_start = menu->cursor - (win->rows - 1);
    }

    wmove(win->content, 0, 0);
    wclrtobot(win->content);

    for (
        i = menu->display_start; 
        i < menu->n_items && i < menu->display_start + win->rows; i++
    ) {
        if (i == menu->cursor) {
            wattrset(win->content, A_BOLD);
            wmove(win->content, i - menu->display_start, 0);
            waddstr(win->content, ">");
            wmove(win->content, i - menu->display_start, win->cols - 1);
            waddstr(win->content, "<");
        }
        
        wmove(win->content, i - menu->display_start, 2);
        waddnwstr(win->content, menu->items[i].label, win->cols - 4);
        wattrset(win->content, A_NORMAL);
    }
}

void ui_menu_attach(struct ui_menu *menu, struct ui_window *win) {
    menu->win = win;
    ui_window_attach(win, menu, ui_menu_input_cb);
    ui_window_subscribe(win, UI_WINDOW_AFTER_DRAW, ui_menu_draw_cb);
}