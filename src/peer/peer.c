#include "../macros.h"
#include "peer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "../deadline/deadline.h"
#include "../thread_pool/thread_pool.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"
#include "../bitfield/bitfield.h"
#include "../torrent/torrent_data.h"

#define METADATA_PIECE_SIZE 16000

struct Peer *peer_new(int32_t ip, uint16_t port) {
    struct Peer *p = NULL;

    p = malloc(sizeof(struct Peer));
    if (!p) {
        throw("peer failed to malloc");
    }

    p->socket = -1;
    p->port = port;
    p->addr.sin_family = AF_INET;
    p->addr.sin_port = net_utils.htons(p->port);
    p->addr.sin_addr.s_addr = net_utils.htonl(ip);
    memset(p->addr.sin_zero, 0x00, sizeof(p->addr.sin_zero));

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

    // log_info("got peer %s:%" PRIu16 "", str_ip, p->port);

    return p;
    error:
    return peer_free(p);
}

void peer_set_socket(struct Peer *p, int socket) {
    p->socket = socket;
    p->status = PEER_CONNECTED;
}

int peer_should_connect(struct Peer *p) {
    return (p->status == PEER_UNCONNECTED);
}

int peer_connect(struct Peer *p) {
    p->status == PEER_CONNECTING;

    if ((p->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        goto error;
    }
    struct timeval timeout;
    timeout.tv_sec = 1; /* 1 Secs Timeout */
    timeout.tv_usec = 0;
    setsockopt(p->socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &timeout, sizeof(struct timeval));
    setsockopt(p->socket, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *) &timeout, sizeof(struct timeval));

    timeout.tv_sec = 1;  /* 1 Secs Timeout */
    timeout.tv_usec = 0;
    if (net_utils.connect(p->socket, (struct sockaddr *) &p->addr, sizeof(struct sockaddr), &timeout) == -1) {
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

int peer_should_handshake(struct Peer *p) {
    return (p->status == PEER_CONNECTED);
}

int peer_handshake(struct Peer *p, int8_t info_hash_hex[20], _Atomic int *cancel_flag) {
    p->status = PEER_HANDSHAKING;

    /* send handshake */
    struct PEER_HANDSHAKE handshake_send;
    handshake_send.pstrlen = 19;
    char *pstr = "BitTorrent protocol";
    memcpy(&handshake_send.pstr, pstr, sizeof(handshake_send.pstr));
    memset(&handshake_send.reserved, 0x00, sizeof(handshake_send.reserved));
    handshake_send.reserved[5] = 0x10; // send reserved byte to signal availability of utmetadata
    memcpy(&handshake_send.info_hash, info_hash_hex, sizeof(int8_t[20]));
    char *peer_id = "UVG01234567891234567";
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
        struct PEER_EXTENSION *extension_send = malloc(extensions_send_size);
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
    log_info("peer handshaked :: %s:%i", p->str_ip, p->port);

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int peer_supports_ut_metadata(struct Peer *p) {
    return (p->utmetadata > 0 && p->status == PEER_HANDSHAKED);
}

int peer_request_metadata_piece(struct Peer *p, struct TorrentData ** torrent_metadata) {
    if ((*torrent_metadata)->initialized == 0) {
        /* initilize metadata_bitfield if needed */
        torrent_data_set_chunk_size(*torrent_metadata, 16000);
        torrent_data_set_data_size(*torrent_metadata, p->metadata_size);
    }

    int metadata_piece = torrent_data_claim_chunk(*torrent_metadata);
    if (metadata_piece != -1) {
        be_node_t *d = be_alloc(DICT);
        be_dict_add_num(d, "msg_type", 0);
        be_dict_add_num(d, "piece", metadata_piece);

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

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int peer_should_request_metadata(struct Peer *p, struct TorrentData ** torrent_metadata) {
    return ((*torrent_metadata)->needed == 1 && peer_supports_ut_metadata(p) == 1);
}

void *peer_read_message(struct Peer *p, _Atomic int *cancel_flag) {
    uint32_t network_ordered_msg_length = 0;

    size_t read_length = read(p->socket, &network_ordered_msg_length, sizeof(uint32_t));

    if (*cancel_flag == 1) {
        return NULL;
    }

    if (read_length == sizeof(uint32_t)) {
        uint32_t msg_length = net_utils.ntohl(network_ordered_msg_length);

        // read msg_id
        uint8_t msg_id = 0;
        read_length = read(p->socket, &msg_id, sizeof(uint8_t));
        if (read_length != sizeof(uint8_t)) {
            log_err("failed to read msg_id :: %s:%i", p->str_ip, p->port);
            peer_disconnect(p);
            return NULL;
        }

        if (*cancel_flag == 1) {
            return NULL;
        }

        // validate that we have a valid message, if not disconnect and throw a visible error
        if (is_valid_msg_id(msg_id) == EXIT_FAILURE) {
            log_err("got invalid msg_id %i :: %s:%i", (int) msg_id, p->str_ip, p->port);
            peer_disconnect(p);
            return NULL;
        }

        size_t buffer_size = sizeof(msg_length) + msg_length;
        void *buffer = malloc(buffer_size);
        if (buffer == NULL) {
            return NULL;
        }
        memset(buffer, 0x00, msg_length);

        // copy msg_len and msg_id into buffer for completeness
        memcpy(buffer, &network_ordered_msg_length, sizeof(network_ordered_msg_length));
        memcpy(buffer + sizeof(msg_length), &msg_id, sizeof(msg_id));

        uint32_t total_bytes_read = sizeof(msg_length) + sizeof(msg_id);
        uint32_t total_expected_bytes = buffer_size;

        /* read full message */
        while (total_bytes_read < total_expected_bytes) {
            size_t read_len = read(p->socket, buffer + total_bytes_read, total_expected_bytes - total_bytes_read);
            if (read_len < 0) {
                if (errno != ETIMEDOUT) {
                    peer_disconnect(p);
                }
                free(buffer);
                return NULL;
            }
            if (*cancel_flag == 1) {
                free(buffer);
                return NULL;
            }
            total_bytes_read += read_len;
        }

        return buffer;
    } else if (read_length != -1) {
        peer_disconnect(p);
        return NULL;
    }

    return NULL;
}

void get_msg_buffer_size(void *buffer, size_t *buffer_size) {
    uint32_t msg_length;
    get_msg_length(buffer, &msg_length);

    *buffer_size = sizeof(msg_length) + msg_length;
}

void get_msg_length(void *buffer, uint32_t *msg_length) {
    *msg_length = net_utils.ntohl(*((uint32_t *) buffer));
}

void get_msg_id(void *buffer, uint8_t *msg_id) {
    *msg_id = *((uint8_t * )(buffer + sizeof(uint32_t)));
}

int is_valid_msg_id(uint8_t msg_id) {
    for (int i = 0; i < sizeof(VALID_MSG_IDS); i++) {
        if (VALID_MSG_IDS[i] == msg_id) { return EXIT_SUCCESS; }
    }
    return EXIT_FAILURE;
}

int peer_should_read_message(struct Peer *p) {
    if (p->status == PEER_HANDSHAKED) {
        struct pollfd poll_set[1];
        memset(poll_set, 0x00, sizeof(poll_set));
        poll_set[0].fd = p->socket;
        poll_set[0].events = POLLIN;

        poll(poll_set, 1, 1);

        if (poll_set[0].revents & POLLIN) {
            return 1;
        }
    }

    return 0;
}

int peer_should_run(struct Peer * p, struct TorrentData ** torrent_metadata) {
    return (peer_should_connect(p) |
            peer_should_handshake(p) |
            peer_should_request_metadata(p, torrent_metadata) |
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

    if (peer_should_connect(p) == 1) {
        if (peer_connect(p) == EXIT_FAILURE) {
            peer_disconnect(p);
            goto error;
        }
        sched_yield();
    }

    if (peer_should_handshake(p) == 1) {
        if (peer_handshake(p, info_hash_hex, cancel_flag) == EXIT_FAILURE) {
            peer_disconnect(p);
            goto error;
        }
        sched_yield();
    }

    /* send my messages */
    if (peer_should_request_metadata(p, torrent_metadata) == 1) {
        /* try to claim and request a metadata piece */
        peer_request_metadata_piece(p, torrent_metadata);
        sched_yield();
    }

    /* receive messages */
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
                struct PEER_EXTENSION *peer_extension_response = (struct PEER_EXTENSION *) msg_buffer;
                size_t extenstion_msg_len = (buffer_size) - sizeof(struct PEER_EXTENSION);
                if (peer_extension_response->extended_msg_id == 0) {
                    /* decode response and extract ut_metadata and metadata_size */
                    size_t read_amount = 0;
                    be_node_t *d = be_decode((char *) &peer_extension_response->msg, extenstion_msg_len,
                                             &read_amount);
                    if (d == NULL) {
                        log_err("failed to perform extended handshake :: %s:%i", p->str_ip, p->port);
                        be_free(d);
                        free(msg_buffer);
                        peer_disconnect(p);
                        goto error;
                    }
                    be_node_t *m = be_dict_lookup(d, "m", NULL);
                    if (m == NULL) {
                        log_err("failed to perform extended handshake :: %s:%i", p->str_ip, p->port);
                        be_free(d);
                        free(msg_buffer);
                        peer_disconnect(p);
                        goto error;
                    }
                    uint32_t ut_metadata = (uint32_t) be_dict_lookup_num(m, "ut_metadata");
                    uint32_t metadata_size = (uint32_t) be_dict_lookup_num(d, "metadata_size");
                    be_free(d);
                    p->utmetadata = ut_metadata;
                    p->metadata_size = metadata_size;
                } else if (peer_extension_response->extended_msg_id == 1) {
                    log_info("GOT DATA ID %s :: %s:%i", (char *) &peer_extension_response->msg, p->str_ip,
                             p->port);

                    queue_push(metadata_queue, msg_buffer);

                    p->running = 0;
                    return EXIT_SUCCESS;

                    /*
                        size_t msg_size = 0;
                        be_node_t * msg = be_decode((char *) &peer_extension_response->msg, extenstion_msg_len, &msg_size);
                        if (msg == NULL) {
                            log_err("failed to perform extended handshake :: %s:%i", p->str_ip, p->port);
                            be_free(msg);
                            free(msg_buffer);
                            peer_disconnect(p);
                            continue;
                        }
                        uint64_t total_size = (uint64_t) be_dict_lookup_num(msg, "total_size");
                        log_info("total_size %" PRId64, total_size);
                        be_free(msg);

                        size_t metadata_read_size = 0;
                        be_node_t * info = be_decode((char *) &peer_extension_response->msg[msg_size], extenstion_msg_len - msg_size, &metadata_read_size);
                        if (info == NULL) {
                            log_err("failed to perform extended handshake :: %s:%i", p->str_ip, p->port);
                            be_free(info);
                            free(msg_buffer);
                            peer_disconnect(p);
                            continue;
                        }

                        char * name = be_dict_lookup_cstr(info, "name");
                        log_info("name %s", name);

                        be_free(info);
                         */
                }
            }

            free(msg_buffer);
            sched_yield();
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

void peer_disconnect(struct Peer *p) {
    if (p->socket > 0) {
        close(p->socket);
        p->socket = -1;
    }
    p->status = PEER_UNCONNECTED;
}
