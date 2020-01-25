//
// Created by vongalixor on 12/30/19.
//

#ifndef UVGTORRENT_C_PEER_H
#define UVGTORRENT_C_PEER_H

#include <inttypes.h>
#include <netinet/ip.h>
#include <stdint.h>
#include "../thread_pool/queue.h"

enum PeerStatus {
    PEER_UNCONNECTED,
    PEER_CONNECTING,
    PEER_CONNECTED,
    PEER_HANDSHAKING,
    PEER_HANDSHAKED
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

    /*
     * peer may lay exclusive claim to pieces of metadata or pieces of the torrent, both of which are
     * managed via shared mutex protected bitfields.
     * when claimed_resource_deadline expires the peer will
     * return the shared resource so a different peer can attempt to request it,
     * by setting claimed_resource_bit to 0 in claimed_resource;
     */
    int64_t claimed_resource_deadline;
    struct Bitfield * claimed_resource;
    int claimed_resource_bit;
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
 * @note if the peer supports utmetadata extended handshake, this function will also do the full extended handshake
 *       and mark this peer as supporting utmetadata requests
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

/* MESSAGES */

/**
 * @brief send a piece request.
 * @param p
 * @param piece_num
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int peer_request_metadata_piece(struct Peer * p, int piece_num);

/**
 * @brief handle a metadata piece.
 * @param p
 * @param piece_num
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int peer_handle_metadata_piece(struct Peer * p, struct Queue * metadata_queue);

/**
 * @brief listen for a message from the peer. this function should determine which message we're dealing with an
 *        call the correct handling function
 * @param p
 * @param metadata_queue queue for returning metadata
 * @param pieces_queue queue for returning pieces of the torrent
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int peer_wait_for_message(struct Peer * p, struct Queue * metadata_queue, struct Queue * pieces_queue);

/**
 * @brief peer main loop
 * @param cancel_flag
 * @param ...
 * @return
 */
extern int peer_run(_Atomic int * cancel_flag, ...);

/**
 * @brief lay exclusive claim to a shared resource, either a metadata piece or a torrent piece that needs to be requested
 * @param p
 * @param shared_resource
 * @return
 */
extern int peer_claim_resource(struct Peer * p, struct Bitfield * shared_resource);

/**
 * @brief if we have an exclusive claim, release it.
 * @note we do this by setting p->claimed_resource_bit to 0 in p->claimed_resource
 * @param p
 * @param shared_resource
 * @return
 */
extern void peer_release_resource(struct Peer * p, struct Bitfield * shared_resource);

/**
 * @brief free the given peer struct
 * @return p after freeing, NULL on success
 */
extern struct Peer * peer_free(struct Peer * p);


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
