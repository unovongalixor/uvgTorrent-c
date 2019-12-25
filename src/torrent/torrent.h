#ifndef UVGTORRENT_C_TORRENT_H
#define UVGTORRENT_C_TORRENT_H

#include "../tracker/tracker.h"
#include "../thread_pool/thread_pool.h"

#define MAX_TRACKERS 5

struct Torrent {
    char *magnet_uri;
    char *path;
    char *name;
    char *hash;

    uint8_t tracker_count;  /*	total number of tracker_scrape                                                          */

    int64_t downloaded;     /*	The number of byte you've downloaded in this session.                                   */
    int64_t left;           /*	The number of bytes you have left to download until you're finished.                    */
    int64_t uploaded;       /*	The number of bytes you have uploaded in this session.                                  */

    struct Tracker *trackers[MAX_TRACKERS];
};

/**
 * extern struct Torrent * torrent_new(char * magnet_uri, char * path)
 *
 * char * magnet_uri  : magnet_uri to download
 * char * path        : local save path
 *
 * NOTES   : mallocs a new torrent struct and parses the given magnet_uri,
           : initializing tracker objects for aquiring peers
 * RETURN  : struct Torrent *
 */
extern struct Torrent *torrent_new(char *magnet_uri, char *path);

/**
 * extern int torrent_add_tracker(struct Torrent * t, char * url)
 *
 * struct Torrent * t;
 * char * url        : tracker url
 *
 * NOTES   : add a tracker at url to the given Torrent struct
 * RETURN  : success
 */
extern int torrent_add_tracker(struct Torrent *t, char *url);

/**
 * extern void torrent_connect_trackers(struct Torrent * t)
 *
 * struct Torrent * t;
 *
 * NOTES   : connect to the trackers attached to the given Torrent struct
 * RETURN  : success
 */
extern int torrent_connect_trackers(struct Torrent *t, struct ThreadPool *tp);

/**
 * extern void torrent_announce_trackers(struct Torrent * t)
 *
 * struct Torrent * t;
 *
 * NOTES   : announce to the connected trackers attached to the given Torrent struct
 * RETURN  :
 */
extern void torrent_announce_trackers(struct Torrent *t);

/**
 * extern void torrent_scrape_trackers(struct Torrent * t)
 *
 * struct Torrent * t;
 *
 * NOTES   : scrape states from the trackers attached to the given Torrent struct
 * RETURN  :
 */
extern void torrent_scrape_trackers(struct Torrent *t);

/**
 * extern struct Torrent * torrent_free(struct Torrent *)
 *
 * struct Torrent * t;
 *
 * NOTES   : clean up the torrent and all associated tracker objects
 * RETURN  : freed and NULL'd struct Torrent *
 */
extern struct Torrent *torrent_free(struct Torrent *);

#endif //UVGTORRENT_C_TORRENT_H
