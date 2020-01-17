#include "../macros.h"
#include "peer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <unistd.h>
#include "../thread_pool/thread_pool.h"
#include "../net_utils/net_utils.h"

struct Peer * peer_new(int32_t ip, uint16_t port, int am_initiating) {
    struct Peer * p = NULL;

    p = malloc(sizeof(struct Peer));
    if (!p) {
        throw("peer failed to malloc");
    }

    p->socket = -1;
    p->port = port;
    p->addr.sin_family=AF_INET;
    p->addr.sin_port=net_utils.htons(p->port);
    p->addr.sin_addr.s_addr = ip;
    memset(p->addr.sin_zero, '\0', sizeof(p->addr.sin_zero));

    char * str_ip = inet_ntoa(p->addr.sin_addr);
    p->str_ip = strndup(str_ip, strlen(str_ip));
    if (!p->str_ip) {
        throw("peer failed to set str_ip");
    }

    p->am_initiating = am_initiating;
    p->am_choking = 1;
    p->am_interested = 0;
    p->peer_choking = 1;
    p->peer_interested = 0;

    p->status = PEER_UNCONNECTED;

    // log_info("got peer %s:%" PRIu16 "", str_ip, p->port);

    return p;
    error:
    return peer_free(p);
}

void peer_set_socket(struct Peer * p, int socket) {
    p->socket = socket;
    p->status = PEER_CONNECTED;
}

int peer_should_connect(struct Peer * p) {
    return (p->status == PEER_UNCONNECTED);
}

int peer_connect(struct Peer * p) {
    p->status == PEER_CONNECTING;

    if ((p->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        throw("cannot connect to user, socket failed :: %s", p->str_ip);
    }

    if (connect(p->socket, (struct sockaddr *)&p->addr, sizeof(struct sockaddr)) == -1) {
        throw("cannot connect to user, connect failed :: %s", p->str_ip);
    }

    log_info("connected to peer :: %s", p->str_ip);

    return EXIT_SUCCESS;

    error:
    if (p->socket > 0) {
        close(p->socket);
    }
    return EXIT_FAILURE;
}

int peer_run(int * cancel_flag, ...) {
    va_list args;
    va_start(args, cancel_flag);

    struct JobArg p_job_arg = va_arg(args, struct JobArg);
    struct Peer *p = (struct Peer *) p_job_arg.arg;

    log_info("running peer :: %s", p->str_ip);

    while (*cancel_flag != 1) {
        if (peer_should_connect(p) == 1) {
            // peer connect here
            sched_yield();
        }
        sched_yield();

        if (p->status == PEER_CONNECTED) {
            // peer handshake + extended handshake here
            sched_yield();
        }
        sched_yield();

        // if my peer has metadata and torrent requires metadata
        // request a metadata piece

        // if my peer
    }
}

struct Peer * peer_free(struct Peer * p) {
    if (p) {
        if(p->socket > 0) {
            close(p->socket);
            p->socket = -1;
        }
        if(p->str_ip) { free(p->str_ip); p->str_ip = NULL; };
        free(p);
        p = NULL;
    }
    return p;
}