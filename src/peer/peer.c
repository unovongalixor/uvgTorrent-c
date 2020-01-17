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
        goto error;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (net_utils.connect(p->socket, (struct sockaddr *)&p->addr, sizeof(struct sockaddr), &timeout) == -1) {
        goto error;
    }

    p->status == PEER_CONNECTED;

    return EXIT_SUCCESS;

    error:
    if (p->socket > 0) {
        close(p->socket);
    }
    return EXIT_FAILURE;
}

int peer_should_handshake(struct Peer * p) {
    return (p->status == PEER_CONNECTED);
}

int peer_handshake(struct Peer * p, int8_t info_hash_hex[20]) {
    p->status = PEER_HANDSHAKING;

    /* send handshake */


    /* receive handshake */

    /* compare infohash and check for metadata support */

    /* if metadata supported, do extended handshake */

    p->status = PEER_HANDSHAKED;
    return EXIT_SUCCESS;
}

int peer_run(int * cancel_flag, ...) {
    va_list args;
    va_start(args, cancel_flag);

    struct JobArg p_job_arg = va_arg(args, struct JobArg);
    struct Peer *p = (struct Peer *) p_job_arg.arg;

    struct JobArg info_hash_job_arg = va_arg(args, struct JobArg);
    int8_t (* info_hash) [20] = (int8_t (*) [20]) info_hash_job_arg.arg;
    int8_t info_hash_hex[20];
    memcpy(&info_hash_hex, info_hash, sizeof(info_hash_hex));

    while (*cancel_flag != 1) {
        if (peer_should_connect(p) == 1) {
            if (peer_connect(p) == EXIT_FAILURE) {
                p->status = PEER_UNCONNECTED;
            }
            sched_yield();
        }

        if (peer_should_handshake(p) == 1) {
            if (peer_handshake(p, info_hash_hex) == EXIT_FAILURE) {
                p->status = PEER_UNCONNECTED;
            }
            sched_yield();
        }

        /* wait 1 second */
        pthread_cond_t condition;
        pthread_mutex_t mutex;

        pthread_cond_init(&condition, NULL);
        pthread_mutex_init(&mutex, NULL);
        pthread_mutex_unlock(&mutex);

        struct timespec timeout_spec;
        clock_gettime(CLOCK_REALTIME, &timeout_spec);
        timeout_spec.tv_sec += 1;

        pthread_cond_timedwait(&condition, &mutex, &timeout_spec);

        pthread_cond_destroy(&condition);
        pthread_mutex_destroy(&mutex);
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