#include "../macros.h"
#include "peer.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"
#include "../deadline/deadline.h"

#define REQUEST_MSG_QUEUE_LENGTH 10

int peer_should_read_message(struct Peer *p) {
    return (p->status == PEER_HANDSHAKE_COMPLETE) && (buffered_socket_can_read(p->socket));
}

void *peer_read_message(struct Peer *p, _Atomic int *cancel_flag) {
    size_t read_length;

    if(p->network_ordered_msg_length_loaded == 0) {
        uint32_t network_ordered_msg_length = 0;
        read_length = buffered_socket_read(p->socket, &network_ordered_msg_length, sizeof(uint32_t));

        if (read_length == sizeof(uint32_t)) {
            p->network_ordered_msg_length = network_ordered_msg_length;
            p->network_ordered_msg_length_loaded = 1;
        } else if (read_length == -1) {
            log_err("failed to read msg_id :: %s:%i", p->str_ip, p->port);
            peer_disconnect(p);
            return NULL;
        } else {
            return NULL;
        }
    }

    if(p->msg_id_loaded == 0) {
        uint8_t msg_id = 0;
        read_length = buffered_socket_read(p->socket, &msg_id, sizeof(uint8_t));
        if (read_length == sizeof(uint8_t)) {
            p->msg_id = msg_id;
            p->msg_id_loaded = 1;
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
    if(msg_length == 0) {
        return NULL;
    }

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
    if(total_expected_bytes > 0) {
        size_t read_len = buffered_socket_read(p->socket, buffer + total_bytes_read, total_expected_bytes);
        if (read_len == -1) {
            log_err("failed to read full msg : %s:%i", p->str_ip, p->port);
            peer_disconnect(p);
            free(buffer);
            return NULL;
        } else if (read_len == 0) {
            free(buffer);
            return NULL;
        }
    }

    p->network_ordered_msg_length = 0;
    p->network_ordered_msg_length_loaded = 0;
    p->msg_id = 0;
    p->msg_id_loaded = 0;
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
int peer_should_send_keepalive(struct Peer *p) {
    return (p->last_message_sent < now() - ((60 * 1000) * 2)); // send keepalive every 2 minutes
}

int peer_send_keepalive(struct Peer *p) {
    uint32_t length = 0;
    if (buffered_socket_write(p->socket, &length, sizeof(uint32_t)) != sizeof(uint32_t)) {
        throw("failed to write keepalive msg :: %s:%i", p->str_ip, p->port);
    }

    log_info("peer sent keepalive :: %s:%i", p->str_ip, p->port);

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int peer_should_timeout(struct Peer *p) {
    return (p->last_message_received < now() - ((60 * 1000) * 2) + 500); // disconnect users with no new messages for 2.5 seconds
}


void peer_schedule_status_refresh(struct Peer *p) {
    p->last_status = p->current_status;
    p->current_status = now();
}

int peer_should_update_status(struct Peer *p, struct TorrentData * torrent_data) {
    return (p->last_status < p->current_status && torrent_data->needed == 1 && p->msg_bitfield_sent == 1 && p->status == PEER_HANDSHAKE_COMPLETE && p->peer_bitfield != NULL);
}

void peer_update_status(struct Peer *p, struct TorrentData * torrent_data) {
    peer_update_choke(p, torrent_data);
    peer_update_interest(p, torrent_data);
}

void peer_update_choke(struct Peer *p, struct TorrentData * torrent_data) {
    // check if peer is seeding, if so, choke
    int seeding = 1;
    for(int i = 0; i < p->peer_bitfield->bit_count; i++) {
        if(bitfield_get_bit(p->peer_bitfield, i) != 1) {
            seeding = 0;
            break;
        }
    }

    if (seeding == 1 && p->am_choking == 0) {
        // choke seeding clients
        peer_send_msg_choke(p);
    } else if (seeding == 0 && p->am_choking == 1) {
        peer_send_msg_unchoke(p);
    }

}

void peer_update_interest(struct Peer *p, struct TorrentData * torrent_data) {
    int peer_has_interesting_pieces = 0;
    for(int i = 0; i < p->peer_bitfield->bit_count; i++) {
        int have = 0;
        if (torrent_data->needed == 1) {
            have = bitfield_get_bit(torrent_data->completed, i);
        }
        if (have == 0 && bitfield_get_bit(p->peer_bitfield, i) == 1) {
            peer_has_interesting_pieces = 1;
            break;
        }
    }

    if(p->am_interested == 0 && peer_has_interesting_pieces == 1) {
        peer_send_msg_interested(p);
    } else if (p->am_interested == 1 && peer_has_interesting_pieces == 0) {
        peer_send_msg_not_interested(p);
    }
}

int peer_send_msg_choke(struct Peer *p) {
    // log_info("peer choked :: %s:%i", p->str_ip, p->port);
    p->am_choking = 1;

    struct PEER_MSG_BASIC choke_msg = {
            .length=net_utils.htonl((uint32_t) sizeof(struct PEER_MSG_BASIC) - sizeof(uint32_t)),
            .msg_id=MSG_CHOKE,
    };

    if (buffered_socket_write(p->socket, &choke_msg, sizeof(struct PEER_MSG_BASIC)) != sizeof(struct PEER_MSG_BASIC)) {
        throw("failed to write choke msg :: %s:%i", p->str_ip, p->port);
    }

    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int peer_handle_msg_choke(struct Peer *p, void * msg_buffer) {
    log_info("peer choked :: %s:%i", p->str_ip, p->port);
    p->peer_choking = 1;
    free(msg_buffer);
}

int peer_send_msg_unchoke(struct Peer *p) {
    p->am_choking = 0;

    struct PEER_MSG_BASIC unchoke_msg = {
            .length=net_utils.htonl((uint32_t) sizeof(struct PEER_MSG_BASIC) - sizeof(uint32_t)),
            .msg_id=MSG_UNCHOKE,
    };

    if (buffered_socket_write(p->socket, &unchoke_msg, sizeof(struct PEER_MSG_BASIC)) != sizeof(struct PEER_MSG_BASIC)) {
        throw("failed to write unchoke msg :: %s:%i", p->str_ip, p->port);
    }

    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int peer_handle_msg_unchoke(struct Peer *p, void * msg_buffer) {
    log_info("peer unchoked :: %s:%i", p->str_ip, p->port);
    p->peer_choking = 0;
    free(msg_buffer);
}

int peer_send_msg_interested(struct Peer *p) {
    log_info("peer interested :: %s:%i", p->str_ip, p->port);
    p->am_interested = 1;

    struct PEER_MSG_BASIC interested_msg = {
            .length=net_utils.htonl((uint32_t) sizeof(struct PEER_MSG_BASIC) - sizeof(uint32_t)),
            .msg_id=MSG_INTERESTED
    };

    if (buffered_socket_write(p->socket, &interested_msg, sizeof(struct PEER_MSG_BASIC)) != sizeof(struct PEER_MSG_BASIC)) {
        throw("failed to write interested msg :: %s:%i", p->str_ip, p->port);
    }

    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int peer_handle_msg_interested(struct Peer *p, void * msg_buffer) {
    p->peer_interested = 1;
    free(msg_buffer);
}

int peer_send_msg_not_interested(struct Peer *p) {
    // log_info("peer not interested :: %s:%i", p->str_ip, p->port);
    p->am_interested = 0;

    struct PEER_MSG_BASIC not_interested_msg = {
            .length=net_utils.htonl((uint32_t) sizeof(struct PEER_MSG_BASIC) - sizeof(uint32_t)),
            .msg_id=MSG_NOT_INTERESTED
    };

    if (buffered_socket_write(p->socket, &not_interested_msg, sizeof(struct PEER_MSG_BASIC)) != sizeof(struct PEER_MSG_BASIC)) {
        throw("failed to write interested msg :: %s:%i", p->str_ip, p->port);
    }

    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int peer_handle_msg_not_interested(struct Peer *p, void * msg_buffer) {
    p->peer_interested = 0;
    free(msg_buffer);
}

int peer_should_send_msg_have(struct Peer *p) {
    return (queue_get_count(p->progress_queue) > 0 && p->status == PEER_HANDSHAKE_COMPLETE);
}

int peer_send_msg_have(struct Peer *p, struct TorrentData * torrent_data) {
    while(queue_get_count(p->progress_queue) > 0) {
        int * piece_id = (int *) queue_pop(p->progress_queue);

        struct PEER_MSG_HAVE peer_msg_have = {
                .length=net_utils.htonl((uint32_t) sizeof(struct PEER_MSG_HAVE) - sizeof(uint32_t)),
                .msg_id=MSG_HAVE,
                .piece_id=net_utils.htonl(*piece_id)
        };

        if (buffered_socket_write(p->socket, &peer_msg_have, sizeof(struct PEER_MSG_HAVE)) != sizeof(struct PEER_MSG_HAVE)) {
            free(piece_id);
            throw("failed to write have msg :: %s:%i", p->str_ip, p->port);
        }

        free(piece_id);
    }

    peer_schedule_status_refresh(p);
    
    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int peer_handle_msg_have(struct Peer *p, void * msg_buffer, struct TorrentData * torrent_data) {
    free(msg_buffer);

    peer_schedule_status_refresh(p);
}

int peer_should_send_msg_bitfield(struct Peer *p, struct TorrentData * torrent_data) {
    return (p->status == PEER_HANDSHAKE_COMPLETE && p->msg_bitfield_sent == 0 && torrent_data->needed == 1);
}

int peer_send_msg_bitfield(struct Peer *p, struct TorrentData * torrent_data) {
    p->msg_bitfield_sent = 1;

    // log_info("peer sending bitfield :: %s:%i", p->str_ip, p->port);

    // prepare bitfield with chunks set to number of pieces
    struct Bitfield * msg_bitfield = bitfield_new(torrent_data->piece_count, 0, 0x00);

    if(p->peer_bitfield) {
        if(p->peer_bitfield->bytes_count < msg_bitfield->bytes_count) {
            log_info("resizing partial bitfield :: %s:%i", p->str_ip, p->port);
            // check to make sure our bitfield is the correct size, some client may send partial bitfields
            struct Bitfield * new_peer_bitfield = bitfield_new(torrent_data->piece_count, 0, 0x00);
            memcpy(&new_peer_bitfield->bytes, &p->peer_bitfield->bytes, p->peer_bitfield->bytes_count);
            bitfield_free(p->peer_bitfield);
            p->peer_bitfield = new_peer_bitfield;
        }
    }

    // loop through bytes in torrent_metadata->completed and set in bitfield
    for(int i = 0; i < torrent_data->piece_count; i++) {
        int bit_value = torrent_data_is_piece_complete(torrent_data, i);
        bitfield_set_bit(msg_bitfield, i, bit_value);
    }

    size_t msg_size = sizeof(struct PEER_MSG_BITFIELD) + msg_bitfield->bytes_count;
    struct PEER_MSG_BITFIELD * peer_bitfield_msg = malloc(msg_size);
    if(peer_bitfield_msg == NULL) {
        throw("couldn't malloc peer bitfield msg :: %s:%i", p->str_ip, p->port);
    }
    peer_bitfield_msg->length = net_utils.htonl((uint32_t) msg_size - sizeof(int32_t));
    peer_bitfield_msg->msg_id = MSG_BITFIELD;
    memcpy(&peer_bitfield_msg->bitfield, &msg_bitfield->bytes, msg_bitfield->bytes_count);

    // send bitfield
    if (buffered_socket_write(p->socket, peer_bitfield_msg, msg_size) != msg_size) {
        goto error;
    }

    free(peer_bitfield_msg);
    bitfield_free(msg_bitfield);

    return EXIT_SUCCESS;
    error:

    if (peer_bitfield_msg != NULL) {
        free(peer_bitfield_msg);
    }
    if (bitfield_free != NULL) {
        bitfield_free(msg_bitfield);
    }

    return EXIT_FAILURE;
}

int peer_handle_msg_bitfield(struct Peer *p, void * msg_buffer, struct TorrentData * torrent_data) {
    size_t buffer_size;
    get_msg_buffer_size(msg_buffer, (size_t * ) & buffer_size);

    int chunk_count = (buffer_size - sizeof(struct PEER_MSG_BITFIELD)) * BITS_PER_INT;
    if(torrent_data->needed == 1) {
        // if we already have torrent metadata loaded and we know how large our bitfield should be, use that size
        chunk_count = torrent_data->piece_count;
    }

    struct PEER_MSG_BITFIELD * bitfield_msg = (struct PEER_MSG_BITFIELD *) msg_buffer;

    if (p->peer_bitfield == NULL) {
        p->peer_bitfield = bitfield_new(chunk_count, 0, 0x00);
    }

    memcpy(&p->peer_bitfield->bytes, &bitfield_msg->bitfield, p->peer_bitfield->bytes_count);
    free(msg_buffer);

    peer_schedule_status_refresh(p);
}


int peer_should_send_msg_request(struct Peer *p, struct TorrentData * torrent_data) {
    return(p->status == PEER_HANDSHAKE_COMPLETE && torrent_data->needed == 1 && p->pending_request_msgs < REQUEST_MSG_QUEUE_LENGTH && p->peer_choking == 0);
}

int peer_send_msg_request(struct Peer *p, struct TorrentData * torrent_data) {
    // build bitfield of chunks we're interested in
    struct Bitfield * interested = bitfield_new(torrent_data->chunk_count, 0, 0xFF);

    for(int i = 0; i < torrent_data->piece_count; i++) {
        int interested_in_piece = bitfield_get_bit(p->peer_bitfield, i);
        int chunks_per_piece = torrent_data->piece_size / torrent_data->chunk_size;
        int uints_per_piece = chunks_per_piece / BITS_PER_INT;
        for (int x = 0; x < uints_per_piece; x++) {
            if((i*uints_per_piece)+x < interested->bytes_count) {
                if (interested_in_piece == 0) {
                    interested->bytes[(i*uints_per_piece)+x] = 0x00;
                } else if (interested_in_piece == 1) {
                    interested->bytes[(i*uints_per_piece)+x] = 0xFF;
                }
            }
        }
    }

    while(p->pending_request_msgs < REQUEST_MSG_QUEUE_LENGTH) {
        // lock a chunk
        int chunk_id = torrent_data_claim_chunk(torrent_data, interested, 5);

        if (chunk_id != -1) {
            struct ChunkInfo chunk_info;
            torrent_data_get_chunk_info(torrent_data, chunk_id, &chunk_info);

            struct PieceInfo piece_info;
            torrent_data_get_piece_info(torrent_data, chunk_info.piece_id, &piece_info);

            // make the request
            struct PEER_MSG_REQUEST msg_request = {
                    .length=net_utils.htonl((uint32_t) sizeof(struct PEER_MSG_REQUEST) - sizeof(uint32_t)),
                    .msg_id=MSG_REQUEST,
                    .index=net_utils.htonl(piece_info.piece_id),
                    .begin=net_utils.htonl(chunk_info.chunk_offset - piece_info.piece_offset),
                    .chunk_length=net_utils.htonl(chunk_info.chunk_size)
            };

            if (buffered_socket_write(p->socket, &msg_request, sizeof(struct PEER_MSG_REQUEST)) !=
                sizeof(struct PEER_MSG_REQUEST)) {

                bitfield_free(interested);
                goto error;
            }

            // increment pending_request_msgs
            p->pending_request_msgs++;
        } else {
            break;
        }
    }

    bitfield_free(interested);
    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int peer_handle_msg_request(struct Peer *p, void * msg_buffer) {
    log_warn("got request :: %s:%i", p->str_ip, p->port);
    free(msg_buffer);
}

int peer_handle_msg_piece(struct Peer *p, void * msg_buffer, struct Queue * data_queue) {
    queue_push(data_queue, msg_buffer);
    log_info("got piece :: %s:%i", p->str_ip, p->port);
    p->pending_request_msgs--;
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

    struct PEER_MSG_EXTENSION *peer_extension_response = (struct PEER_MSG_EXTENSION *) msg_buffer;
    size_t extenstion_msg_len = (buffer_size) - sizeof(struct PEER_MSG_EXTENSION);
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