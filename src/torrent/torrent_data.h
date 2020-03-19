/**
 * @file torrent/torrent_data.h
 * @author Simon Bursten <smnbursten@gmail.com>
 *
 * @brief the torrent_data struct provides an interface for organizing access to a shared resource which is being
 *        concurrently requested from a swarm of peers. it can be used to represent either torrent metadata or
 *        torrent data.
 *
 *        the struct provides interfaces for claiming, reading & writing segments of the data, while mapping completed
 *        bits of data to files on disk in the background.
 *         ___________________________
 *        |       TORRENT DATA        |
 *        |___________________________|
 *        | Piece1 | Piece2 | Piece3  |
 *        |________|________|_________|
 *        | C  | C | C  | C |  C | C  |
 *        |____|___|____|___|____|____|
 *        | File 1    |    File 2     |
 *        |___________|_______________|
 *
 * @note this struct provides 3 sets of interfaces for the peer struct, the torrent struct, and the tracker struct:
 *
 *        - peer structs can use torrent_data_claim_chunk to lay claim to an uncompleted and unclaimed chunk of the torrent_data, so that
 *          multiple peers aren't requesting the same chunk in parallel.
 *        - peer structs can read completed chunks for the purposes of sharing them.
 *
 *        - the torrent struct can use torrent_data_write_chunk to write a chunk received from a peer and declare it complete
 *        - the torrent struct can access torrent_data_release_expired_claims regularly to ensure expired claims are released
 *
 *        - tracker structs can access downloaded, left and uploaded to provided needed data to trackers during
 *          announce requests
 *
 *        using these interfaces will guarantee safe, sane shared access to torrent data / metadata
 *
 * @note torrent_data is allocated uninitialized as the information needed for initialization becomes known bit by bit.
 *       in order to initialized you should:
 *          - call torrent_data_set_chunk_size to set up chunk size
 *          - call torrent_data_set_piece_size to set up piece size
 *          - call torrent_data_add_file for as many files as are represented by this data,
 *            in the order they appear in the data. this will map the pieces of this data to files on the harddrive for
 *            reading and writing.
 *
 *          - then call set data_size to set the size of the data being downloaded. order doesn't matter, but if you
 *            skipped any of the above steps you will get an error.
 *          - you are now initialized
 *
 * @note releasing expired claim deadlines depends on torrent_data_release_expired_claims() being called regularly from
 *       the main loop. it's important that this function is getting called or the first claim on a chunk will never expire
 *       and the swarm will only request it once.
 *
 * @see http://bittorrent.org/bittorrentecon.pdf
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
 */
#ifndef UVGTORRENT_C_TORRENT_DATA_H
#define UVGTORRENT_C_TORRENT_DATA_H

#include "../hash_map/hash_map.h"
#include "../bitfield/bitfield.h"
#include <pthread.h>
#include <stdio.h>

struct TorrentDataClaim {
    int64_t deadline;
    int chunk_id;
    struct TorrentDataClaim * next;
};

/**
 * the TorrentDataFileMapping struct holds the information needed to map pieces to files
 * for saving and for reading
 */
struct TorrentDataFileInfo {
    FILE * fp;
    char * file_path;
    size_t file_offset;
    size_t file_size;
    struct TorrentDataFileInfo * next;
};

struct ChunkInfo {
    int chunk_id;
    int piece_id; // which piece does the chunk belong to
    size_t chunk_size;
    size_t chunk_offset;
};

struct PieceInfo {
    int piece_id;
    size_t piece_size;
    size_t piece_offset;
};

struct TorrentData {
    /* STATE */
    _Atomic int needed; // are there chunks of this data that peers should be requesting?
    _Atomic int initialized; // am i usable yet? set to true when data_size is set
    struct TorrentDataClaim * claims; // linked list of claims to different chunks of this data
    struct Bitfield * claimed; // bitfield indicating whether each chunk is currently claimed by someone else.
    struct Bitfield * completed; // bitfield indicating whether each chunk  &| piece is completed

    /* FILE MAPPING STUFF */
    char * root_path;
    struct TorrentDataFileInfo * files;
    size_t files_size;

    /* CONFIG */
    size_t piece_size; // number of bytes that make up a piece of this data.
    size_t chunk_size; // number of bytes that make up a chunk of a piece of this data.
    size_t data_size;  // size of data.
    int piece_count;
    int completed_pieces;
    int chunk_count;

    _Atomic int_fast64_t downloaded;     /*	The number of byte you've downloaded in this session.                                   */
    _Atomic int_fast64_t left;           /*	The number of bytes you have left to download until you're finished.                    */
    _Atomic int_fast64_t uploaded;       /*	The number of bytes you have uploaded in this session.                                  */

    struct HashMap * data;
    pthread_mutex_t initializer_lock;

    int (*validate_piece)(struct TorrentData * td, struct PieceInfo piece_info, void * piece_data);
};

extern struct TorrentData * torrent_data_new(char * root_path);

/* initialization */
extern void torrent_data_set_piece_size(struct TorrentData * td, size_t piece_size);

extern void torrent_data_set_chunk_size(struct TorrentData * td, size_t chunk_size);

extern int torrent_data_add_file(struct TorrentData * td, char * path, uint64_t length);

extern int torrent_data_set_validate_piece_function(struct TorrentData * td, int (*validate_piece)(struct TorrentData * td, struct PieceInfo piece_info, void * piece_data));

extern int torrent_data_set_data_size(struct TorrentData * td, size_t data_size);

/* claiming data */
extern int torrent_data_claim_chunk(struct TorrentData * td, struct Bitfield * interested_chunks, int timeout_seconds, int num_chunks, int * out);

extern int torrent_data_release_expired_claims(struct TorrentData * td);

/* writing data */
extern int torrent_data_write_chunk(struct TorrentData * td, int chunk_id, void * data, size_t data_size);

/* reading data */
extern int torrent_data_read_data(struct TorrentData * td, void * buff, size_t offset, size_t length);

/* chunk & piece info */
extern int torrent_data_get_chunk_info(struct TorrentData * td, int chunk_id, struct ChunkInfo * chunk_info);
extern int torrent_data_get_piece_info(struct TorrentData * td, int piece_id, struct PieceInfo * piece_info);
extern int torrent_data_is_piece_complete(struct TorrentData *td, int piece_id);

extern int torrent_data_is_complete(struct TorrentData *td);

/* cleanup */
extern struct TorrentData * torrent_data_free(struct TorrentData * td);

#endif //UVGTORRENT_C_TORRENT_DATA_H
