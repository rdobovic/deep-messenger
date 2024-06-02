#include <wchar.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <debug.h>
#include <ncurses.h>
#include <ui_stack.h>
#include <helpers.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <sys_crash.h>
#include <prot_main.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <app.h>

// Handle data from stdin
static void app_stdin_read_cb(evutil_socket_t fd, short what, void *arg);
// Handle window resize signal
static void app_winch_handle_cb(evutil_socket_t fd, short what, void *arg);
// Handle app shutdown
static void app_sigint_handle_cb(evutil_socket_t fd, short what, void *arg);

// Accept incomming connection
static void app_accept_connection(struct evconnlistener *listener, 
    evutil_socket_t sock, struct sockaddr *addr, int len, void *ptr);

// Init libevent and standard input event
void app_event_init(struct app_data *app) {
    int rc;
    struct event *sigint_ev;
    struct evconnlistener *listener;
    struct addrinfo hints, *servinfo, *aip;

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

    // Find address and start connection listener
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rc = getaddrinfo("127.0.0.1", app->cf.app_local_port, &hints, &servinfo)) != 0) {
        sys_crash("Network", "Failed to get address to bind, getaddrinfo: %s", gai_strerror(rc));
    }

    for (aip = servinfo; aip != NULL; aip = aip->ai_next) {
        listener = evconnlistener_new_bind(app->base, app_accept_connection, 
            app, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1, aip->ai_addr, aip->ai_addrlen);

        if (listener) break;
    }
    freeaddrinfo(servinfo);

    if (aip == NULL)
        sys_crash("Network", "Failed to bind connection listener");
}

// Start event loop
void app_event_start(struct app_data *app) {
    event_base_dispatch(app->base);
}

// Stop event loop
void app_event_end(struct app_data *app) {
    event_base_loopbreak(app->base);
}

// Handle data from stdin
static void app_stdin_read_cb(evutil_socket_t fd, short what, void *arg) {
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

// Handle window resize signal
static void app_winch_handle_cb(evutil_socket_t fd, short what, void *arg) {
    struct app_data *app = arg;

    // For some reason ncurses won't detect new window dimensions
    // unless I call endwin first :(
    endwin();
    refresh();
    app_ui_define(app);
    app_ui_add_titles(app);
    ui_stack_redraw(app->ui.stack);
}

// Handle app shutdown
static void app_sigint_handle_cb(evutil_socket_t fd, short what, void *arg) {
    struct app_data *app = arg;

    app_end(app);
}

// Accept incomming connection
static void app_accept_connection(struct evconnlistener *listener, 
    evutil_socket_t sock, struct sockaddr *addr, int len, void *ptr
) {
    struct event_base *base;
    struct bufferevent *bev;
    struct prot_main *pmain;
    struct app_data *app = ptr;

    debug("Got connection");

    base = evconnlistener_get_base(listener);
    bev = bufferevent_socket_new(base, sock, BEV_OPT_CLOSE_ON_FREE);
    pmain = prot_main_new(base, app->db);

    pmain->mode = app->cf.is_mailbox ? PROT_MODE_MAILBOX : PROT_MODE_MAILBOX;
    prot_main_assign(pmain, bev);
}