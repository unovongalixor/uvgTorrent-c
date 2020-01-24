//
// Created by vongalixor on 12/30/19.
//

#ifndef UVGTORRENT_C_PEER_H
#define UVGTORRENT_C_PEER_H

#include <inttypes.h>
#include <netinet/ip.h>
#include <stdint.h>

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
