#include <ncurses.h>
#include <form.h>

int main() {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Create a form
    FORM *form = new_form(NULL);

    // Define fields
    FIELD *fields[3];
    fields[0] = new_field(1, 10, 0, 0, 0, 0);
    fields[1] = new_field(1, 10, 2, 0, 0, 0);
    fields[2] = NULL;

    // Set fields for the form
    set_form_fields(form, fields);

    // Post the form to the screen
    post_form(form);
    refresh();

    // Loop to interact with the form
    int ch;
    while ((ch = getch()) != KEY_F(1)) {
        // Process input
        switch (ch) {
            case KEY_DOWN:
                form_driver(form, REQ_NEXT_FIELD);
                form_driver(form, REQ_END_LINE);
                break;
            case KEY_UP:
                form_driver(form, REQ_PREV_FIELD);
                form_driver(form, REQ_END_LINE);
                break;
            default:
                form_driver(form, ch);
                break;
        }
    }

    // Clean up
    unpost_form(form);
    free_form(form);
    free_field(fields[0]);
    free_field(fields[1]);
    endwin();

    return 0;
}