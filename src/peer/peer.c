#include "../macros.h"
#include "peer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "../thread_pool/thread_pool.h"
#include "../net_utils/net_utils.h"
#include "../bitfield/bitfield.h"
#include "../deadline/deadline.h"

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

    p->ut_metadata = 0;
    p->ut_metadata_requested = NULL;
    p->ut_metadata_size = 0;

    p->running = 0;
    p->am_choking = 1;
    p->am_interested = 0;
    p->peer_choking = 1;
    p->peer_interested = 0;
    p->peer_bitfield = NULL;
    p->progress_queue = queue_new();

    p->status = PEER_UNCONNECTED;
    p->reconnect_deadline = now();

    p->network_ordered_msg_length = 0;
    p->network_ordered_msg_length_loaded = 0;
    p->msg_id = 0;
    p->msg_id_loaded = 0;

    p->msg_bitfield_sent = 0;
    p->pending_request_count = 0;

    return p;
    error:
    return peer_free(p);
}

int peer_should_handle_network_buffers(struct Peer * p) {
    if(p->socket == NULL || p->socket->socket == -1) {
        return 0;
    }
    int can_network_write = buffered_socket_can_network_write(p->socket);
    int can_network_read = buffered_socket_can_network_read(p->socket);
    int peer_hanged_up = (p->status >= PEER_CONNECTED && buffered_socket_has_hungup(p->socket));
    return  can_network_write |
            can_network_read |
            peer_hanged_up;
}

int peer_handle_network_buffers(struct Peer * p) {
    if(buffered_socket_has_hungup(p->socket) == 1) {
        goto error;
    }
    if(buffered_socket_can_network_write(p->socket)) {
        if(buffered_socket_network_write(p->socket) == -1) {
            goto error;
        }
    }
    if(buffered_socket_can_network_read(p->socket)) {
        int result = buffered_socket_network_read(p->socket);
        if(result == -1) {
            if (errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK) {
                goto error;
            }
        }
    }
    return EXIT_SUCCESS;
    error:
    peer_disconnect(p, __FILE__, __LINE__);
    return EXIT_FAILURE;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
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
    struct TorrentData * torrent_metadata = (struct TorrentData *) metadata_job_arg.arg;

    struct JobArg data_job_arg = va_arg(args, struct JobArg);
    struct TorrentData * torrent_data = (struct TorrentData *) data_job_arg.arg;

    struct JobArg metadata_queue_job_arg = va_arg(args, struct JobArg);
    struct Queue * metadata_queue = (struct Queue *) metadata_queue_job_arg.arg;

    struct JobArg data_queue_job_arg = va_arg(args, struct JobArg);
    struct Queue * data_queue = (struct Queue *) data_queue_job_arg.arg;

    /* connect */
    if (peer_should_connect(p) == 1) {
        if (peer_connect(p) == EXIT_FAILURE) {
            peer_disconnect(p, __FILE__, __LINE__);
            goto error;
        }
        sched_yield();
    }

    /* handle network, write buffered messages to peer
     * and read any available data into the read buffer */
    if(peer_should_handle_network_buffers(p)) {
        if(peer_handle_network_buffers(p) == EXIT_FAILURE){
            goto error;
        }
    }

    /* handshake */
    if (peer_should_send_handshake(p) == 1) {
        if (peer_send_handshake(p, info_hash_hex, cancel_flag) == EXIT_FAILURE) {
            peer_disconnect(p, __FILE__, __LINE__);
            goto error;
        }
        sched_yield();
    }

    if (peer_should_handle_handshake(p) == 1) {
        if (peer_handle_handshake(p, info_hash_hex, torrent_metadata, cancel_flag) == EXIT_FAILURE) {
            peer_disconnect(p, __FILE__, __LINE__);
            goto error;
        }
    }

    /* write messages to buffered tcp socket */
    if(peer_should_send_msg_have(p) == 1) {
        peer_send_msg_have(p, torrent_data);
    }

    if(peer_should_send_msg_bitfield(p, torrent_data) == 1) {
        peer_send_msg_bitfield(p, torrent_data);
    }

    if (peer_should_send_ut_metadata_request(p, torrent_metadata) == 1) {
        /* try to claim and request a metadata piece */
        peer_send_ut_metadata_request(p, torrent_metadata);
    }

    if (peer_should_send_msg_request(p, torrent_data) == 1) {
        peer_send_msg_request(p, torrent_data);
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

            switch (msg_id) {
                case MSG_CHOKE:
                    peer_handle_msg_choke(p, msg_buffer);
                    break;

                case MSG_UNCHOKE:
                    peer_handle_msg_unchoke(p, msg_buffer);
                break;

                case MSG_INTERESTED:
                    peer_handle_msg_interested(p, msg_buffer);
                break;

                case MSG_NOT_INTERESTED:
                    peer_handle_msg_not_interested(p, msg_buffer);
                break;

                case MSG_HAVE:
                    peer_handle_msg_have(p, msg_buffer, torrent_data);
                break;

                case MSG_BITFIELD:
                    peer_handle_msg_bitfield(p, msg_buffer, torrent_data);
                break;

                case MSG_REQUEST:
                    peer_handle_msg_request(p, msg_buffer);
                break;

                case MSG_PIECE:
                    peer_handle_msg_piece(p, msg_buffer, data_queue);
                break;

                case MSG_CANCEL:
                    peer_handle_msg_cancel(p, msg_buffer);
                break;

                case MSG_PORT:
                    peer_handle_msg_port(p, msg_buffer);
                break;

                case MSG_EXTENSION:
                    peer_handle_msg_extension(p, msg_buffer, torrent_metadata, metadata_queue);
                break;

                default:
                    log_err("got unknown msg id %i :: %s:%i", msg_id, p->str_ip, p->port);
                    free(msg_buffer);
            }
        }
    }

    if(peer_should_send_keepalive(p) == 1) {
        peer_send_keepalive(p);
    }

    if(peer_should_timeout(p) == 1) {
        peer_disconnect(p, __FILE__, __LINE__);
    }

    /* handle network, write buffered messages to peer
     * and read any available data into the read buffer */
    if(peer_should_handle_network_buffers(p)) {
        if(peer_handle_network_buffers(p) == EXIT_FAILURE){
            goto error;
        }
    }

    p->running = 0;
    return EXIT_SUCCESS;
    error:
    p->running = 0;
    return EXIT_FAILURE;
}
#pragma clang diagnostic pop

struct Peer *peer_free(struct Peer *p) {
    if (p) {
        peer_disconnect(p, __FILE__, __LINE__);
        if (p->str_ip) {
            free(p->str_ip);
            p->str_ip = NULL;
        };
        if (p->ut_metadata_requested) {
            bitfield_free(p->ut_metadata_requested);
            p->ut_metadata_requested = NULL;
        }
        if(p->peer_bitfield) {
            bitfield_free(p->peer_bitfield);
            p->peer_bitfield = NULL;
        }
        if(p->progress_queue) {
            while(queue_get_count(p->progress_queue) > 0) {
                int * i = (int *) queue_pop(p->progress_queue);
                free(i);
            }
            queue_free(p->progress_queue);
            p->progress_queue = NULL;
        }
        free(p);
        p = NULL;
    }
    return p;
}
