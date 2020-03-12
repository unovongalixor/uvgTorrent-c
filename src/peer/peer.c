#include "../macros.h"
#include "peer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "../thread_pool/thread_pool.h"
#include "../net_utils/net_utils.h"

struct Peer *peer_new(int32_t ip, uint16_t port) {
    struct Peer *p = NULL;

    p = malloc(sizeof(struct Peer));
    if (!p) {
        throw("peer failed to malloc");
    }

    p->port = port;
    p->addr.sin_family = AF_INET;
    p->addr.sin_port = net_utils.htons(p->port);
    p->addr.sin_addr.s_addr = net_utils.htonl(ip);
    memset(p->addr.sin_zero, 0x00, sizeof(p->addr.sin_zero));
    p->socket = NULL;

    char *str_ip = inet_ntoa(p->addr.sin_addr);
    p->str_ip = strndup(str_ip, strlen(str_ip));
    if (!p->str_ip) {
        throw("peer failed to set str_ip");
    }

    p->utmetadata = 0;
    p->metadata_size = 0;
    p->am_choking = 1;
    p->am_interested = 0;
    p->peer_choking = 1;
    p->peer_interested = 0;

    p->status = PEER_UNCONNECTED;
    p->running = 0;

    p->network_ordered_msg_length = 0;
    p->msg_id = 0;

    // log_info("got peer %s:%" PRIu16 "", str_ip, p->port);

    return p;
    error:
    return peer_free(p);
}

int peer_should_handle_network_buffers(struct Peer * p) {
    if(p->socket == NULL) {
        return 0;
    }
    return buffered_socket_can_network_write(p->socket) | buffered_socket_can_network_read(p->socket);
}

int peer_handle_network_buffers(struct Peer * p) {
    if(buffered_socket_can_network_write(p->socket)) {
        buffered_socket_network_write(p->socket);
    }
    if(buffered_socket_can_network_read(p->socket)) {
        if(buffered_socket_network_read(p->socket) == 0) {
            if (errno != EINPROGRESS) {
                peer_disconnect(p);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

int peer_should_run(struct Peer * p, struct TorrentData ** torrent_metadata) {
    return (peer_should_connect(p) |
            peer_should_send_handshake(p) |
            peer_should_handle_handshake(p) |
            peer_should_request_metadata(p, torrent_metadata) |
            peer_should_handle_network_buffers(p) |
            peer_should_read_message(p)) & p->running == 0;
}

int peer_run(_Atomic int *cancel_flag, ...) {
    va_list args;
    va_start(args, cancel_flag);

    struct JobArg p_job_arg = va_arg(args, struct JobArg);
    struct Peer *p = (struct Peer *) p_job_arg.arg;

    struct JobArg info_hash_job_arg = va_arg(args, struct JobArg);
    int8_t(*info_hash)[20] = (int8_t (*)[20]) info_hash_job_arg.arg;
    int8_t info_hash_hex[20];
    memcpy(&info_hash_hex, info_hash, sizeof(info_hash_hex));

    struct JobArg metadata_job_arg = va_arg(args, struct JobArg);
    struct TorrentData ** torrent_metadata = (struct TorrentData **) metadata_job_arg.arg;

    struct JobArg metadata_queue_job_arg = va_arg(args, struct JobArg);
    struct Queue * metadata_queue = (struct Queue *) metadata_queue_job_arg.arg;

    /* connect */
    if (peer_should_connect(p) == 1) {
        if (peer_connect(p) == EXIT_FAILURE) {
            peer_disconnect(p);
            goto error;
        }
        sched_yield();
    }

    /* handshake */
    if (peer_should_send_handshake(p) == 1) {
        if (peer_send_handshake(p, info_hash_hex, cancel_flag) == EXIT_FAILURE) {
            peer_disconnect(p);
            goto error;
        }
        sched_yield();
    }

    if (peer_should_handle_handshake(p) == 1) {
        if (peer_handle_handshake(p, info_hash_hex, torrent_metadata, cancel_flag) == EXIT_FAILURE) {
            peer_disconnect(p);
            goto error;
        }
    }

    /* write messages to buffered tcp socket */
    if (peer_should_request_metadata(p, torrent_metadata) == 1) {
        /* try to claim and request a metadata piece */
        peer_request_metadata_piece(p, torrent_metadata);
    }

    /* handle network, write buffered messages to peer
     * and read any available data into the read buffer */
    if(peer_should_handle_network_buffers(p)) {
        if(peer_handle_network_buffers(p) == EXIT_FAILURE){
            goto error;
        }
    }

    /* read incoming messages */
    if (peer_should_read_message(p) == 1) {
        void *msg_buffer = peer_read_message(p, cancel_flag);
        if (msg_buffer != NULL) {
            uint32_t msg_length;
            uint8_t msg_id;
            size_t buffer_size;

            get_msg_length(msg_buffer, (uint32_t * ) & msg_length);
            get_msg_id(msg_buffer, (uint8_t * ) & msg_id);
            get_msg_buffer_size(msg_buffer, (size_t * ) & buffer_size);

            if (msg_id == MSG_EXTENSION) {
                if(peer_handle_extension_msg(p, msg_buffer, torrent_metadata, metadata_queue) == EXIT_FAILURE) {
                    free(msg_buffer);
                }
            } else {
                free(msg_buffer);
            }
        }
    }

    p->running = 0;
    return EXIT_SUCCESS;
    error:
    p->running = 0;
    return EXIT_FAILURE;
}

struct Peer *peer_free(struct Peer *p) {
    if (p) {
        peer_disconnect(p);
        if (p->str_ip) {
            free(p->str_ip);
            p->str_ip = NULL;
        };
        free(p);
        p = NULL;
    }
    return p;
}
