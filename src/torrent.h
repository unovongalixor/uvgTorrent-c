//
// Created by vongalixor on 2019-12-10.
//

#ifndef UVGTORRENT_C_TORRENT_H
#define UVGTORRENT_C_TORRENT_H

struct Torrent {
    int size;
    char magnet_uri[512];
    char path[512];
    char name[512];
    char hash[512];

    int metadata_loaded;
    int chunk_size;
};

struct Torrent * new_torrent(char * magnet_uri);
int torrent_parse_magnet_uri(struct Torrent *);

#endif //UVGTORRENT_C_TORRENT_H
