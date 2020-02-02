#include "../macros.h"
#include "peer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include "../deadline/deadline.h"
#include "../thread_pool/thread_pool.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"
#include "../bitfield/bitfield.h"

#define METADATA_PIECE_SIZE 16000

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
    memset(p->addr.sin_zero, 0x00, sizeof(p->addr.sin_zero));

    char * str_ip = inet_ntoa(p->addr.sin_addr);
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

    p->claimed_bitfield_resource_deadline = 0;
    p->claimed_bitfield_resource = NULL;
    p->claimed_bitfield_resource_bit = -1;

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

int peer_handshake(struct Peer * p, int8_t info_hash_hex[20], _Atomic int * cancel_flag) {
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
    }

    p->status = PEER_HANDSHAKED;
    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int peer_supports_ut_metadata(struct Peer * p) {
    return (p->utmetadata > 0 && p->status == PEER_HANDSHAKED);
}

int peer_request_metadata_piece(struct Peer * p, struct Bitfield ** metadata_pieces) {
    if (p->claimed_bitfield_resource_bit == -1) {
        /* initilize metadata_bitfield if needed */
        if (*metadata_pieces == NULL) {
            size_t total_pieces = (p->metadata_size + (METADATA_PIECE_SIZE - 1)) / METADATA_PIECE_SIZE;
            *metadata_pieces = bitfield_new(total_pieces);
        }

        if (peer_claim_resource(p, *metadata_pieces) == EXIT_SUCCESS) {
            be_node_t *d = be_alloc(DICT);
            be_dict_add_num(d, "msg_type", 0);
            be_dict_add_num(d, "piece", p->claimed_bitfield_resource_bit);

            char metadata_request_message[1000] = {'\0'};
            size_t metadata_request_message_len = be_encode(d, (char *) &metadata_request_message, 1000);
            be_free(d);

            size_t metadata_send_size = sizeof(struct PEER_EXTENSION) + metadata_request_message_len;
            struct PEER_EXTENSION *metadata_send = malloc(metadata_send_size);
            metadata_send->length = net_utils.htonl(metadata_send_size - sizeof(int32_t));
            metadata_send->msg_id = 20;
            metadata_send->extended_msg_id = p->utmetadata;
            memcpy(&metadata_send->msg, &metadata_request_message, metadata_request_message_len);

            if (write(p->socket, metadata_send, metadata_send_size) != metadata_send_size) {
                free(metadata_send);
                goto error;
            } else {
                free(metadata_send);
            }
        } else {
            goto error;
        }
    } else {
        goto error;
    }

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int peer_should_request_metadata(struct Peer * p, int * needs_metadata) {
    return (*needs_metadata == 1 && peer_supports_ut_metadata(p) == 1);
}

int peer_read_message(struct Peer * p, struct Queue * metadata_queue, struct Queue * pieces_queue) {

}

int peer_run(_Atomic int * cancel_flag, ...) {
    va_list args;
    va_start(args, cancel_flag);

    struct JobArg p_job_arg = va_arg(args, struct JobArg);
    struct Peer *p = (struct Peer *) p_job_arg.arg;

    struct JobArg info_hash_job_arg = va_arg(args, struct JobArg);
    int8_t (* info_hash) [20] = (int8_t (*) [20]) info_hash_job_arg.arg;
    int8_t info_hash_hex[20];
    memcpy(&info_hash_hex, info_hash, sizeof(info_hash_hex));

    struct JobArg needs_metadata_job_arg = va_arg(args, struct JobArg);
    int * needs_metadata = (int *) needs_metadata_job_arg.arg;

    struct JobArg metadata_pieces_job_arg = va_arg(args, struct JobArg);
    struct Bitfield ** metadata_pieces = (struct Bitfield **) metadata_pieces_job_arg.arg;

    while (*cancel_flag != 1) {
        if (peer_should_connect(p) == 1) {
            if (peer_connect(p) == EXIT_FAILURE) {
                peer_disconnect(p);
            }
            sched_yield();
        }

        if (peer_should_handshake(p) == 1) {
            if (peer_handshake(p, info_hash_hex, cancel_flag) == EXIT_FAILURE) {
                peer_disconnect(p);
            }
            sched_yield();
        }

        /* send my messages */
        if(peer_should_request_metadata(p, needs_metadata) == 1) {
            /* try to claim and request a metadata piece */
            if(peer_request_metadata_piece(p, metadata_pieces) == EXIT_SUCCESS){
                log_info("GOT PIECE %i :: %s:%i", p->claimed_bitfield_resource_bit, p->str_ip, p->port);
            }
        }

        /* release any resources that need releasing */
        if(p->claimed_bitfield_resource_bit > -1) {
            /* if we have a claim on a piece of metadata check for a timeout on it */
            if(p->claimed_bitfield_resource_deadline < now()) {
                peer_release_resource(p);
            }
        }

        /* receive messages */
        if (p->status == PEER_HANDSHAKED) {
            uint32_t msg_length = 0;

            size_t read_length = read(p->socket, &msg_length, sizeof(uint32_t));
            if (read_length == sizeof(uint32_t)) {
                msg_length = net_utils.ntohl(msg_length);
                uint8_t buffer[sizeof(msg_length) + msg_length];
                memset(&buffer, 0x00, msg_length);

                uint32_t total_bytes_read = sizeof(msg_length);
                uint32_t total_expected_bytes = sizeof(msg_length) + msg_length;

                /* read full message */
                while (total_bytes_read < total_expected_bytes) {
                    size_t read_len = read(p->socket, &buffer[total_bytes_read], total_expected_bytes - total_bytes_read);
                    if (read_len < 0) {
                        peer_disconnect(p);
                    }
                    if (*cancel_flag == 1) {
                        return EXIT_FAILURE;
                    }
                    total_bytes_read += read_len;
                }

                uint8_t * msg_id = &buffer[sizeof(msg_length)];
                if (*msg_id == 20) {
                    struct PEER_EXTENSION * peer_extension_response = (struct PEER_EXTENSION *) &buffer;
                    if (peer_extension_response->extended_msg_id == 0) {
                        /* decode response and extract ut_metadata and metadata_size */
                        size_t msg_len = (total_expected_bytes) - sizeof(struct PEER_EXTENSION);
                        size_t read_amount = 0;

                        be_node_t * d = be_decode((char *) &peer_extension_response->msg, msg_len, &read_amount);
                        if (d == NULL) {
                            be_free(d);
                        }
                        be_node_t *m = be_dict_lookup(d, "m", NULL);
                        if(m == NULL) {
                            be_free(d);
                        }
                        uint32_t ut_metadata = (uint32_t) be_dict_lookup_num(m, "ut_metadata");
                        uint32_t metadata_size = (uint32_t) be_dict_lookup_num(d, "metadata_size");
                        be_free(d);

                        p->utmetadata = ut_metadata;
                        p->metadata_size = metadata_size;
                        /* decode extended msg */
                        log_info("peer extended handshaked %"PRId32" %"PRId32" :: %s:%i", ut_metadata, metadata_size, p->str_ip, p->port);
                    } else {
                        log_info("GOT MESSAGE ID %i %i :: %s:%i", (int) *msg_id, total_expected_bytes, p->str_ip, p->port);
                    }
                }
            } else if (read_length != -1){
                peer_disconnect(p);
            }
        } else {
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
}

int peer_claim_resource(struct Peer * p, struct Bitfield * shared_resource) {
    p->claimed_bitfield_resource_bit = -1;
    bitfield_lock(shared_resource);
    for (int i = 0; i < (shared_resource->bit_count); i++) {
        int bit_val = bitfield_get_bit(shared_resource, i);
        if (bit_val == 0) {
            bitfield_set_bit(shared_resource, i, 1);
            p->claimed_bitfield_resource_deadline = now() + 2000; // 2 second resource claim
            p->claimed_bitfield_resource_bit = i;
            p->claimed_bitfield_resource = shared_resource;

            bitfield_unlock(shared_resource);
            return EXIT_SUCCESS;
        }
    }

    bitfield_unlock(shared_resource);
    return EXIT_FAILURE;
}

void peer_release_resource(struct Peer * p) {
    if(p->claimed_bitfield_resource_bit > -1) {
        bitfield_lock(p->claimed_bitfield_resource);
        bitfield_set_bit(p->claimed_bitfield_resource, p->claimed_bitfield_resource_bit, 0);
        bitfield_unlock(p->claimed_bitfield_resource);
        p->claimed_bitfield_resource_deadline = 0;
        p->claimed_bitfield_resource_bit = -1;
        p->claimed_bitfield_resource = NULL;
    }
}

struct Peer * peer_free(struct Peer * p) {
    if (p) {
        peer_disconnect(p);
        if(p->str_ip) { free(p->str_ip); p->str_ip = NULL; };
        free(p);
        p = NULL;
    }
    return p;
}

void peer_disconnect(struct Peer * p) {
    if(p->socket > 0) {
        close(p->socket);
        p->socket = -1;
    }
    p->status = PEER_UNCONNECTED;
}