/**
 * @file torrent/torrent_data.h
 * @author Simon Bursten <smnbursten@gmail.com>
 *
 * @brief the torrent_data struct provides an interface for organizing access to a shared resource which is being
 *        concurrently requested from a swarm of peers. it can be used to represent either torrent metadata or
 *        torrent data.
 *
 *        it is the single source of truth for the information needed to synchronize downloading a torrent. it also
 *        handles mapping that torrent data to the harddrive, allowing other parts of the code to simply interact
 *        with the read and write functions, and without worrying about how that data is mapped to the drive
 *
 *        this struct provides 3 sets of interfaces for the peer struct, the torrent struct, and the tracker struct:
 *
 *        - peer structs can use torrent_data_claim_chunk to lay claim to an uncompleted and unclaimed chunk of the torrent_data, so that
 *          multiple peers aren't requesting the same chunk in parallel. *
 *        - peer structs can access a function for reading completed chunks for the purposes of uploading them.
 *
 *        - the torrent struct can use torrent_data_write_chunk to write a chunk received from a peer and declare it complete
 *        - the torrent struct is also responsible for initializing this struct (in both metadata and torrent data cases)
 *          and defining which files the data maps to.
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
 * @note this struct only retains incomplete pieces in memory. completed pieces are written out to drive based on the
 *       file mappings defined during initialization. when you call torrent_data_read_data the struct will handle
 *       loading from the disk into memory and copying the needed data into your buffer.
 *
 * @note pieces are saved in the data hashmap with their piece index as a key. when writing a chunk we can safely assume
 *       that the chunk is entirely within the bounds of a single piece, but when reading from the torrent data we
 *       need to handle reads across multiple pieces. you can see this in torrent_data_read_data.
 *
 * @note releasing expired claim deadlines depends on torrent_data_release_expired_claims() being called regularly from
 *       the main loop. it's important that this function is getting called or the first claim on a chunk will never expire
 *       and the swarm will only request it once.
 *
 * @note writing torrent data is the responsibility of the torrent struct. this class will provide functions
 *       to get the offset and length of any piece / chunk of the torrent to help with writing to a file.
 *
 * @note completeness is marked by the completed bitfield, with each bit representing a chunk, and a certain number of
 *       bytes representing a piece. for example if there are 16 chunks to a piece, then each piece will be represented
 *       by 2 bytes of the bitfield. therefore if we wanted to see if piece 0 was complete we would check if
 *       td->completed->bytes[0] and td->completed->bytes[1] both equaled 0xFF. for piece 1 we would check bytes[2] and
 *       bytes[3], and so on.
 *
 * @note this struct makes no assumptions about how it's user will want to make use of it's pieces.
 *       when used for ut_metadata we ignore pieces, simply requesting chunks until the data is complete and
 *       then reading the entire data in order to parse it.
 *
 *       when loading torrent data, however, each piece is validated when it's final chunk is written and the memory freed.
 *
 * @see http://bittorrent.org/bittorrentecon.pdf
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
 */
#ifndef UVGTORRENT_C_TORRENT_DATA_H
#define UVGTORRENT_C_TORRENT_DATA_H

#include "../hash_map/hash_map.h"
#include <pthread.h>

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
    _Atomic int needed; // are there chunks of this data that peers should be requesting?
    _Atomic int initialized; // am i usable yet? set to true when data_size is set and the data buffer
    struct Bitfield * claimed; // bitfield indicating whether each chunk is currently claimed by someone else.
    struct Bitfield * completed; // bitfield indicating whether each chunk is completed yet or not
    struct Bitfield * pieces; // bitfield indicating the status of the pieces of this data
                              // useful for sending BITFIELD messages to peers, ignorable for metadata

    struct TorrentDataFileInfo * files;
    size_t files_size;

    size_t piece_size; // number of bytes that make up a piece of this data.
    size_t chunk_size; // number of bytes that make up a chunk of a piece of this data.
    size_t data_size;  // size of data.
    int chunk_count; // number of chunks in this data

    struct TorrentDataClaim * claims; // linked list of claims to different parts of this data

    _Atomic int_fast64_t downloaded;     /*	The number of byte you've downloaded in this session.                                   */
    _Atomic int_fast64_t left;           /*	The number of bytes you have left to download until you're finished.                    */
    _Atomic int_fast64_t uploaded;       /*	The number of bytes you have uploaded in this session.                                  */

    struct HashMap * data;
    pthread_mutex_t initializer_lock;
};

extern struct TorrentData * torrent_data_new();

/* initialization */
extern void torrent_data_set_piece_size(struct TorrentData * td, size_t piece_size);

extern void torrent_data_set_chunk_size(struct TorrentData * td, size_t chunk_size);

extern int torrent_data_add_file(struct TorrentData * td, char * path, uint64_t length);

extern int torrent_data_set_data_size(struct TorrentData * td, size_t data_size);

/* claiming data */
extern int torrent_data_claim_chunk(struct TorrentData * td);

extern int torrent_data_release_expired_claims(struct TorrentData * td);

/* writing data */
extern int torrent_data_write_chunk(struct TorrentData * td, int chunk_id, void * data, size_t data_size);

/* reading data */
extern int torrent_data_read_data(struct TorrentData * td, void * buff, size_t offset, size_t length);

/* chunk & piece info */
extern int torrent_data_get_chunk_info(struct TorrentData * td, int chunk_id, struct ChunkInfo * chunk_info);
extern int torrent_data_get_piece_info(struct TorrentData * td, int piece_id, struct PieceInfo * piece_info);
extern int torrent_data_is_piece_complete(struct TorrentData *td, int piece_id);

/* cleanup */
extern struct TorrentData * torrent_data_free(struct TorrentData * td);

#endif //UVGTORRENT_C_TORRENT_DATA_H
