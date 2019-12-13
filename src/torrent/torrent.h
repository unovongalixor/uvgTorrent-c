//
// Created by vongalixor on 2019-12-10.
//
#include "../tracker/tracker.h"

#ifndef UVGTORRENT_C_TORRENT_H
#define UVGTORRENT_C_TORRENT_H

#define MAX_TRACKERS 5

struct Torrent {
    char * magnet_uri;
    char * path;
    char * name;
    char * hash;

    uint8_t tracker_count;

    int64_t	downloaded;     /*	The number of byte you've downloaded in this session.                                   */
    int64_t	left;           /*	The number of bytes you have left to download until you're finished.                    */
    int64_t	uploaded;       /*	The number of bytes you have uploaded in this session.                                  */

    struct Tracker * trackers[MAX_TRACKERS];
};

extern struct Torrent * torrent_new(char * magnet_uri, char * path);
extern int torrent_add_tracker(struct Torrent * t, char * url);
extern void torrent_announce_trackers(struct Torrent * t);
extern struct Torrent * torrent_free(struct Torrent *);

#endif //UVGTORRENT_C_TORRENT_H
