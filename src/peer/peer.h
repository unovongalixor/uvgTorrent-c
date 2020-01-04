//
// Created by vongalixor on 12/30/19.
//

#ifndef UVGTORRENT_C_PEER_H
#define UVGTORRENT_C_PEER_H

#include <inttypes.h>
#include <netinet/ip.h>

struct Peer {
    struct in_addr addr;
    char * str_ip;
    int32_t ip;
    uint16_t port;
};

/**
 * @brief create a new peer struct
 * @param ip ip for this peer, taken from tracker announce response
 * @param port port for this peer, taken from tracker announce response
 * @return struct Peer *
 */
extern struct Peer * peer_new(int32_t ip, uint16_t port);

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


#endif //UVGTORRENT_C_PEER_H
