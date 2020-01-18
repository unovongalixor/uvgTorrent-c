#include "../macros.h"
#include "peer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include "../thread_pool/thread_pool.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"

struct Peer * peer_new(int32_t ip, uint16_t port) {
    struct Peer * p = NULL;

    p = malloc(sizeof(struct Peer));
    if (!p) {
        throw("peer failed to malloc");
    }

    p->socket = -1;
    p->port = port;
    p->addr.sin_family=AF_INET;
    p->addr.sin_port=net_utils.htons(p->port);
    p->addr.sin_addr.s_addr = net_utils.htonl(ip);
    memset(p->addr.sin_zero, '\0', sizeof(p->addr.sin_zero));

    char * str_ip = inet_ntoa(p->addr.sin_addr);
    p->str_ip = strndup(str_ip, strlen(str_ip));
    if (!p->str_ip) {
        throw("peer failed to set str_ip");
    }

    p->utmetadata = 0;
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
    timeout.tv_sec = 2;  /* 2 Secs Timeout */
    timeout.tv_usec = 0;

    setsockopt(p->socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout,sizeof(struct timeval));
    setsockopt(p->socket, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&timeout,sizeof(struct timeval));

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (net_utils.connect(p->socket, (struct sockaddr *)&p->addr, sizeof(struct sockaddr), &timeout) == -1) {
        goto error;
    }

    p->status = PEER_CONNECTED;

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

int peer_handshake(struct Peer * p, int8_t info_hash_hex[20], int * cancel_flag) {
    p->status = PEER_HANDSHAKING;

    /* send handshake */
    struct PEER_HANDSHAKE handshake_send;
    handshake_send.pstrlen = 19;
    char * pstr = "BitTorrent protocol";
    memcpy(&handshake_send.pstr, pstr, sizeof(handshake_send.pstr));
    memset(&handshake_send.reserved, 0x00, sizeof(handshake_send.reserved));
    handshake_send.reserved[5] = 0x10; // send reserved byte to signal availability of utmetadata
    memcpy(&handshake_send.info_hash, info_hash_hex, sizeof(int8_t[20]));
    char * peer_id = "UVG01234567891234567";
    memcpy(&handshake_send.peer_id, peer_id, sizeof(handshake_send.peer_id));

    if (write(p->socket, &handshake_send, sizeof(struct PEER_HANDSHAKE)) != sizeof(struct PEER_HANDSHAKE)) {
        goto error;
    }
    /* receive handshake */
    struct PEER_HANDSHAKE handshake_receive;
    memset(&handshake_receive, 0x00, sizeof(handshake_receive));

    if (read(p->socket, &handshake_receive, sizeof(handshake_receive)) != sizeof(struct PEER_HANDSHAKE)) {
        goto error;
    }

    /* compare infohash and check for metadata support */
    for (int i = 0; i < 8; i++) {
        if (handshake_receive.info_hash[i] != handshake_send.info_hash[i]) {
            throw("mismatched infohash");
        }
    }

    log_info("peer handshaked %s:%i", p->str_ip, p->port);

    p->status = PEER_HANDSHAKED;

    /* if metadata supported, do extended handshake */
    if (handshake_receive.reserved[5] == 0x10) {
        // include metadata size here if we have that information available
        be_node_t *d = be_alloc(DICT);
        be_node_t *m = be_alloc(DICT);
        be_dict_add_num(m, "ut_metadata", 1);
        be_dict_add(d, "m", m);

        char extended_handshake_message[1000] = {'\0'};
        size_t extended_handshake_message_len = be_encode(d, (char *) &extended_handshake_message, 1000);
        be_free(d);

        size_t extensions_send_size = sizeof(struct PEER_EXTENSION) + extended_handshake_message_len;
        struct PEER_EXTENSION * extension_send = malloc(extensions_send_size);
        extension_send->length = net_utils.htonl(extensions_send_size - sizeof(int32_t));
        extension_send->msg_id = 20;
        extension_send->extended_msg_id = 0; // extended handshake id
        memcpy(&extension_send->msg, &extended_handshake_message, extended_handshake_message_len);

        if (write(p->socket, extension_send, extensions_send_size) != extensions_send_size) {
            free(extension_send);
            goto error;
        } else {
            free(extension_send);
        }

        /* receive extended handshake */
        uint8_t buffer[5000] = {'\0'};
        struct PEER_EXTENSION * extension_receive = (void *) &buffer;

        /* get message length */
        if (read(p->socket, &buffer, sizeof(extension_receive->length)) != sizeof(extension_receive->length)) {
            goto error;
        }
        extension_receive->length = net_utils.ntohl(extension_receive->length);
        uint32_t total_bytes_read = sizeof(extension_receive->length);
        uint32_t total_expected_bytes = extension_receive->length + sizeof(extension_receive->length);

        /* read full message */
        while (total_bytes_read < total_expected_bytes) {
            size_t read_len = read(p->socket, &buffer[total_bytes_read], total_expected_bytes - total_bytes_read);
            if (read_len < 0) {
                goto error;
            }
            if (*cancel_flag == 1) {
                goto error;
            }
            total_bytes_read += read_len;
        }

        /* decode response and extract ut_metadata and metadata_size */
        size_t msg_len = (total_expected_bytes) - sizeof(struct PEER_EXTENSION);
        size_t read_amount = 0;

        d = be_decode((char *) &extension_receive->msg, msg_len, &read_amount);
        if (d == NULL) {
            be_free(d);
            throw("failed to decode extended handshake :: %s:%i", p->str_ip, p->port);
        }
        m = be_dict_lookup(d, "m", NULL);
        if(m == NULL) {
            be_free(d);
            throw("no m in extended handshake response :: %s:%i", p->str_ip, p->port);
        }
        uint32_t ut_metadata = (uint32_t) be_dict_lookup_num(m, "ut_metadata");
        uint32_t metadata_size = (uint32_t) be_dict_lookup_num(d, "metadata_size");
        be_free(d);

        p->utmetadata = ut_metadata;
        /* decode extended msg */
        log_info("peer extended handshaked %"PRId32" %"PRId32" :: %s:%i", ut_metadata, metadata_size, p->str_ip, p->port);
    }
    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
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
            if (peer_handshake(p, info_hash_hex, cancel_flag) == EXIT_FAILURE) {
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