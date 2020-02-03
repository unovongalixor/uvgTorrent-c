#ifndef UVGTORRENT_C_TORRENT_DATA_H
#define UVGTORRENT_C_TORRENT_DATA_H

struct TorrentDataClaim {
    int64_t deadline;
    int chunk_id;
    struct TorrentDataClaim * next;
};

struct TorrentData {
    _Atomic int needed; // are there chunks of this data that peers should be requesting?
    struct Bitfield * claimed; // bitfield indicating whether each chunk is currently claimed by someone else.
    struct Bitfield * completed; // bitfield indicating whether each chunk is completed yet or not

    size_t piece_size; // number of bytes that make up a piece of this data
    size_t chunk_size; // number of bytes that make up a chunk of a piece of this data
    size_t data_size;  // size of data
    int chunk_count; // number of chunks in this data

    struct TorrentDataClaim * claims; // linked list of claims to different parts of this data

    uint8_t data[];
};

#endif //UVGTORRENT_C_TORRENT_DATA_H
