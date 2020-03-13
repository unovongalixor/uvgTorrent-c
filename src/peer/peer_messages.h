//
// Created by vongalixor on 3/12/20.
//

#ifndef UVGTORRENT_C_PEER_MESSAGES_H
#define UVGTORRENT_C_PEER_MESSAGES_H

/**
 * @brief PEER WIRE PROTOCOL
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
 */
enum PeerMsgIDs {
    MSG_CHOKE = 0,
    MSG_UNCHOKE = 1,
    MSG_INTERESTED = 2,
    MSG_NOT_INTERESTED = 3,
    MSG_HAVE = 4,
    MSG_BITFIELD = 5,
    MSG_REQUEST = 6,
    MSG_PIECE = 7,
    MSG_CANCEL = 8,
    MSG_PORT = 9,
    MSG_EXTENSION = 20,
};

static uint8_t VALID_MSG_IDS[11] = {
        MSG_CHOKE,
        MSG_UNCHOKE,
        MSG_INTERESTED,
        MSG_NOT_INTERESTED,
        MSG_HAVE,
        MSG_BITFIELD,
        MSG_REQUEST,
        MSG_PIECE,
        MSG_CANCEL,
        MSG_PORT,
        MSG_EXTENSION
};

#pragma pack(push, 1)
struct PEER_EXTENSION {
    uint32_t length;
    uint8_t msg_id;
    uint8_t extended_msg_id;
    uint8_t msg[];
};
#pragma pack(pop)


/**
 * @brief returns true if there is a message available for reading
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int peer_should_read_message(struct Peer * p);

/**
 * @brief attempt to read a message from the peer. will either return NULL
 *        or will allocate memory to use as a buffer if we are dealing with a valid message
 *        the returned buffer can be passed to get_msg_length and get_msg_id to extract
 *        message id and message length
 * @note buffer will be a minimum of 5 bytes (4 bytes for message length, 1 byte for message id)
 * @param p
 * @param buffer_size
 * @return pointer to message buffer. NULL if there's no available valid message to handle
 */
extern void * peer_read_message(struct Peer * p, _Atomic int * cancel_flag);



/**
 * @brief function to extract the total size of a message buffer returned from peer_read_message
 * @param buffer
 * @param buffer_size value will be dumped here
 */
extern void get_msg_buffer_size(void * buffer, size_t * buffer_size);

/**
 * @brief function to extract the message length prefix from a message buffer returned from peer_read_message
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29 <- messages section
 * @param buffer
 * @param msg_length value will be dumped here
 */
extern void get_msg_length(void * buffer, uint32_t * msg_length);

/**
 * @brief function to extract the message id from a message buffer returned from peer_read_message
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29 <- messages section
 * @param buffer
 * @param msg_id
 */
extern void get_msg_id(void * buffer, uint8_t * msg_id);

/**
 * @brief returns EXIT_SUCCESS if msg_id is valid
 * @param msg_id
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int is_valid_msg_id(uint8_t msg_id);

extern int peer_handle_msg_choke(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_unchoke(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_interested(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_not_interested(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_have(struct Peer *p, void * msg_buffer);

extern int peer_should_send_msg_bitfield(struct Peer *p, struct TorrentData * torrent_data);
extern int peer_send_msg_bitfield(struct Peer *p, struct TorrentData * torrent_data);
extern int peer_handle_msg_bitfield(struct Peer *p, void * msg_buffer);

extern int peer_handle_msg_request(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_piece(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_cancel(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_port(struct Peer *p, void * msg_buffer);
extern int peer_handle_msg_extension(struct Peer * p, void * msg_buffer, struct TorrentData * torrent_metadata, struct Queue * metadata_queue);


#endif //UVGTORRENT_C_PEER_MESSAGES_H
