#ifndef _INCLUDE_UI_STACK_H_
#define _INCLUDE_UI_STACK_H_

#include <ui_window.h>

// UI stack size
#define UI_STACK_MAX_WINDOWS 32
// Printed on memory error
#define STACK_MEMORY_ERROR "Failed to allocate memory for UI stack"

struct ui_stack {
    int n_wins;
    struct ui_window *wins[UI_MANAGER_MAX_WINS];
};

// Create new stack instance
struct ui_stack * ui_stack_new(void);

// Free all the stack memory
void ui_stack_free(struct ui_stack *stack);

// Push new window to the stack
int ui_stack_push(struct ui_stack *stack, struct ui_window *win);

// Pop top most window from the stack
struct ui_window * ui_stack_pop(struct ui_stack *stack);

// Pop top most window from the stack and destroy it
int ui_stack_trash(struct ui_stack *stack);

// Forward input top window
void ui_stack_handle_input(struct ui_stack *stack, wchar_t ch, int is_special_key);

// Redraw entire stack
void ui_stack_redraw(struct ui_stack *stack);

#endif