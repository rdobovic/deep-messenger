#include <array.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <ui_window.h>
#include <ui_logger.h>
#include <sys_memory.h>
#include <debug.h>
#include <helpers.h>
#include <stdarg.h>

static void ui_logger_draw_if_selected(struct ui_logger *logr) {
    // Redraw the window if it's selected or manager is selected
    if (
        ui_window_is_defined(logr->win) && (logr->win->selected || 
            logr->win->manager && 
            ui_window_is_defined(logr->win->manager->win) && 
            logr->win->manager->win->selected
        )
    ) {
        ui_window_draw(logr->win, 1, 1);
    }
}

struct ui_logger * ui_logger_new(void) {
    struct ui_logger *logr;

    logr = safe_malloc(sizeof(struct ui_logger), LOGGER_MEMORY_ERROR);
    memset(logr, 0, sizeof(struct ui_logger));

    logr->lines = array(wchar_t *);
    logr->line_sizes = array(int);

    return logr;
}

void ui_logger_clear(struct ui_logger *logr) {
    int i;
    for (i = 0; i < logr->size; i++) {
        array_free(logr->lines[i]);
    }

    if (ui_window_is_defined(logr->win)) {
        wmove(logr->win->content, 0, 0);
        wclrtobot(logr->win->content);
    }

    logr->size = 0;
    logr->i_line = 0;
    logr->i_wrap = 0;
    ui_logger_draw_if_selected(logr);
}

void ui_logger_free(struct ui_logger *logr) {
    ui_logger_clear(logr);
    array_free(logr->lines);
    array_free(logr->line_sizes);
    free(logr);
}

void ui_logger_log(struct ui_logger *logr, const wchar_t *text) {
    int len, i, line_start;

    len = wcslen(text);
    line_start = 0;

    for (i = 0; i <= len; i++) {
        wchar_t *wchp;

        if (text[i] == '\n' || i == len) {
            // Allocate array for new line
            wchp = array(wchar_t);
            array_expand(wchp, i - line_start + 1);
            wcsncpy(wchp, text + line_start, i - line_start);
            wchp[i - line_start] = 0;

            // Store line data
            array_set(logr->lines, logr->size, wchp);
            array_set(logr->line_sizes, logr->size, i - line_start);

            ++logr->size;
            line_start = i + 1;
        }
    }

    logr->i_line = logr->size - 1;
    if (ui_window_is_defined(logr->win))
        logr->i_wrap = ui_logger_line_size(logr, logr->i_line) - 1;
    else
        logr->i_wrap = 0;

    // Redraw the window if it's selected or manager is selected
    ui_logger_draw_if_selected(logr);
}

void ui_logger_printf(struct ui_logger *logr, const wchar_t *format, ...) {
    va_list vl;
    wchar_t buffer[UI_LOGGER_PRINTF_BUFFER_SIZE];

    va_start(vl, format);
    if (vswprintf(buffer, UI_LOGGER_PRINTF_BUFFER_SIZE, format, vl) == -1) {
        ui_logger_log(logr, L"??? log message was too long to fit in printf buffer ???");
    } else {
        ui_logger_log(logr, buffer);
    }
    va_end(vl);
}

void ui_logger_input_cb(
    struct ui_window *win,
    wchar_t ch,
    int is_special_key,
    void *component
) {
    int line_cnt;
    int stop_li, stop_wi;
    struct ui_logger *logr = component;

    if (!is_special_key) {
        if (ch == KEY_CTRL('L') && logr->allow_clear) {
            ui_logger_clear(logr);
        }
        return;
    }

    switch (ch) {
        case KEY_UP:
            // Calculate where to stop scrolling up
            line_cnt = 0;
            for (stop_li = 0; stop_li < logr->size && line_cnt < win->rows; stop_li++)
                for (stop_wi = 0; stop_wi < ui_logger_line_size(logr, stop_wi) && line_cnt < win->rows; stop_wi++)
                    ++line_cnt;

            if (logr->i_line >= stop_li || logr->i_wrap >= stop_wi) {
                if (logr->i_wrap > 0) {
                    --logr->i_wrap;
                } else {
                    --logr->i_line;
                    logr->i_wrap = ui_logger_line_size(logr, logr->i_line) - 1;
                }
            }
            break;

        case KEY_DOWN:
            if (
                logr->i_line < logr->size - 1 || 
                logr->i_wrap < ui_logger_line_size(logr, logr->size - 1) - 1
            ) {
                if (logr->i_wrap < ui_logger_line_size(logr, logr->i_line) - 1) {
                    ++logr->i_wrap;
                } else {
                    ++logr->i_line;
                    logr->i_wrap = 0;
                }
            }
            break;
    }

    ui_window_draw(logr->win, 1, 1);
}

void ui_logger_define_cb(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
) {
    int end_index;
    struct ui_logger *logr = component;

    if (logr->size > 0) {
        logr->i_line = logr->size - 1;
        logr->i_wrap = ui_logger_line_size(logr, logr->i_line) - 1;
    } else {
        logr->i_line = logr->i_wrap = 0;
    }
}

void ui_logger_draw_cb(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
) {
    int i, j;
    int line_cnt = 0;
    int line_size, max_lines, bottom_padding;
    struct ui_logger *logr = component;

    if (logr->size == 0) return;

    // Calculate size of empty space under the text if there are
    // more rows than lines
    line_cnt = 0;
    for (i = 0; i < logr->size; i++)  {
        line_cnt += ui_logger_line_size(logr, i);
        bottom_padding = win->rows - line_cnt;

        if (bottom_padding < 0) {
            bottom_padding = 0;
            break;
        }
    }

    // Draw lines in the content window
    line_cnt = 0;
    for (i = logr->i_line; i >= 0; i--) {
        line_size = ui_logger_line_size(logr, i);

        for (j = line_cnt ? line_size - 1 : logr->i_wrap; j >= 0; j--) {
            int wsize;      // Wrap line size
            wchar_t *wline; // Wrap line pointer

            if (line_cnt == win->rows)
                goto done_drawing;

            wline = ui_logger_get_wline(logr, i, j);
            wsize = ui_logger_wline_size(logr, i, j);

            wmove(win->content, (win->rows - 1) - (bottom_padding + line_cnt), 0);
            wclrtoeol(win->content);
            if (wsize > 0)
                waddnwstr(win->content, wline, wsize);
            ++line_cnt;
        }
    }
    done_drawing:
}

void ui_logger_attach(struct ui_logger *logr, struct ui_window *win) {
    logr->win = win;
    ui_window_attach(win, logr, ui_logger_input_cb);
    ui_window_subscribe(win, UI_WINDOW_AFTER_DRAW, ui_logger_draw_cb);
    ui_window_subscribe(win, UI_WINDOW_AFTER_DEFINE, ui_logger_define_cb);
}