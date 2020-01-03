#ifndef UVGTORRENT_C_TORRENT_H
#define UVGTORRENT_C_TORRENT_H

#include "../tracker/tracker.h"
#include "../thread_pool/thread_pool.h"

#define MAX_TRACKERS 5

struct Torrent {
    char *magnet_uri;
    char *path;
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
};

/**
 * @brief mallocs a new torrent struct and parses the given magnet_uri,
 *        initializing tracker objects for acquiring peers
 * @param magnet_uri
 * @param path
 * @return struct Torrent *. NULL on failure
 */
extern struct Torrent *torrent_new(char *magnet_uri, char *path);

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
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
extern int torrent_run_trackers(struct Torrent *t, struct ThreadPool *tp, struct Queue * peer_queue);

/**
 * @brief clean up the torrent and all child structs (trackers, peers, etc)
 * @param t
 * @return t after freeing. NULL on success
 */
extern struct Torrent *torrent_free(struct Torrent * t);

#endif //UVGTORRENT_C_TORRENT_H
