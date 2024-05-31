#ifndef _INCLUDE_LOGGER_H_
#define _INCLUDE_LOGGER_H_

#include <ui_window.h>
#include <wchar.h>

#define UI_LOGGER_PRINTF_BUFFER_SIZE 2047 // Size of buffer used for sprintf operations

// Printed on memory error
#define LOGGER_MEMORY_ERROR "Failed to allocate memory for logger"

// Get number of wrap lines inside given line
#define ui_logger_line_size(logr, i) ( \
    ((logr)->line_sizes[i] == 0) \
    + ((logr)->line_sizes[i] / (logr)->win->cols) \
    + !!((logr)->line_sizes[i] % (logr)->win->cols) \
)

// Get pointer to given wrap line
#define ui_logger_get_wline(logr, i, wi) \
    ((logr)->lines[i] + (logr)->win->cols * (wi))

// Get length of wrap line
#define ui_logger_wline_size(logr, i, wi) ( \
    ((logr)->line_sizes[i] / (logr)->win->cols > (wi)) ? \
    ((logr)->win->cols) : \
    ((logr)->line_sizes[i] % (logr)->win->cols) \
)

struct ui_logger {
    struct ui_window *win;

    int i_line;  // Index of line inside the buffer
    int i_wrap;  // Index of wrap inside the line
    int allow_clear;

    int size;

    wchar_t **lines;
    int *line_sizes;
};

// Create new logger component
struct ui_logger * ui_logger_new(void);

// Free logger memory
void ui_logger_free(struct ui_logger *logr);

// Log new line
void ui_logger_log(struct ui_logger *logr, const char *text);

// Log new line, but use wide char string (for UI internal use)
void ui_logger_log_wc(struct ui_logger *logr, const wchar_t *text);

// Printf like function to use with logger, uses vswprintf and char buffer
// of size UI_LOGGER_PRINTF_BUFFER_SIZE under the hood
void ui_logger_printf(struct ui_logger *logr, const char *format, ...);

// Attach the logger to window
void ui_logger_attach(struct ui_logger *logr, struct ui_window *win);

// Clear the log buffer
void ui_logger_clear(struct ui_logger *logr);

#endif
