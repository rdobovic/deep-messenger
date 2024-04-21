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
    zero_chunk(logr, struct ui_logger);
    logr->end = -1;

    return logr;
}

void ui_logger_clear(struct ui_logger *logr) {
    int i;
    for (i = 0; i < ui_logger_size(logr); i++) {
        if (ui_logger_line(logr, i))
            free(ui_logger_line(logr, i));
    }

    if (ui_window_is_defined(logr->win)) {
        wmove(logr->win->content, 0, 0);
        wclrtobot(logr->win->content);
    }

    logr->start = 0;
    logr->end = -1;
    logr->i_line = 0;
    logr->i_wrap = 0;
    ui_logger_draw_if_selected(logr);
}

void ui_logger_free(struct ui_logger *logr) {
    ui_logger_clear(logr);
    free(logr);
}

void ui_logger_log(struct ui_logger *logr, const wchar_t *text) {
    int len, end_index;
    wchar_t *wchp;

    len = wcslen(text);
    wchp = safe_malloc(sizeof(wchar_t) * (len + 1), LOGGER_MEMORY_ERROR);
    wcsncpy(wchp, text, len);
    wchp[len] = 0;

    if (ui_logger_size(logr) == UI_LOGGER_BUFFER_SIZE) {
        free(ui_logger_line(logr, 0));
        logr->start = ui_logger_i(logr, 1);
    }

    end_index = ui_logger_size(logr);

    logr->end = ui_logger_i(logr, end_index);
    logr->buffer[logr->end] = wchp;
    logr->buffer_sizes[logr->end] = len;

    logr->i_line = end_index;
    logr->i_wrap = ui_window_is_defined(logr->win) ?
        (ui_logger_line_size(logr, end_index) - 1) : 0;

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
    int end_index;
    int stop_li, stop_wi;
    struct ui_logger *logr = component;

    if (!is_special_key) {
        if (ch == KEY_CTRL('L')) {
            ui_logger_clear(logr);
        }
        return;
    }

    switch (ch) {
        case KEY_UP:
            // Calculate where to stop scrolling up
            line_cnt = 0;
            for (stop_li = 0; stop_li < ui_logger_size(logr) && line_cnt < win->rows; stop_li++)
                for (stop_wi = 0; stop_wi < ui_logger_line_size(logr, stop_li) && line_cnt < win->rows; stop_wi++)
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
            end_index = ui_logger_size(logr) - 1;
            if (
                logr->i_line < end_index || 
                logr->i_wrap < ui_logger_line_size(logr, end_index) - 1
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

    end_index = ui_logger_size(logr) - 1;
    logr->i_line = end_index;
    logr->i_wrap = ui_logger_line_size(logr, end_index) - 1;
}

void ui_logger_draw_cb(
    struct ui_window *win,
    enum ui_window_actions action,
    void *component
) {
    int i, j;
    int line_cnt = 0;
    int buff_size, line_size, max_lines, bottom_padding;
    struct ui_logger *logr = component;

    line_cnt = 0;
    buff_size = ui_logger_size(logr);

    if (buff_size == 0) return;

    // Calculate size of empty space under the text if there are
    // more rows than lines
    for (i = 0; i < buff_size; i++)  {
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

            ui_logger_wline(logr, i, j, wline, wsize);
            wmove(win->content, (win->rows - 1) - (bottom_padding + line_cnt), 0);
            wclrtoeol(win->content);
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