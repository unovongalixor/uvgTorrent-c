/**
 * @file torrent/torrent_data.h
 * @author Simon Bursten <smnbursten@gmail.com>
 *
 * @brief the torrent_data struct provides an interface for organizing access to a shared resource which is being
 *        concurrently requested from a swarm of peers. it can be used to represent either torrent metadata or
 *        torrent data, more information on the differences between the two in notes.
 *
 *        it is the single source of truth for the information needed to synchronize downloading a torrent.
 *
 *        this struct provides 3 sets of interfaces for the peer struct, the torrent struct, and the tracker struct:
 *
 *        - peer structs can use torrent_data_claim_chunk to lay claim to an uncompleted and unclaimed chunk of the torrent_data, so that
 *          multiple peers aren't requesting the same chunk in parallel. *
 *        - peer structs can access a function for reading completed chunks for the purposes of uploading them.
 *
 *        - the torrent struct can use torrent_data_write_chunk to write a chunk received from a peer and declare it complete
 *        - the torrent struct can access functions to get piece information such as offset and length
 *          to help with validation and writing to disk.
 *
 *        - tracker structs can access downloaded, left and uploaded to provided needed data to trackers during
 *          announce requests
 *
 *        using these interfaces will guarantee safe, sane shared access to torrent data / metadata
 *
 * @note releasing expired claim deadlines depends on torrent_data_release_expired_claims() being called regularly from
 *       the main loop. it's important that this function is getting called or the first claim on a chunk will never expire
 *       and the swarm will only request it once.
 *
 * @note writing torrent data to drive is the responsibility of the torrent struct. this class will provide functions
 *       to get the offset and length of any piece / chunk of the torrent to help with writing to a file.
 *
 * @note completeness is marked by the completed bitfield, with each bit representing a chunk, and a certain number of
 *       bytes representing a piece. for example if there are 16 chunks to a piece, then each piece will be represented
 *       by 2 bytes of the bitfield. therefore if we wanted to see if piece 0 was complete we would check if
 *       td->completed->bytes[0] and td->completed->bytes[1] both equaled 0xFF. for piece 1 we would check bytes[2] and
 *       bytes[3], and so on.
 *
 * @note when using a torrent_data struct to represent torrent metadata being requested via the ut_metadata extension
 *       you can ignore the pieces. when all the chunks are complete (i.e. all of td->completed->bytes == 0xFF) then
 *       you can try to decode the metadata
 *
 * @see http://bittorrent.org/bittorrentecon.pdf
 * @see https://wiki.theory.org/index.php/BitTorrentSpecification#Peer_wire_protocol_.28TCP.29
 */
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

extern int torrent_data_release_expired_claims(struct TorrentData * td);

/* writing data */
extern int torrent_data_write_chunk(struct TorrentData * td, int chunk_id, void * data, size_t data_size);

/* cleanup */
extern struct TorrentData * torrent_data_free(struct TorrentData * td);

#endif //UVGTORRENT_C_TORRENT_DATA_H
