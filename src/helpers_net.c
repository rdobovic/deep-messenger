#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <helpers.h>

// Find free port on the system
// returns port on success and copies it to port_dest if not NULL
// on error it returns 0
uint16_t get_free_port(char *port_dest) {
    int sockfd;
    int free_port;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        return 0;
    }
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return 0;
    }

    if (getsockname(sockfd, (struct sockaddr*)&addr, &addr_len) < 0) {
        close(sockfd);
        return 0;
    }

    free_port = ntohs(addr.sin_port);
    close(sockfd);

    if (port_dest)
        snprintf(port_dest, MAX_PORT_STR_LEN, "%d", free_port);

    return free_port;
}