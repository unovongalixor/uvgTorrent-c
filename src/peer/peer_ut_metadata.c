#include "../macros.h"
#include "peer.h"
#include "../net_utils/net_utils.h"
#include "../bencode/bencode.h"
#include "../bitfield/bitfield.h"

int peer_supports_ut_metadata(struct Peer *p) {
    return (p->ut_metadata > 0 && p->status == PEER_HANDSHAKE_COMPLETE);
}

int peer_handle_ut_metadata_handshake(struct Peer * p, void * msg_buffer) {
    uint32_t msg_length;
    uint8_t msg_id;
    size_t buffer_size;

    get_msg_length(msg_buffer, (uint32_t * ) & msg_length);
    get_msg_id(msg_buffer, (uint8_t * ) & msg_id);
    get_msg_buffer_size(msg_buffer, (size_t * ) & buffer_size);

    struct PEER_EXTENSION *peer_extension_response = (struct PEER_EXTENSION *) msg_buffer;
    size_t extenstion_msg_len = (buffer_size) - sizeof(struct PEER_EXTENSION);

    /* decode response and extract ut_metadata and metadata_size */
    size_t read_amount = 0;
    be_node_t *d = be_decode((char *) &peer_extension_response->msg, extenstion_msg_len,
                             &read_amount);
    if (d == NULL) {
        log_err("failed to perform extended handshake :: %s:%i", p->str_ip, p->port);
        goto error;
    }
    be_node_t *m = be_dict_lookup(d, "m", NULL);
    if (m == NULL) {
        log_err("failed to perform extended handshake :: %s:%i", p->str_ip, p->port);
        goto error;
    }
    uint32_t ut_metadata = (uint32_t) be_dict_lookup_num(m, "ut_metadata");
    uint32_t ut_metadata_size = (uint32_t) be_dict_lookup_num(d, "metadata_size");

    p->ut_metadata = ut_metadata;
    p->ut_metadata_size = ut_metadata_size;

    be_free(d);
    return EXIT_SUCCESS;
    error:
    be_free(d);
    return EXIT_FAILURE;
}

int peer_handle_ut_metadata_request(struct Peer * p, uint64_t chunk_id, struct TorrentData * torrent_metadata) {
    // check if metadata is available
    if(torrent_metadata->is_completed == 1) {
        // get chunk info
        struct ChunkInfo chunk_info;
        torrent_data_get_chunk_info(torrent_metadata, chunk_id, &chunk_info);

        log_info("sending metadata msg %"PRId64" :: %s:%i", chunk_id, p->str_ip, p->port);
        log_info("chunk_offset :: %zu", chunk_info.chunk_offset);
        log_info("chunk_size :: %zu", chunk_info.chunk_size);

        // prepare buffer
        uint8_t buffer[65535] = {0x00}; // 65535 == max tcp packet size

        // prepare message
        be_node_t *m = be_alloc(DICT);
        be_dict_add_num(m, "msg_type", 1);
        be_dict_add_num(m, "piece", chunk_id);
        be_dict_add_num(m, "total_size", (int) torrent_metadata->data_size);

        size_t encoded_size = be_encode(m, (char *) &buffer, sizeof(buffer));
        be_free(m);

        // copy metadata to buffer
        torrent_data_read_data(torrent_metadata, &buffer[encoded_size], chunk_info.chunk_offset, chunk_info.chunk_size);

        // send metadata
        size_t msg_size = encoded_size + chunk_info.chunk_size;

        struct PEER_EXTENSION * peer_extension = malloc(sizeof(struct PEER_EXTENSION) + msg_size);
        peer_extension->length = net_utils.htonl(sizeof(struct PEER_EXTENSION) + msg_size - sizeof(uint32_t));
        peer_extension->msg_id = 20;
        peer_extension->extended_msg_id = p->ut_metadata;
        memcpy(&peer_extension->msg, &buffer, msg_size);

        if (buffered_socket_write(p->socket, peer_extension, sizeof(struct PEER_EXTENSION) + msg_size) != sizeof(struct PEER_EXTENSION) + msg_size) {
            free(peer_extension);
            goto error;
        }
        free(peer_extension);
    } else {
        log_info("sending reject msg %"PRId64" :: %s:%i", chunk_id, p->str_ip, p->port);
        // send reject msg
    }

    return EXIT_SUCCESS;

    error:

    return EXIT_FAILURE;
}

int peer_handle_ut_metadata_data(struct Peer * p, void * msg_buffer, struct Queue * metadata_queue) {
    queue_push(metadata_queue, msg_buffer);
}

int peer_handle_ut_metadata_reject(struct Peer * p) {
    log_warn("peer rejected ut_metadata :: %s:%i", p->str_ip, p->port);
    p->ut_metadata = 0;
}




int peer_send_ut_metadata_request(struct Peer *p, struct TorrentData * torrent_metadata) {
    if(p->ut_metadata_requested == NULL) {
        size_t chunk_count = (p->ut_metadata_size + (METADATA_CHUNK_SIZE - 1)) / METADATA_CHUNK_SIZE;
        p->ut_metadata_requested = bitfield_new((int) chunk_count, 1, 0xFF);
    }

    if (torrent_metadata->initialized == 0) {
        /* initilize metadata_bitfield if needed */
        torrent_data_set_piece_size(torrent_metadata, METADATA_PIECE_SIZE);
        torrent_data_set_chunk_size(torrent_metadata, METADATA_CHUNK_SIZE);
        log_info("got metadata size %i", p->ut_metadata_size);

        torrent_data_add_file(torrent_metadata, "/metadata.bencode", p->ut_metadata_size);
        torrent_data_set_data_size(torrent_metadata, p->ut_metadata_size);
    }

    int metadata_piece = torrent_data_claim_chunk(torrent_metadata, p->ut_metadata_requested);
    if (metadata_piece != -1) {
        log_info("requesting chunk %i :: %s:%i", metadata_piece, p->str_ip, p->port);

        bitfield_set_bit(p->ut_metadata_requested, metadata_piece, 0);

        be_node_t *d = be_alloc(DICT);
        be_dict_add_num(d, "msg_type", 0);
        be_dict_add_num(d, "piece", metadata_piece);

        char metadata_request_message[1000] = {0x00};
        size_t metadata_request_message_len = be_encode(d, (char *) &metadata_request_message, 1000);
        be_free(d);

        size_t metadata_send_size = sizeof(struct PEER_EXTENSION) + metadata_request_message_len;
        struct PEER_EXTENSION *metadata_send = malloc(metadata_send_size);
        metadata_send->length = net_utils.htonl(metadata_send_size - sizeof(int32_t));
        metadata_send->msg_id = MSG_EXTENSION;
        metadata_send->extended_msg_id = p->ut_metadata;
        memcpy(&metadata_send->msg, &metadata_request_message, metadata_request_message_len);

        if (buffered_socket_write(p->socket, metadata_send, metadata_send_size) != metadata_send_size) {
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

int peer_should_send_ut_metadata_request(struct Peer *p, struct TorrentData * torrent_metadata) {
    return (torrent_metadata->needed == 1 &&
            peer_supports_ut_metadata(p) == 1 &&
            p->status == PEER_HANDSHAKE_COMPLETE);
}
