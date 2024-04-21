#ifndef _INCLUDE_LOGGER_H_
#define _INCLUDE_LOGGER_H_

#include <ui_window.h>
#include <wchar.h>

#define UI_LOGGER_BUFFER_SIZE 63          // Log buffer, size of the chunk
#define UI_LOGGER_PRINTF_BUFFER_SIZE 2047 // Size of buffer used for sprintf operations

// Printed on memory error
#define LOGGER_MEMORY_ERROR "Failed to allocate memory for logger"

// Convert normal buffer index into circular index
#define ui_logger_i(logr, i) \
    (((logr)->start + (i)) % UI_LOGGER_BUFFER_SIZE)

// Get number of lines in the buffer
#define ui_logger_size(logr) ( \
    ((logr)->end == -1) ? 0 : ( \
        ((logr)->end - (logr)->start < 0) ? \
        ((logr)->end - (logr)->start + UI_LOGGER_BUFFER_SIZE) : \
        ((logr)->end - (logr)->start) \
    ) + 1 \
)

// Get line on given index
#define ui_logger_line(logr, i) \
    ((logr)->buffer[ui_logger_i((logr), (i))])

// Get number of wrap lines inside given line
#define ui_logger_line_size(logr, i) ( \
    ((logr)->buffer_sizes[ui_logger_i((logr), (i))] / (logr)->win->cols) \
    + !!((logr)->buffer_sizes[ui_logger_i((logr), (i))] % (logr)->win->cols) \
)

// Get pointer to wrap line and length of wrap line
#define ui_logger_wline(logr, i, wi, line, size) ( \
    (line) = ((logr)->buffer[ui_logger_i((logr), (i))] + (logr)->win->cols * (wi)), \
    (size) = ((logr)->buffer_sizes[ui_logger_i((logr), (i))] / (logr)->win->cols > (wi)) ? \
        ((logr)->win->cols) : \
        ((logr)->buffer_sizes[ui_logger_i((logr), (i))] % (logr)->win->cols) \
)

struct ui_logger {
    struct ui_window *win;

    int i_line;  // Index of line inside the buffer
    int i_wrap;  // Index of wrap inside the line

    int start; // Buffer start index
    int end;   // Buffer end index

    wchar_t *buffer[UI_LOGGER_BUFFER_SIZE];
    int buffer_sizes[UI_LOGGER_BUFFER_SIZE];
};

// Create new logger component
struct ui_logger * ui_logger_new(void);

// Free logger memory
void ui_logger_free(struct ui_logger *logr);

// Log new line
void ui_logger_log(struct ui_logger *logr, const wchar_t *text);

// Printf like function to use with logger, uses vswprintf and char buffer
// of size UI_LOGGER_PRINTF_BUFFER_SIZE under the hood
void ui_logger_printf(struct ui_logger *logr, const wchar_t *format, ...);

// Attach the logger to window
void ui_logger_attach(struct ui_logger *logr, struct ui_window *win);

// Clear the log buffer
void ui_logger_clear(struct ui_logger *logr);

#endif
