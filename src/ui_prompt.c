#include <ctype.h>
#include <wchar.h>
#include <ncurses.h>
#include <ui_prompt.h>
#include <ui_window.h>
#include <stdlib.h>
#include <string.h>
#include <debug.h>
#include <helpers.h>
#include <sys_memory.h>
#include <array.h>

// Create new prompt
struct ui_prompt * ui_prompt_new(void) {
    struct ui_prompt *prt;

    prt = safe_malloc(sizeof(struct ui_prompt), PROMPT_MEMORY_ERROR);
    memset(prt, 0, sizeof(prt));
    prt->mb_input_buffer = array(char);
    return prt;
}

// Free prompt memory
void ui_prompt_free(struct ui_prompt *prt) {

    if (prt->win)
        ui_window_detach(prt->win);

    array_free(prt->mb_input_buffer);
    free(prt);
}

// Clear the prompt buffer
void ui_prompt_clear(struct ui_prompt *prt) {
    prt->ch_cursor = 0;
    prt->buffer_len = 0;
    prt->display_start = 0;

    if (ui_window_is_defined(prt->win))
        ui_window_draw(prt->win, 1, 1);
}

// Handle user input
void ui_prompt_input_cb(
    struct ui_window *win,
    wchar_t ch,
    int is_special_key,
    void *component
) {
    int i;
    struct ui_prompt *prt = component;

    if (is_special_key) {
        switch (ch) {

            case KEY_BACKSPACE:
                if (prt->ch_cursor > 0) {
                    for (i = prt->ch_cursor; i < prt->buffer_len; i++) {
                        prt->input_buffer[i - 1] = prt->input_buffer[i];
                    }
                    --(prt->ch_cursor);
                    --(prt->buffer_len);
                }
                break;

            case KEY_DC:
                if (prt->ch_cursor < prt->buffer_len) {
                    for (i = prt->ch_cursor; i < prt->buffer_len - 1; i++) {
                        prt->input_buffer[i] = prt->input_buffer[i + 1];
                    }
                    --(prt->buffer_len);
                }
                break;

            case KEY_LEFT:
                if (prt->ch_cursor > 0) {
                    --(prt->ch_cursor);
                }
                break;

            case KEY_RIGHT:
                if (prt->ch_cursor < prt->buffer_len) {
                    ++(prt->ch_cursor);
                }
                break;

            case KEY_HOME:
                prt->display_start = 0;
                prt->ch_cursor = prt->display_start;
                break;

            case KEY_END:
                prt->ch_cursor = prt->buffer_len;
                break;
        }

    } else if (ch == KEY_CTRL('L')) {
        prt->ch_cursor = 0;
        prt->buffer_len = 0;
        prt->display_start = 0;

    } else if (ch == '\n') {
        prt->input_buffer[prt->buffer_len] = 0;
        if (prt->submit_cb)
            prt->submit_cb(prt, prt->cb_attribute);

    } else if (ch == '\r') {
        // Do nothing

    } else if (prt->buffer_len < UI_PROMPT_BUFFER_SIZE) {
        if (!iscntrl(ch)) {
            for (i = prt->buffer_len - 1; i >= prt->ch_cursor; i--) {
                prt->input_buffer[i + 1] = prt->input_buffer[i];
            }

            prt->input_buffer[prt->ch_cursor] = ch;
            ++(prt->ch_cursor);
            ++(prt->buffer_len);
        }
    }

    // If there is more to display than currently displayed
    if (
        prt->buffer_len > (win->cols - 3) && 
        prt->buffer_len - prt->display_start < (win->cols - 3)
    ) {
        prt->display_start = prt->buffer_len - (win->cols - 3);
    }

    // Cursor is of the screen to the right
    if (prt->display_start < prt->ch_cursor - (win->cols - 3))
        prt->display_start = prt->ch_cursor - (win->cols - 3);

    // Cursor is of the screen to the left
    if (prt->display_start > prt->ch_cursor)
        prt->display_start = prt->ch_cursor;

    // Print prompt content
    ui_window_draw(win, 1, 1);
}

// Handle prompt window selection
void ui_prompt_select_cb(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
) {
    if (action == UI_WINDOW_SELECT) {
        curs_set(1);
    }

    if (action == UI_WINDOW_UNSELECT) {
        curs_set(0);
    }

    wrefresh(win->content);
}

// Setup prompt title before drawing
void ui_prompt_before_draw_cb(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
) {
    struct ui_prompt *prt = component;
    ui_window_clear_title(win);
    if (prt->buffer_len == UI_PROMPT_BUFFER_SIZE)
        ui_window_write_title(win, "Type your message (you reached size limit):", UI_WINDOW_LEFT);
    else
        ui_window_write_title(win, "Type your message:", UI_WINDOW_LEFT);
}

// Draw prompt to the screen
void ui_prompt_after_draw_cb(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
) {
    struct ui_prompt *prt = component;

    wmove(win->content, 0, 0);
    wclrtoeol(win->content);

    mvwaddstr(win->content, 0, 0, ">");
    mvwaddnwstr(
        win->content, 0, 2, (prt->input_buffer + prt->display_start),
        (prt->buffer_len < win->cols - 3) ? prt->buffer_len : (win->cols - 3)
    );

    wmove(win->content, 0, prt->ch_cursor - prt->display_start + 2);
}

// Attach prompt component to the window
void ui_prompt_attach(struct ui_prompt *prt, struct ui_window *win) {
    prt->win = win;

    ui_window_attach(win, prt, ui_prompt_input_cb);
    ui_window_subscribe(win, UI_WINDOW_SELECT, ui_prompt_select_cb);
    ui_window_subscribe(win, UI_WINDOW_UNSELECT, ui_prompt_select_cb);
    ui_window_subscribe(win, UI_WINDOW_BEFORE_DRAW, ui_prompt_before_draw_cb);
    ui_window_subscribe(win, UI_WINDOW_AFTER_DRAW, ui_prompt_after_draw_cb);
}

// Set submit callback
void ui_prompt_set_submit_cb(struct ui_prompt *prt, ui_prompt_submit_cb cb, void *att) {
    prt->submit_cb = cb;
    prt->cb_attribute = att;
}

// Convert prompt wchar buffer to chars and return it
char * ui_prompt_get_input(struct ui_prompt *prt) {
    size_t len;

    len = wcstombs(NULL, prt->input_buffer, 0) + 1;
    array_expand(prt->mb_input_buffer, len);
    wcstombs(prt->mb_input_buffer, prt->input_buffer, len);
    
    return prt->mb_input_buffer;
}