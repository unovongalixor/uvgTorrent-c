#include "../macros.h"
#include "peer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct Peer * peer_new(int32_t ip, uint16_t port) {
    struct Peer * p = NULL;

    p = malloc(sizeof(struct Peer));
    if (!p) {
        throw("peer failed to malloc");
    }

    p->port = port;
    p->addr.s_addr = ip;
    char * str_ip = inet_ntoa(p->addr);
    p->str_ip = strndup(str_ip, strlen(str_ip));
    if (!p->str_ip) {
        throw("peer failed to set str_ip");
    }

    // log_info("got peer %s:%" PRIu16 "", str_ip, p->port);

    return p;
    error:
    return peer_free(p);
}

int peer_run(int * cancel_flag, ...) {
    log_info("running peer..");
}

struct Peer * peer_free(struct Peer * p) {
    if (p) {
        if(p->str_ip) { free(p->str_ip); p->str_ip = NULL; };
        free(p);
        p = NULL;
    }
    return p;
}