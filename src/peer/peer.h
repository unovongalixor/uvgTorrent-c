/**
 * @file peer/peer.h
 * @author Simon Bursten <smnbursten@gmail.com>
 *
 * @brief the peer struct establishes and manages the state of a connection with a given peer. the peer struct will handle
 *        the connection and handshake process and then handle a stream of messages to and from the peer.
 *
 * @note to see the core behavior of the peer struct you should look at peer_run. like tracker/tracker.h it follows the
 *       pattern of first calling a "peer_should_take_action()" function to see if the current state calls for an associated
 *       action, and then calling "peer_take_action()" if the should function returned 1.
 *
 *       the peer struct also provides a "peer_should_run()" function for torrent/torrent.h to use to determine if
 *       a peer_run job needs to be scheduled.
 *
 * @note it's important any time you exit the peer_run function to set p->running = 0; so that the peer
 *       can be scheduled again as needed.
 */

#ifndef UVGTORRENT_C_PEER_H
#define UVGTORRENT_C_PEER_H

#include <inttypes.h>
#include <netinet/ip.h>
#include <stdint.h>
#include "../thread_pool/queue.h"
#include "../torrent/torrent_data.h"

/* message related stuff */
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

/* peer related stuff */
enum PeerStatus {
    PEER_UNCONNECTED,
    PEER_CONNECTING,
    PEER_CONNECTED,
    PEER_HANDSHAKING,
    PEER_HANDSHAKED,
    PEER_RUNNING
};

struct Peer {
    struct sockaddr_in addr;
    char * str_ip;
    int32_t ip;
    uint16_t port;
    int socket;
    int utmetadata;
    int metadata_size;

    int am_initiating;
    int am_choking;
    int am_interested;
    int peer_choking;
    int peer_interested;

    enum PeerStatus status;
    int running;
};

/**
 * @brief create a new peer struct
 * @param ip ip for this peer, taken from tracker announce response
 * @param port port for this peer, taken from tracker announce response
 * @param am_initiating is this client being intitiated by us? this tells us whether we are expected to send
 *        the handshake first or not
 * @return struct Peer *
 */
extern struct Peer * peer_new(int32_t ip, uint16_t port);

/**
 * @brief set this peers socket and set status to connected
 * @param p
 * @param socket
 */
extern void peer_set_socket(struct Peer * p, int socket);

/**
 * @brief return true false, this peer is unconnected and ready to establish a connection
 * @param p
 * @return
 */
extern int peer_should_connect(struct Peer * p);

/**
 * @brief set up and connect a socket to this peer
 * @param p
 * @return
 */
extern int peer_connect(struct Peer * p);

/**
 * @brief return true false, this peer is connected and ready to perform an extended handshake
 * @param p
 * @return
 */
extern int peer_should_handshake(struct Peer * p);

/**
 * @brief perform handshake with peer.
 * @note if the peer supports utmetadata extended handshake, this function will also send the extended handshake
 * @param p
 * @param info_hash_hex
 * @return
 */
extern int peer_handshake(struct Peer * p, int8_t info_hash_hex[20], _Atomic int * cancel_flag);

/**
 * @brief returns 1 if the peer supports ut_metadata, 0 if not
 * @param p
 * @return
 */
extern int peer_supports_ut_metadata(struct Peer * p);

/**
 * @brief should this peer attempt to claim and request a chunk of metadata?
 * @param p
 * @return
 */
extern int peer_should_request_metadata(struct Peer * p, struct TorrentData ** torrent_metadata);

/**
 * @brief send a piece request. the piece to be requested should have been claimed using peer_claim_resource
 *        and the piece to be requested should be stored in p->claimed_bitfield_resource_bit
 * @param piece_num
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int peer_request_metadata_piece(struct Peer * p, struct TorrentData ** torrent_metadata);

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

/**
 * @brief returns true if there is any action available for this peer to perform
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int peer_should_run(struct Peer * p, struct TorrentData ** torrent_metadata);

/**
 * @brief peer main loop
 * @param cancel_flag
 * @param ...
 * @return
 */
extern int peer_run(_Atomic int * cancel_flag, ...);

/**
 * @brief free the given peer struct
 * @return p after freeing, NULL on success
 */
extern struct Peer * peer_free(struct Peer * p);

/**
 * @brief close the peer socket and set status to unconnected
 * @param p
 * @return
 */
extern void peer_disconnect(struct Peer * p);


/**
 * @brief PEER WIRE PROTOCOL
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
 */
#pragma pack(push, 1)

struct PEER_HANDSHAKE {
    uint8_t pstrlen;
    uint8_t pstr[19];
    uint8_t reserved[8];
    uint8_t info_hash[20];
    uint8_t peer_id[20];
};

struct PEER_EXTENSION {
    uint32_t length;
    uint8_t msg_id;
    uint8_t extended_msg_id;
    uint8_t msg[];
};

#pragma pack(pop)
#endif //UVGTORRENT_C_PEER_H
