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
extern struct Peer * peer_new(int32_t ip, uint16_t port, int am_initiating);

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
 * @brief peer main loop
 * @param cancel_flag
 * @param ...
 * @return
 */
extern int peer_run(int * cancel_flag, ...);

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
    int8_t pstrlen;
    int8_t pstr[19];
    int8_t reserved[8];
    int8_t info_hash[20];
    int8_t peer_id[20];
};

#pragma pack(pop)
#endif //UVGTORRENT_C_PEER_H
