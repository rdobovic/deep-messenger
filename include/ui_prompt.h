#ifndef _INCLUDE_UI_PROMPT_H_
#define _INCLUDE_UI_PROMPT_H_

#include <ui_window.h>
#include <wchar.h>

//#define UI_PROMPT_BUFFER_SIZE 8192
#define UI_PROMPT_BUFFER_SIZE 4095 // For testing (remove later)

// Printed on memory error
#define PROMPT_MEMORY_ERROR "Failed to allocate memory for prompt UI component"

struct ui_prompt;

typedef void (*ui_prompt_submit_cb)(struct ui_prompt *prt, void *att);

struct ui_prompt {
    struct ui_window *win;

    void *cb_attribute;

    int ch_cursor;
    int display_start;
    int buffer_len;
    wchar_t input_buffer[UI_PROMPT_BUFFER_SIZE + 1];

    ui_prompt_submit_cb submit_cb;
};

// Create new prompt instance
struct ui_prompt * ui_prompt_new(void);

// Detach from window and free memory
void ui_prompt_free(struct ui_prompt *prt);

// Attach the prompt to the window
void ui_prompt_attach(struct ui_prompt *prt, struct ui_window *win);

// Set callback to call on submit
void ui_prompt_set_submit_cb(struct ui_prompt *prt, ui_prompt_submit_cb cb, void *att);

// Clear the prompt
void ui_prompt_clear(struct ui_prompt *prt);

#endif