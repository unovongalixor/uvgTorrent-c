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
 *
 * @note check out peer_should_handle_network_buffers & peer_handle_network_buffers. if these functions aren't part of
 *       peer_should_run and peer_run the peer won't do anything as it wont see any data in it's buffered_sockets buffers
 */

#ifndef UVGTORRENT_C_PEER_H
#define UVGTORRENT_C_PEER_H

#include "peer_struct.h"
#include "peer_connect.h"
#include "peer_messages.h"
#include "peer_ut_metadata.h"

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
 * @brief network buffer handling
 * @param p
 * @return
 */
extern int peer_should_handle_network_buffers(struct Peer * p);
extern int peer_handle_network_buffers(struct Peer * p);

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



#endif //UVGTORRENT_C_PEER_H
