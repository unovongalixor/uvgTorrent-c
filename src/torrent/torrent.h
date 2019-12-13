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

    int size;
    int metadata_loaded;
    int chunk_size;
    int tracker_count;

    struct Tracker * trackers[MAX_TRACKERS];
};

extern struct Torrent * torrent_new(char * magnet_uri, char * path);
extern int torrent_add_tracker(struct Torrent * t, char * url);
extern struct Torrent * torrent_free(struct Torrent *);

#endif //UVGTORRENT_C_TORRENT_H
