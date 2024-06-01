#include <helpers.h>
#include <debug.h>

int main() {
    int port;
    char port_str[MAX_PORT_STR_LEN];
    debug_set_fp(stdout);

    port_str[0] = '\0';

    port = get_free_port(port_str);
    debug("Found free port %d [%s]", port, port_str);
    return 0;
}