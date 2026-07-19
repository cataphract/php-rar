#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    int resolver_error;
    int found_loopback = 0;

    if (res_init() != 0) {
        fputs("res_init failed\n", stderr);
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    resolver_error = getaddrinfo("localhost", NULL, &hints, &addresses);
    if (resolver_error != 0) {
        fprintf(stderr, "resolver lookup failed after res_init: %s\n",
                gai_strerror(resolver_error));
        return 1;
    }
    for (address = addresses; address != NULL; address = address->ai_next) {
        if (address->ai_family == AF_INET &&
            ntohl(((struct sockaddr_in *)address->ai_addr)->sin_addr.s_addr) ==
                INADDR_LOOPBACK) {
            found_loopback = 1;
            break;
        }
        if (address->ai_family == AF_INET6 &&
            IN6_IS_ADDR_LOOPBACK(
                &((struct sockaddr_in6 *)address->ai_addr)->sin6_addr)) {
            found_loopback = 1;
            break;
        }
    }
    freeaddrinfo(addresses);
    if (!found_loopback) {
        fputs("resolver returned no loopback address for localhost\n", stderr);
        return 1;
    }

    puts("res_init ok");
    return 0;
}
