#ifndef _INCLUDE_UI_MENU_H_
#define _INCLUDE_UI_MENU_H_

#include <wchar.h>
#include <ui_window.h>

#define UI_MENU_LABEL_SIZE 63  // Max size of menu item label
#define UI_MENU_ALLOC_CHUNK 8  // Number of menu items to allocate/reallocate at once

#define MENU_MEMORY_ERROR "Failed to allocate memory for menu UI component"

struct ui_menu;
typedef void (*ui_menu_select_cb)(struct ui_menu *menu, void *attr);

struct ui_menu_item {
    int id;
    wchar_t label[UI_MENU_LABEL_SIZE];
    void *attr;
    ui_menu_select_cb cb;
};

struct ui_menu {
    struct ui_window *win;

    int cursor;        // Selected menu item
    int display_start; // 

    int n_items;       // Number of items used
    int items_alloc;   // Number of items allocated
    struct ui_menu_item *items;
};

// Allocate new menu component
struct ui_menu * ui_menu_new(void);

// Free menu component memory
void ui_menu_free(struct ui_menu *menu);

// Attach component to the window
void ui_menu_attach(struct ui_menu *menu, struct ui_window *win);

// Add new item to the menu
void ui_menu_add(
    struct ui_menu *menu,
    int id,
    char *label,
    ui_menu_select_cb cb,
    void *attr
);

// Remove label with given id
void ui_menu_remove(struct ui_menu *menu, int id);

// Update label on item with given id
void ui_menu_item_update(struct ui_menu *menu, int id, char *label);

#endif