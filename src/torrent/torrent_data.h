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

    size_t piece_size; // number of bytes that make up a piece of this data. defaults to 256k
    size_t chunk_size; // number of bytes that make up a chunk of a piece of this data. defaults to 16k
    size_t data_size;  // size of data. defaults to 0.
    int chunk_count; // number of chunks in this data

    struct TorrentDataClaim * claims; // linked list of claims to different parts of this data

    void * data;
};

extern struct TorrentData * torrent_data_new();

extern void torrent_data_set_piece_size(struct TorrentData * td, size_t piece_size);

extern void torrent_data_set_chunk_size(struct TorrentData * td, size_t chunk_size);

extern void torrent_data_set_data_size(struct TorrentData * td, size_t data_size);

extern int torrent_data_claim_chunk(struct TorrentData * td);

extern int torrent_data_release_claims(struct TorrentData * td);

extern struct TorrentData * torrent_data_free(struct TorrentData * td);

#endif //UVGTORRENT_C_TORRENT_DATA_H
