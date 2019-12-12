//
// Created by vongalixor on 2019-12-10.
//

#ifndef UVGTORRENT_C_TORRENT_H
#define UVGTORRENT_C_TORRENT_H

struct Torrent {
    int size;
    int metadata_loaded;
    int chunk_size;

    char * magnet_uri;
    char * path;
    char * name;
    char * hash;
};

extern struct Torrent * torrent_new(char * magnet_uri, char * path);
extern void torrent_free(struct Torrent *);

#endif //UVGTORRENT_C_TORRENT_H
