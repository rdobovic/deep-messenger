#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <socks5.h>
#include <sys/socket.h>
#include <debug.h>
#include <event2/util.h>

#define LOCALHOST        0x7F000001
#define ONION_ADDRESS    "g7kfkvigtyx45az27obwydfq3zrxfwl77so3n3tqv22cw3qvz6cuv4qd.onion"
#define HTTP_SERVER_PORT 80

void log_this(int sev, const char *msg)
{
    debug("%s", msg);
}

void read_cb(struct bufferevent *bev, void *ctx)
{
    int n;
    char buf[1024];
    struct evbuffer *in;
    in = bufferevent_get_input(bev);

    debug("GOT SOMETHING");
    while ((n = evbuffer_remove(in, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, n, stdout);
    }
}

void write_cb(struct bufferevent *bev, void *ctx)
{
    debug("WRITING SUCCESS buffer len now = %d", evbuffer_get_length(bufferevent_get_output(bev)));
}

void cb(struct bufferevent *buffev, enum socks5_errors err, void *attr)
{
    struct evbuffer *out;

    const char request[] = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

    if (!buffev) {
        debug("FAILED TO CONNECT: %s", socks5_error_string(err));
        return;
    }

    bufferevent_setcb(buffev, read_cb, NULL, NULL, NULL);
    out = bufferevent_get_output(buffev);
    evbuffer_add(out, request, sizeof(request) - 1);
    bufferevent_enable(buffev, EV_READ|EV_WRITE);

    debug("Something happened: CODE(%d) BEV(%s)", err, !!buffev ? "SOMETHING" : "NULL");
    debug("OUT BUFFER LEN = %d", evbuffer_get_length(out));
}

int main(void)
{
    struct event_base *base;
    struct bufferevent *buffev;
    struct sockaddr_in sin;
    struct evbuffer *buff;

    uint8_t method_select[] = { 0x05, 0x01, 0x00 };

    debug_set_fp(stdout);

    event_set_log_callback(log_this);

    base = event_base_new();

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(LOCALHOST);
    sin.sin_port = htons(9050);
    //sin.sin_port = htons(5000);

    buffev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    //buffev = bufferevent_socket_new(base, -1, 0);

    if (bufferevent_socket_connect(buffev, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        debug("Failed to connect");
    }

    //bufferevent_setcb(buffev, read_cb, write_cb, NULL, NULL);
    //buff = bufferevent_get_output(buffev);

    //evbuffer_add(buff, method_select, sizeof(method_select));

    socks5_connect_onion(buffev, ONION_ADDRESS, HTTP_SERVER_PORT, cb, NULL);
    bufferevent_enable(buffev, EV_READ|EV_WRITE);

    debug("BEV setup done !");

    debug("HERE");

    event_base_dispatch(base);

    //debug("%s", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
}