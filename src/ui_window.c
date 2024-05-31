#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <ui_window.h>
#include <helpers.h>
#include <wchar.h>
#include <sys_memory.h>

#include <debug.h>

static int _ui_window_colors_supported = 0;

void ui_window_start_colors(void) {
    if (!has_colors()) return;

    _ui_window_colors_supported = 1;

    init_pair(
        UI_WINDOW_TXT_PAIR_ID, 
        UI_WINDOW_COLOR_TXT_TXT,
        UI_WINDOW_COLOR_TXT_BKG
    );
    init_pair(
        UI_WINDOW_TITLE_PAIR_ID, 
        UI_WINDOW_COLOR_TITLE_TXT,
        UI_WINDOW_COLOR_TITLE_BKG 
    );
    init_pair(
        UI_WINDOW_TITLE_SELECTED_PAIR_ID, 
        UI_WINDOW_COLOR_TITLE_SELECTED_TXT,
        UI_WINDOW_COLOR_TITLE_SELECTED_BKG
    );
    init_pair(
        UI_WINDOW_BORDER_PAIR_ID,
        UI_WINDOW_COLOR_BORDER_TXT,
        UI_WINDOW_COLOR_BORDER_BKG
    );
    init_pair(
        UI_WINDOW_BORDER_SELECTED_PAIR_ID,
        UI_WINDOW_COLOR_BORDER_SELECTED_TXT,
        UI_WINDOW_COLOR_BORDER_SELECTED_BKG
    );
}

void ui_window_stop_colors(void) {
    _ui_window_colors_supported = 0;
}

struct ui_window *ui_window_new(void) {
    struct ui_window *win;

    win = safe_malloc(sizeof(struct ui_window), WINDOW_MEMORY_ERROR);
    memset(win, 0, sizeof(struct ui_window));
    win->refresh_after_handle_draw = 1;

    return win;
}

/**
 * @brief Define the window size position on the screen
 * 
 * If function fails to allocate memory or create the ncurses window
 * it will call sys_memory_crash to crash the application
 * 
 * @param win  window structure to define
 * @param y    Y coordinate (row) where window should be displayed
 * @param x    X coordinate (col) where window should be displayed
 * @param rows number of rows, or height of the window
 * @param cols number of columns, or width of the window
 * 
 * @return always returns zero
 */
int ui_window_define(struct ui_window *win, int y, int x, int rows, int cols) {
    int suby = 0, subx = 0;
    int subrows, subcols;

    win->colors_supported = _ui_window_colors_supported;
    ui_window_call_action(win, UI_WINDOW_BEFORE_DEFINE);

    if (win->win || win->content)
        ui_window_undefine(win);

    win->real_y = win->y = y + (win->manager ? win->manager->win->y : 0);
    win->real_x = win->x = x + (win->manager ? win->manager->win->x : 0);

    if(!(win->win = newwin(rows, cols, win->y, win->x)))
        sys_memory_crash("Failed to allocate ncurses window for UI window");

    subrows = rows = getmaxy(win->win);
    subcols = cols = getmaxx(win->win);

    subx += win->border_active;
    suby += win->title_active + win->border_active;

    subcols -= win->border_active * 2;
    subrows -= win->title_active + win->border_active * 2;

    win->x += subx;
    win->y += suby;

    if(!(win->content = derwin(win->win, subrows, subcols, suby, subx)))
        sys_memory_crash("Failed to allocate ncurses subwindow for UI window content");

    getmaxyx(win->content, win->rows, win->cols);

    if (win->title_active) {
        win->title = safe_malloc(sizeof(wchar_t) * subcols, WINDOW_MEMORY_ERROR);
        memset(win->title, 0, sizeof(wchar_t) * subcols);

        if (!(win->titlewin = derwin(win->win, 1, subcols, suby - 1, subx)))
            sys_memory_crash("Failed to allocate ncurses subwindow for UI window title");
    }

    ui_window_call_action(win, UI_WINDOW_AFTER_DEFINE);
    return 0;
}

void ui_window_undefine(struct ui_window *win) {
    if (!win) return;

    ui_window_call_action(win, UI_WINDOW_UNDEFINE);

    if (win->title)
        free(win->title);
    if (win->win)
        delwin(win->win);
    if (win->content)
        delwin(win->content);
    if (win->titlewin)
        delwin(win->titlewin);

    win->win = NULL;
    win->content = NULL;
    win->title = NULL;
    win->titlewin = NULL;
}

void ui_window_free(struct ui_window *win) {
    ui_window_detach(win);
    ui_window_undefine(win);
    free(win);
}

int ui_window_use_title(struct ui_window *win, int use_it) {
    win->title_active = !!use_it;
    return 0;
}

int ui_window_use_border(struct ui_window *win, int use_it) {
    win->border_active = !!use_it;
    return 0;
}

void ui_window_clear_title(struct ui_window *win) {
    memset(win->title, 0, sizeof(wchar_t) * win->cols);
}

void ui_window_write_title(struct ui_window *win, char *str, enum ui_window_align align) {
    int maxlen, start, len;

    len = mbstowcs(NULL, str, 0);
    maxlen = win->cols;
    len = len > maxlen ? maxlen : len;
    
    switch (align) {
        case UI_WINDOW_LEFT:
            start = 0; break;
        case UI_WINDOW_RIGHT:
            start = (maxlen - len); break;
        case UI_WINDOW_CENTER:
            start = (maxlen - len) / 2; break;
    }

    mbstowcs(win->title + start, str, len);
}

void ui_window_attach(
    struct ui_window *win,
    void *component,
    ui_window_input_cb input_cb
) {
    win->component = component;
    win->handle_input = input_cb;

    // If window is already defined remove old window content
    if (ui_window_is_defined(win)) {
        wmove(win->win, 0, 0);
        wclrtobot(win->win);
    }

    ui_window_call_action(win, UI_WINDOW_ATTACH);
}

void ui_window_detach(struct ui_window *win) {
    int i;

    if (!win) return;

    ui_window_undefine(win);
    ui_window_call_action(win, UI_WINDOW_DETACH);

    win->component = NULL;
    win->handle_input = NULL;
    
    for (i = 0; i < UI_WINDOW_MAX_ACTIONS; i++)
        win->handle_action[i] = NULL;
}

void ui_window_set_selected(struct ui_window *win, int selected) {
    win->selected = selected;
    ui_window_call_action(win,
        selected ? UI_WINDOW_SELECT : UI_WINDOW_UNSELECT
    );
}

void ui_window_draw(struct ui_window *win, int touch, int update) {
    int i;

    // Update colors support
    win->colors_supported = _ui_window_colors_supported;

    // Check if color support changed, some components may need special redrawing
    win->color_support_changed = win->colors_supported != win->_last_color_support;
    win->_last_color_support = win->colors_supported;

    if (!win->win || !win->content)
        return;

    ui_window_call_action(win, UI_WINDOW_BEFORE_DRAW);

    if (win->border_active) {
        if (win->colors_supported)
            box_attr(win->win,
                win->selected ? COLOR_PAIR(UI_WINDOW_BORDER_SELECTED_PAIR_ID) :
                    COLOR_PAIR(UI_WINDOW_BORDER_PAIR_ID)
            );
        else
            box_attr(win->win, A_NORMAL);
    }

    if (win->title_active) {
        wclear(win->titlewin);

        for (i = 0; i < win->cols; i++) {
            if (win->title[i] != 0) {
                // wadd_wch() function takes cchar_t so we need to convert
                // wchar_t to it before printing it using setcchar()
                cchar_t ch;
                setcchar(&ch, win->title + i, 0, 0, NULL);
                mvwadd_wch(win->titlewin, 0, i, &ch);
            }
        }

        if (win->selected) {
            wbkgd(win->titlewin, 
                win->colors_supported ? COLOR_PAIR(UI_WINDOW_TITLE_SELECTED_PAIR_ID) : ('_' | A_REVERSE | A_BOLD)
            );
        } else {
            wbkgd(win->titlewin, 
                win->colors_supported ? COLOR_PAIR(UI_WINDOW_TITLE_PAIR_ID) : A_REVERSE
            );
        }
    }

    if (win->colors_supported)
        wbkgd(win->content, COLOR_PAIR(UI_WINDOW_TXT_PAIR_ID));

    if (touch) {
        touchwin(win->win);
        touchwin(win->content);
    }

    wnoutrefresh(win->win);
    wnoutrefresh(win->content);

    ui_window_call_action(win, UI_WINDOW_AFTER_DRAW);

    if (win->refresh_after_handle_draw) {
        wnoutrefresh(win->win);
        wnoutrefresh(win->content);
    }

    if (update)
        doupdate();
}

void ui_window_handle_input(struct ui_window *win, wchar_t ch, int is_special_key) {
    if (win->handle_input)
        win->handle_input(win, ch, is_special_key, win->component);
}

void ui_window_subscribe(
    struct ui_window *win,
    enum ui_window_actions action,
    ui_window_action_cb cb
) {
    win->handle_action[action] = cb;
}