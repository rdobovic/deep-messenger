#include <wchar.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <debug.h>
#include <ncurses.h>
#include <ui_stack.h>
#include <helpers.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <sys_crash.h>

#include <app.h>

// Handle data from stdin
void app_stdin_read_cb(evutil_socket_t fd, short what, void *arg);
// Handle window resize signal
void app_winch_handle_cb(evutil_socket_t fd, short what, void *arg);
// Handle app shutdown
void app_sigint_handle_cb(evutil_socket_t fd, short what, void *arg);

// Init libevent and standard input event
void app_event_init(struct app_data *app) {
    struct event *sigint_ev;

    app->base = event_base_new();
    event_base_priority_init(app->base, APP_EV_PRIORITY_COUNT);

    // If this is client start UI input handleing
    if (!app->cf.is_mailbox) {
        struct event *stdin_ev;
        struct event *winch_ev;

        stdin_ev = event_new(app->base, STDIN_FILENO, EV_READ|EV_PERSIST, app_stdin_read_cb, app);
        event_add(stdin_ev, NULL);
        event_priority_set(stdin_ev, APP_EV_PRIORITY_USER);

        winch_ev = evsignal_new(app->base, SIGWINCH, app_winch_handle_cb, app);
        evsignal_add(winch_ev, NULL);
        event_priority_set(winch_ev, APP_EV_PRIORITY_USER);
    }

    sigint_ev = evsignal_new(app->base, SIGINT, app_sigint_handle_cb, app);
    evsignal_add(sigint_ev, NULL);
    event_priority_set(sigint_ev, APP_EV_PRIORITY_PRIMARY);

    if (get_free_port(app->cf.app_local_port) == 0) {
        sys_crash("Network", "Failed to get free port for app to listen on");
    }
}

// Start event loop
void app_event_start(struct app_data *app) {
    event_base_dispatch(app->base);
}

void app_stdin_read_cb(evutil_socket_t fd, short what, void *arg) {
    int ch_type;
    wchar_t wch;
    struct app_data *app = arg;

    while ((ch_type = get_wch(&wch)) != -1) {
        if (wch == KEY_CTRL('X')) {
            app_end(app);
            continue;
        }
        
        ui_stack_handle_input(app->ui.stack, wch, ch_type == KEY_CODE_YES);
    }
}

void app_event_end(struct app_data *app) {
    event_base_loopbreak(app->base);
}

void app_winch_handle_cb(evutil_socket_t fd, short what, void *arg) {
    struct app_data *app = arg;

    // For some reason ncurses won't detect new window dimensions
    // unless I call endwin first :(
    endwin();
    refresh();
    app_ui_define(app);
    app_ui_add_titles(app);
    ui_stack_redraw(app->ui.stack);
}

void app_sigint_handle_cb(evutil_socket_t fd, short what, void *arg) {
    struct app_data *app = arg;

    app_end(app);
}