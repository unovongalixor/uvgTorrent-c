#ifndef UVGTORRENT_C_TORRENT_H
#define UVGTORRENT_C_TORRENT_H

#include "../tracker/tracker.h"
#include "../thread_pool/thread_pool.h"
#include "../peer/peer.h"
#include "../hash_map/hash_map.h"
#include "../bitfield/bitfield.h"
#include <stdatomic.h>

#define MAX_TRACKERS 5

struct Torrent {
    char *magnet_uri;
    char *path;
    uint16_t port;
    char *name;
    char *info_hash;
    int8_t info_hash_hex[20];

    uint8_t tracker_count;  /*	total number of tracker_scrape                                                          */

    pthread_mutex_t downloaded_mutex;
    int64_t downloaded;     /*	The number of byte you've downloaded in this session.                                   */
    pthread_mutex_t left_mutex;
    int64_t left;           /*	The number of bytes you have left to download until you're finished.                    */
    pthread_mutex_t uploaded_mutex;
    int64_t uploaded;       /*	The number of bytes you have uploaded in this session.                                  */

    struct Tracker *trackers[MAX_TRACKERS];
    struct HashMap * peers;

    _Atomic int needs_metadata; // atomic int
    struct Bitfield * metadata_pieces;
    int loaded_metadata_pieces;
};

/**
 * @brief mallocs a new torrent struct and parses the given magnet_uri,
 *        initializing tracker objects for acquiring peers
 * @param magnet_uri
 * @param path
 * @param port
 * @return struct Torrent *. NULL on failure
 */
extern struct Torrent *torrent_new(char *magnet_uri, char *path, int port);

/**
 * @brief add a tracker at url to the given Torrent
 * @param t
 * @param url
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int torrent_add_tracker(struct Torrent *t, char *url);

/**
 * @brief run each trackers run function in the given ThreadPool
 * @param t
 * @param tp
 * @param peer_queue queue to put peers into
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int torrent_run_trackers(struct Torrent *t, struct ThreadPool *tp, struct Queue * peer_queue);

/**
 * @brief add and run the given peer to the given torrent if we don't already have a peer struct for this peer
 *        added peers are also ran in the given thread pool
 * @note if the torrent already has this peer, the peer is free'd.
 *       if not, peer is added to the torrent and peer is ran in tp.
 * @param t
 * @param p
 * @param tp
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int torrent_add_peer(struct Torrent *t, struct ThreadPool *tp, struct Peer * p);

/**
 * @brief listen for connecting peers, return peer objects to peer_queue
 * @param cancel_flag
 * @param ...
 * @return
 */
extern int torrent_listen_for_peers(_Atomic int * cancel_flag, ...);

/**
 * @brief clean up the torrent and all child structs (trackers, peers, etc)
 * @param t
 * @return t after freeing. NULL on success
 */
extern struct Torrent *torrent_free(struct Torrent * t);

#endif //UVGTORRENT_C_TORRENT_H
