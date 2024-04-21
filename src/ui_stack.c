#include <string.h>
#include <stdlib.h>
#include <ui_stack.h>
#include <ui_window.h>
#include <ncurses.h>
#include <sys_memory.h>
#include <debug.h>

struct ui_stack * ui_stack_new(void) {
    struct ui_stack *stack;

    stack = safe_malloc(sizeof(struct ui_stack), STACK_MEMORY_ERROR);
    memset(stack, 0, sizeof(struct ui_stack));
    return stack;
}

void ui_stack_free(struct ui_stack *stack) {
    free(stack);
}

void ui_stack_handle_input(struct ui_stack *stack, wchar_t ch, int is_special_key) {
    if (!stack->n_wins) return;

    if (ch == '\e') {
        if (stack->n_wins > 1) {
            ui_stack_trash(stack);
        }
        return;
    }

    ui_window_handle_input(stack->wins[stack->n_wins - 1], ch, is_special_key);
}

struct ui_window * ui_stack_pop(struct ui_stack *stack) {
    int i;

    if (stack->n_wins == 0)
        return NULL;

    touchwin(stdscr);
    wnoutrefresh(stdscr);

    // Redraw all background windows if defined
    for (i = 0; i < stack->n_wins - 2; i++) {
        if (ui_window_is_defined(stack->wins[i]))
            ui_window_draw(stack->wins[i], 1, 0);
    }

    // If top window is selected unselect it
    if (stack->wins[stack->n_wins - 1]->selected)
        ui_window_set_selected(stack->wins[stack->n_wins - 1], 0);

    // Select window below it
    ui_window_set_selected(stack->wins[stack->n_wins - 2], 1);

    // If defined redraw window below
    if (ui_window_is_defined(stack->wins[stack->n_wins - 2]))
        ui_window_draw(stack->wins[stack->n_wins - 2], 1, 0);

    doupdate();
    return stack->wins[--(stack->n_wins)];
}

int ui_stack_trash(struct ui_stack *stack) {
    struct ui_window *win = ui_stack_pop(stack);

    if (!win) return 1;
    ui_window_detach(win);
    ui_window_free(win);
    
    return 0;
}

int ui_stack_push(struct ui_stack *stack, struct ui_window *win) {
    int i;
    if (!win) return 1;

    if (stack->n_wins == UI_STACK_MAX_WINDOWS)
        return 1;

    ui_window_set_selected(win, 1);
    for (i = 0; i < stack->n_wins; i++) {
        if (stack->wins[i]->selected)
            ui_window_set_selected(stack->wins[i], 0);
        if (ui_window_is_defined(stack->wins[i]))
            ui_window_draw(stack->wins[i], 1, 0);
    }

    stack->wins[stack->n_wins++] = win;
    if (ui_window_is_defined(win))
        ui_window_draw(win, 1, 1);

    return 0;
}

void ui_stack_redraw(struct ui_stack *stack) {
    int i;
    touchwin(stdscr);
    wnoutrefresh(stdscr);

    for (i = 0; i < stack->n_wins; i++) {
        if (ui_window_is_defined(stack->wins[i]))
            ui_window_draw(stack->wins[i], 1, 0);
    }
    doupdate();
}