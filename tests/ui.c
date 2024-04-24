#include <ncurses.h>
#include <ui_window.h>
#include <ui_manager.h>
#include <stdio.h>
#include <stdlib.h>
#include <debug.h>
#include <helpers.h>
#include <locale.h>
#include <wchar.h>
#include <ui_prompt.h>
#include <ui_stack.h>
#include <ui_logger.h>
#include <ui_menu.h>
#include <sys_memory.h>

int colors_on = 1;

// Window manager
static struct ui_manager *mgr;
static struct ui_window *mgrwin;
static struct ui_stack *stack;

// All UI parts
static struct ui_window *chat, *settings, *contacts, *prompt, *info;
static struct ui_prompt *prompt_comp;
static struct ui_logger *logger;
static struct ui_menu *menu_comp;

void prompt_submit(struct ui_prompt *prt, void *att) {
    ui_logger_printf(logger, L"PROMPT >> %ls", prt->input_buffer);
    ui_prompt_clear(prt);
}

void print_something_menu(struct ui_menu *menu, void *att) {
    debug("Submit on menu %d", att);
}

void ncurses_crash(void *attr) {
    endwin();
    debug("CRASH NCURSES CB");
}

int ui_init(void) {
    // Init ncurses stuff
    debug("Running ncurses init");
    initscr();
    curs_set(0);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();

    debug("Setting crash CB");
    sys_crash_cb_add(ncurses_crash, NULL);

    // Create window color pairs
    ui_window_start_colors();

    debug("Allocating stack and manager");

    stack = ui_stack_new();

    mgrwin = ui_window_new();
    mgr = ui_manager_new(mgrwin);

    //ui_window_use_border(mgrwin, 1);

    debug("Creating windows");

    chat = ui_window_new();
    info = ui_window_new();
    prompt = ui_window_new();
    contacts = ui_window_new();
    settings = ui_window_new();

    debug("Attaching windows to the manager");

    ui_manager_add_window(mgr, info,     0);
    ui_manager_add_window(mgr, chat,     1);
    ui_manager_add_window(mgr, prompt,   2);
    ui_manager_add_window(mgr, contacts, 4);
    ui_manager_add_window(mgr, settings, 3);

    debug("Creating prompt");

    prompt_comp = ui_prompt_new();
    ui_prompt_attach(prompt_comp, prompt);
    ui_prompt_set_submit_cb(prompt_comp, prompt_submit, NULL);

    debug("Creating logger");

    logger = ui_logger_new();
    ui_logger_attach(logger, info);

    debug("Populating logger");

    size_t len;
    char *str;
    FILE *fd = fopen("/home/roko/random.txt", "r");

    debug("File opened %p", fd);

    while (getline(&str, &len, fd) != -1) {
        wchar_t buff[512];
        mbstowcs(buff, str, 510);
        ui_logger_log(logger, buff);
    }

    debug("Generating and attaching menu comp");

    menu_comp = ui_menu_new();
    ui_menu_add(menu_comp, 0, L"Hello world 01", print_something_menu, NULL);
    ui_menu_add(menu_comp, 1, L"Hello world 02", print_something_menu, NULL);
    ui_menu_add(menu_comp, 2, L"Hello world 03 AAAAAAAAAAAAAAAAAAAAAAA", print_something_menu, NULL);
    ui_menu_add(menu_comp, 3, L"Hello world 04", print_something_menu, NULL);
    ui_menu_add(menu_comp, 4, L"Hello world 05", print_something_menu, NULL);
    ui_menu_add(menu_comp, 5, L"Hello world 06", print_something_menu, NULL);
    ui_menu_add(menu_comp, 6, L"Hello world 07", print_something_menu, NULL);
    ui_menu_add(menu_comp, 7, L"Hello world 08", print_something_menu, NULL);
    ui_menu_add(menu_comp, 8, L"Hello world 09", print_something_menu, NULL);
    ui_menu_attach(menu_comp, settings);

    debug("Adding manager to the stack");

    ui_stack_push(stack, mgrwin);
    refresh();

    debug("Init done");
}

int ui_define(void) {
    int col1_w, col2_w, divider_w;
    int settings_h, contacts_h, chat_h, prompt_h, info_h;

    ui_window_define(mgrwin, 0, 0, 0, 0);

    divide("p30f1p70", mgrwin->cols, &col1_w, &divider_w, &col2_w);
    divide("p40p60", mgrwin->rows, &settings_h, &contacts_h);
    divide("p30p70f2", mgrwin->rows, &info_h, &chat_h, &prompt_h);

    wmove(mgrwin->content, 0, col1_w);
    wvline(mgrwin->content, ACS_VLINE, mgrwin->rows);

    ui_window_use_title(chat, 1);
    ui_window_use_title(info, 1);
    ui_window_use_title(prompt, 1);
    ui_window_use_title(contacts, 1);
    ui_window_use_title(settings, 1);

    ui_window_define(chat, info_h, col1_w + divider_w, chat_h, col2_w);
    ui_window_define(info, 0, col1_w + divider_w, info_h, col2_w);
    ui_window_define(prompt, info_h + chat_h, col1_w + divider_w, prompt_h, col2_w);
    ui_window_define(contacts, settings_h, 0, contacts_h, col1_w);
    ui_window_define(settings, 0, 0, settings_h, col1_w);

    return 0;
}

int ui_end(void) {

    ui_window_free(chat);
    ui_window_free(info);
    ui_window_free(prompt);
    ui_window_free(contacts);
    ui_window_free(settings);

    ui_manager_free(mgr);
    ui_window_free(mgrwin);

    return 0;
}

int ui_add_titiles(void) {
    ui_window_write_title(chat, L"rdobovic", UI_WINDOW_LEFT);
    ui_window_write_title(chat, L"CHAT", UI_WINDOW_CENTER);
    ui_window_write_title(info, L"Deep Messenger v1.0.0", UI_WINDOW_LEFT);
    ui_window_write_title(info, L"INFO", UI_WINDOW_CENTER);
    ui_window_write_title(prompt, L"Type your message", UI_WINDOW_LEFT);
    ui_window_write_title(contacts, L"CONTACTS", UI_WINDOW_CENTER);
    ui_window_write_title(settings, L"SETTINGS", UI_WINDOW_CENTER);

    return 0;
}

int main(void) {
    debug_init();

    int sl = -1;
    wchar_t wch;

    setlocale(LC_ALL, "");

    debug("Starting app");

    ui_init();
    debug("INIT PASSED");

    ui_define();

    ui_add_titiles();
    ui_stack_redraw(stack);

    //ui_window_draw(mgrwin, 1, 1);

    int ch_cnt = 0;
    int ch_type;

    while (1) {

        ch_type = get_wch(&wch);

        if (wch == 'a') {
            mvwaddstr(chat->content, 4, 0, "A DETECTED");
            ui_menu_remove(menu_comp, 2);
            ui_window_draw(settings, 1, 1);
            //sys_memory_crash("Something is strange with letter A");
            //sys_crash("Random system", "A detected, I hate it :(");
        }

        ui_stack_handle_input(stack, wch, ch_type == KEY_CODE_YES);

        if (wch == KEY_CTRL('X')) {
            break;
        }
        if (wch == KEY_RESIZE) {
            ui_define();
            ui_add_titiles();
            //ui_window_draw(mgrwin, 1, 1);
            ui_stack_redraw(stack);

            debug("RESIZE %dx%d %dx%d", getmaxx(stdscr), getmaxy(stdscr), mgrwin->cols, mgrwin->rows);
        }

        if (wch == KEY_CTRL('A')) {
            struct ui_window *popup = ui_window_new();
            ui_window_use_title(popup, 1);
            ui_window_use_border(popup, 1);
            ui_window_define(popup, (getmaxy(stdscr) - 20) / 2, (getmaxx(stdscr) - 80) / 2, 20, 80);
            ui_window_write_title(popup, L"Add new contact", UI_WINDOW_CENTER);
            ui_window_set_selected(popup, 1);
            
            ui_stack_push(stack, popup);
        }

        if (wch == KEY_CTRL('B')) {
            if (colors_on = !colors_on)
                ui_window_start_colors();
            else
                ui_window_stop_colors();
                
            ui_define();
            ui_add_titiles();
            ui_stack_redraw(stack);
        }
    }

    debug(">> DONE <<");

    ui_end();
    debug(">> UNDEFINED <<");
    endwin();
}