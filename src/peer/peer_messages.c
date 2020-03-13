#include "../macros.h"
#include "peer.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"
#include "../deadline/deadline.h"

int peer_should_read_message(struct Peer *p) {
    return (p->status == PEER_HANDSHAKE_COMPLETE) & (buffered_socket_can_read(p->socket));
}

void *peer_read_message(struct Peer *p, _Atomic int *cancel_flag) {
    size_t read_length;

    if(p->network_ordered_msg_length == 0) {
        uint32_t network_ordered_msg_length = 0;
        read_length = buffered_socket_read(p->socket, &network_ordered_msg_length, sizeof(uint32_t));

        if (read_length == sizeof(uint32_t)) {
            p->network_ordered_msg_length = network_ordered_msg_length;
        } else if (read_length == -1) {
            log_err("failed to read msg_id :: %s:%i", p->str_ip, p->port);
            peer_disconnect(p);
            return NULL;
        } else {
            return NULL;
        }
    }

    if(p->msg_id == 0) {
        uint8_t msg_id = 0;
        read_length = buffered_socket_read(p->socket, &msg_id, sizeof(uint8_t));
        if (read_length == sizeof(uint8_t)) {
            p->msg_id = msg_id;
        } else if (read_length == -1) {
            log_err("failed to read msg_id :: %s:%i", p->str_ip, p->port);
            peer_disconnect(p);
            return NULL;
        } else {
            return NULL;
        }
    }

    if (is_valid_msg_id(p->msg_id) == EXIT_FAILURE) {
        log_err("got invalid msg_id %i :: %s:%i", (int) p->msg_id, p->str_ip, p->port);
        peer_disconnect(p);
        return NULL;
    }

    uint32_t msg_length = net_utils.ntohl(p->network_ordered_msg_length);

    size_t buffer_size = sizeof(msg_length) + msg_length;
    void *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        log_err("failed to allocate msg buffer : %s:%i", p->str_ip, p->port);
        peer_disconnect(p);
        return NULL;
    }
    memset(buffer, 0x00, msg_length);

    // copy msg_len and msg_id into buffer for completeness
    memcpy(buffer, &p->network_ordered_msg_length, sizeof(p->network_ordered_msg_length));
    memcpy(buffer + sizeof(msg_length), &p->msg_id, sizeof(p->msg_id));

    uint32_t total_bytes_read = sizeof(msg_length) + sizeof(p->msg_id);
    uint32_t total_expected_bytes = buffer_size - total_bytes_read;

    /* read full message */
    size_t read_len = buffered_socket_read(p->socket, buffer + total_bytes_read, total_expected_bytes);
    if(read_len == -1) {
        log_err("failed to read full msg : %s:%i", p->str_ip, p->port);
        peer_disconnect(p);
        free(buffer);
        return NULL;
    } else if(read_len == 0) {
        free(buffer);
        return NULL;
    }

    p->network_ordered_msg_length = 0;
    p->msg_id = 0;
    return buffer;
}

/* msg reading functions */
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

/* msg handles */
int peer_handle_msg_choke(struct Peer *p, void * msg_buffer) {
    p->peer_choking = 1;
    free(msg_buffer);
}

int peer_handle_msg_unchoke(struct Peer *p, void * msg_buffer) {
    p->peer_choking = 0;
    free(msg_buffer);
}

int peer_handle_msg_interested(struct Peer *p, void * msg_buffer) {
    p->peer_interested = 1;
    free(msg_buffer);
}

int peer_handle_msg_not_interested(struct Peer *p, void * msg_buffer) {
    p->peer_interested = 0;
    free(msg_buffer);
}

int peer_handle_msg_have(struct Peer *p, void * msg_buffer) {
    free(msg_buffer);
}

int peer_should_send_msg_bitfield(struct Peer *p, struct TorrentData * torrent_data) {
    return (p->status == PEER_HANDSHAKE_COMPLETE && p->msg_bitfield_deadline < now() && torrent_data->needed == 1);
}

int peer_send_msg_bitfield(struct Peer *p, struct TorrentData * torrent_data) {
    p->msg_bitfield_deadline = now() + ((60 * 1000) * 10);

    log_info("peer sending bitfield :: %s:%i", p->str_ip, p->port);

    // prepare bitfield with chunks set to number of pieces

    // loop through bytes in torrent_metadata->completed and set in bitfield

    // send bitfield

    return EXIT_SUCCESS;
}

int peer_handle_msg_bitfield(struct Peer *p, void * msg_buffer) {
    free(msg_buffer);
}

int peer_handle_msg_request(struct Peer *p, void * msg_buffer) {
    free(msg_buffer);
}

int peer_handle_msg_piece(struct Peer *p, void * msg_buffer) {
    free(msg_buffer);
}

int peer_handle_msg_cancel(struct Peer *p, void * msg_buffer) {
    free(msg_buffer);
}

int peer_handle_msg_port(struct Peer *p, void * msg_buffer) {
    free(msg_buffer);
}

int peer_handle_msg_extension(struct Peer * p, void * msg_buffer, struct TorrentData * torrent_metadata, struct Queue * metadata_queue) {
    uint32_t msg_length;
    uint8_t msg_id;
    size_t buffer_size;

    get_msg_length(msg_buffer, (uint32_t * ) & msg_length);
    get_msg_id(msg_buffer, (uint8_t * ) & msg_id);
    get_msg_buffer_size(msg_buffer, (size_t * ) & buffer_size);

    struct PEER_EXTENSION *peer_extension_response = (struct PEER_EXTENSION *) msg_buffer;
    size_t extenstion_msg_len = (buffer_size) - sizeof(struct PEER_EXTENSION);
    if (peer_extension_response->extended_msg_id == 0) {
        if(peer_handle_ut_metadata_handshake(p, msg_buffer) == EXIT_FAILURE) {
            goto error;
        } else {
            free(msg_buffer);
        }
    } else if (peer_extension_response->extended_msg_id == UT_METADATA_ID) {
        // decode message
        size_t read_amount = 0;
        be_node_t *d = be_decode((char *) &peer_extension_response->msg, extenstion_msg_len, &read_amount);
        if (d == NULL) {
            log_err("failed to decode ut_metadata message :: %s:%i", p->str_ip, p->port);
            be_free(d);
            goto error;
        }
        uint64_t msg_type = (uint64_t) be_dict_lookup_num(d, "msg_type");
        uint64_t chunk_id = (uint64_t) be_dict_lookup_num(d, "piece");

        if(msg_type == 0) {
            peer_handle_ut_metadata_request(p, chunk_id, torrent_metadata);
            free(msg_buffer);
        } else if(msg_type == 1) {
            peer_handle_ut_metadata_data(p, msg_buffer, metadata_queue);
        } else if (msg_type == 2) {
            peer_handle_ut_metadata_reject(p);
            free(msg_buffer);
        } else {
            free(msg_buffer);
        }

        be_free(d);
    }

    return EXIT_SUCCESS;

    error:
    peer_disconnect(p);
    free(msg_buffer);
    return EXIT_FAILURE;
}