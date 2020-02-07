#ifndef UVGTORRENT_C_TORRENT_DATA_H
#define UVGTORRENT_C_TORRENT_DATA_H

#include <pthread.h>

struct TorrentDataClaim {
    int64_t deadline;
    int chunk_id;
    struct TorrentDataClaim * next;
};

struct TorrentData {
    _Atomic int needed; // are there chunks of this data that peers should be requesting?
    _Atomic int initialized; // am i usable yet? set to true when data_size is set and the data buffer 
    struct Bitfield * claimed; // bitfield indicating whether each chunk is currently claimed by someone else.
    struct Bitfield * completed; // bitfield indicating whether each chunk is completed yet or not

    size_t piece_size; // number of bytes that make up a piece of this data.
    size_t chunk_size; // number of bytes that make up a chunk of a piece of this data.
    size_t data_size;  // size of data.
    int chunk_count; // number of chunks in this data

    struct TorrentDataClaim * claims; // linked list of claims to different parts of this data

    _Atomic int_fast64_t downloaded;     /*	The number of byte you've downloaded in this session.                                   */
    _Atomic int_fast64_t left;           /*	The number of bytes you have left to download until you're finished.                    */
    _Atomic int_fast64_t uploaded;       /*	The number of bytes you have uploaded in this session.                                  */

    void * data;
    pthread_mutex_t initializer_lock;
};

extern struct TorrentData * torrent_data_new();

/* initialization */
extern void torrent_data_set_piece_size(struct TorrentData * td, size_t piece_size);

extern void torrent_data_set_chunk_size(struct TorrentData * td, size_t chunk_size);

extern int torrent_data_set_data_size(struct TorrentData * td, size_t data_size);

/* claiming data */
extern int torrent_data_claim_chunk(struct TorrentData * td);

extern int torrent_data_release_claims(struct TorrentData * td);

/* writing data */
extern int torrent_data_write_chunk(struct TorrentData * td, int chunk_id, void * data, size_t data_size);

/* cleanup */
extern struct TorrentData * torrent_data_free(struct TorrentData * td);

#endif //UVGTORRENT_C_TORRENT_DATA_H
