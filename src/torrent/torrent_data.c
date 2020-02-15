#include <stdlib.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <pthread.h>
#include "torrent_data.h"
#include "../macros.h"
#include "../bitfield/bitfield.h"
#include "../hash_map/hash_map.h"
#include "../deadline/deadline.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct TorrentData * torrent_data_new() {
    struct TorrentData * td = malloc(sizeof(struct TorrentData));
    if(td == NULL) {
        throw("torrent_data failed to malloc");
    }

    td->needed = ATOMIC_VAR_INIT(0); // are there chunks of this data that peers should be requesting?
    td->initialized =  ATOMIC_VAR_INIT(0);
    td->claimed = NULL; // bitfield indicating whether each chunk is currently claimed by someone else.
    td->completed = NULL; // bitfield indicating whether each chunk is completed yet or not
    td->pieces = NULL;

    td->piece_size = 0; // number of bytes that make up a piece of this data.
    td->chunk_size = 0; // number of bytes that make up a chunk of a piece of this data.
    td->data_size = 0;  // size of data.

    td->claims = NULL; // linked list of claims to different parts of this data

    td->downloaded = ATOMIC_VAR_INIT(0);
    td->left = ATOMIC_VAR_INIT(0);
    td->uploaded = ATOMIC_VAR_INIT(0);

    td->data = NULL;
    pthread_mutex_init(&td->initializer_lock, NULL);

    return td;
    error:

    return NULL;
}

/* initialization */
void torrent_data_set_piece_size(struct TorrentData * td, size_t piece_size) {
    if(td->initialized == 1) {
        log_err("can't set piece size after setting data size");
    }
    td->piece_size = piece_size;
}

void torrent_data_set_chunk_size(struct TorrentData * td, size_t chunk_size) {
    if(td->initialized == 1) {
        log_err("can't set chunk size after setting data size");
    }
    td->chunk_size = chunk_size;
}

int torrent_data_set_data_size(struct TorrentData * td, size_t data_size) {
    pthread_mutex_lock(&td->initializer_lock);

    if(td->initialized == 1 & td->data_size != data_size) {
        throw("data already malloced, got unexpected data size");
    } else if(td->initialized == 1 & td->data_size == data_size) {
        goto error;
    }

    if(td->chunk_size == 0) {
        throw("can't initialize with a chunk size set to 0");
    }

    td->data_size = data_size;

    int pieces_enabled = (td->piece_size > 0);
    // determine how many chunks are needed
    size_t chunk_count = (td->data_size + (td->chunk_size - 1)) / td->chunk_size;

    // initialize bitfields
    td->claimed = bitfield_new((int) chunk_count, 0);
    td->completed = bitfield_new((int) chunk_count, 0);

    // optional pieces stuff. useful for torrent data, can be ignored for torrent metadata
    if(pieces_enabled) {
        size_t pieces_count = (td->data_size + (td->piece_size - 1)) / td->piece_size;
        log_info("td->data_size %zu", td->data_size);
        log_info("td->piece_size %zu", td->piece_size);
        log_info("td->pieces_count %zu", pieces_count);
        td->pieces = bitfield_new((int) pieces_count, 0);
        memset(td->pieces->bytes, 0x00, td->pieces->bytes_count);
    }

    // initialize stats
    td->downloaded = ATOMIC_VAR_INIT(0);
    td->left = ATOMIC_VAR_INIT(td->data_size);
    td->uploaded = ATOMIC_VAR_INIT(0);

    // initialize data
    td->data = hashmap_new(1000);

    td->initialized = 1;

    pthread_mutex_unlock(&td->initializer_lock);

    return EXIT_SUCCESS;
    error:

    pthread_mutex_unlock(&td->initializer_lock);
    return EXIT_FAILURE;
};

/* claiming data */
int torrent_data_claim_chunk(struct TorrentData * td) {
    if(td->initialized == 1) {
        bitfield_lock(td->claimed);
        for (int i = 0; i < (td->claimed->bit_count); i++) {
            if (bitfield_get_bit(td->claimed, i) == 0) {
                bitfield_set_bit(td->claimed, i, 1);

                struct TorrentDataClaim * claim = malloc(sizeof(struct TorrentDataClaim));
                claim->deadline = now() + 1000;
                claim->chunk_id = i;
                if(td->claims == NULL) {
                    claim->next = NULL;
                } else {
                    claim->next = td->claims;
                }
                td->claims = claim;

                bitfield_unlock(td->claimed);
                return i;
            }
        }

        bitfield_unlock(td->claimed);
    }

    return -1;
}

int torrent_data_release_expired_claims(struct TorrentData * td) {
    if(td->initialized == 1) {
        bitfield_lock(td->claimed);

        if(td->claims != NULL) {
            struct TorrentDataClaim ** current = &td->claims;
            struct TorrentDataClaim ** prev = NULL;

            while (*current != NULL) {
                if((*current)->deadline < now()){
                    bitfield_lock(td->completed);
                    if(bitfield_get_bit(td->completed, (*current)->chunk_id) == 0) {
                        bitfield_set_bit(td->claimed, (*current)->chunk_id, 0);
                    }
                    bitfield_unlock(td->completed);

                    struct TorrentDataClaim * next = (*current)->next;
                    free((*current));
                    (*current) = NULL;

                    if (prev == NULL) {
                        *current = next;
                    } else {
                        (*prev)->next = next;
                    }

                    if(next == NULL) {
                        break;
                    }
                } else {
                    prev = current;
                    current = &(*current)->next;
                }
            }
        }

        bitfield_unlock(td->claimed);
        return EXIT_SUCCESS;
    }
}

/* writer */
int torrent_data_write_chunk(struct TorrentData * td, int chunk_id, void * data, size_t data_size) {
    // is this chunk already completed?
    bitfield_lock(td->completed);

    if (bitfield_get_bit(td->completed, chunk_id) == 1) {
        // ignore already completed chunks
        bitfield_unlock(td->completed);
        return EXIT_FAILURE;
    }

    // get chunk info
    struct ChunkInfo chunk_info;
    torrent_data_get_chunk_info(td, chunk_id, &chunk_info);

    // get piece info
    struct PieceInfo piece_info;
    torrent_data_get_piece_info(td, chunk_info.piece_id, &piece_info);

    // validate that we received the expected length
    if(data_size != chunk_info.chunk_size) {
        throw("data lengths mismatch %zu %zu", data_size, chunk_info.chunk_size);
    }

    // try and get
    char piece_key[10] = {0x00};
    sprintf(piece_key, "%i", chunk_info.piece_id);
    void * piece = hashmap_get(td->data, (char *) &piece_key);
    if (piece == NULL) {
        log_info("init piece");
        piece = malloc(td->piece_size);
        memset(piece, 0x00, td->piece_size);
    }

    int relative_chunk_offset = chunk_info.chunk_offset - piece_info.piece_offset;
    log_info("writing to relative_chunk_offset :: %i %i %zu", relative_chunk_offset, chunk_info.chunk_id, chunk_info.chunk_size);
    memcpy(piece + relative_chunk_offset, data, chunk_info.chunk_size);

    hashmap_set(td->data, (char *) &piece_key, piece);

    bitfield_set_bit(td->completed, chunk_info.chunk_id, 1);

    td->downloaded += chunk_info.chunk_size;
    td->left -= chunk_info.chunk_size;

    bitfield_unlock(td->completed);
    return EXIT_SUCCESS;
    error:
    bitfield_unlock(td->completed);
    return EXIT_FAILURE;
}

/* reading data */
int torrent_data_read_data(struct TorrentData * td, void * buff, size_t offset, size_t length) {
    size_t read_length = 0;

    while(read_length < length) {
        // for current offset, get piece and relative offset within piece
        int piece_id = offset / td->piece_size;

        struct PieceInfo piece_info;
        torrent_data_get_piece_info(td, piece_id, &piece_info);

        // get piece
        char piece_key[10] = {0x00};
        sprintf(piece_key, "%i", piece_info.piece_id);
        void * piece = hashmap_get(td->data, (char *) &piece_key);
        if (piece == NULL) {
            throw("couldn't find piece %i", piece_info.piece_id)
        }

        // get relative offset and number of bytes we want to copy from this piece
        int relative_offset = offset - piece_info.piece_offset;
        int bytes_to_read = MIN(piece_info.piece_size - relative_offset, length - read_length);

        // copy
        memcpy(buff + read_length, piece + relative_offset, bytes_to_read);

        // update read_length and read_offset
        read_length += bytes_to_read;
    }

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

/* chunk & piece info */
int torrent_data_get_chunk_info(struct TorrentData * td, int chunk_id, struct ChunkInfo * chunk_info) {
    if(td->chunk_size == 0 || td->data_size == 0){
        throw("can't get chunk info while chunk_size or data_size is set to 0");
    }
    chunk_info->chunk_id = chunk_id;
    chunk_info->chunk_offset = td->chunk_size * chunk_info->chunk_id;
    chunk_info->chunk_size = MIN(td->chunk_size, td->data_size - chunk_info->chunk_offset);
    chunk_info->total_chunks = (int) (td->data_size + (td->chunk_size - 1)) / td->chunk_size;
    chunk_info->piece_id = chunk_info->chunk_offset / td->piece_size;

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int get_piece_id_for_chunk_id(struct TorrentData * td, int chunk_id) {
    if(td->piece_size == 0 || td->data_size == 0) {
        throw("can't get any piece info while piece_size or data_size is set to 0.");
    }
    struct ChunkInfo chunk_info;
    torrent_data_get_chunk_info(td, chunk_id, &chunk_info);

    return chunk_info.chunk_offset / td->piece_size;
    error:
    return -1;
}

int torrent_data_get_piece_info(struct TorrentData * td, int piece_id, struct PieceInfo * piece_info) {
    if(td->piece_size == 0 || td->data_size == 0) {
        throw("can't get any piece info while piece_size or data_size is set to 0.");
    }
    piece_info->piece_id = piece_id;
    piece_info->piece_offset = td->piece_size * piece_info->piece_id;
    piece_info->piece_size = MIN(td->piece_size, td->data_size - piece_info->piece_offset);
    piece_info->total_pieces = (int) (td->data_size + (td->piece_size - 1)) / td->piece_size;

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

/* cleanup */
struct TorrentData * torrent_data_free(struct TorrentData * td) {
    if(td != NULL) {

        if(td->claimed != NULL) {
            td->claimed = bitfield_free(td->claimed);
        }

        if(td->completed != NULL) {
            td->completed = bitfield_free(td->completed);
        }

        if(td->pieces != NULL) {
            td->pieces = bitfield_free(td->pieces);
        }

        if (td->data != NULL) {
            void * piece = hashmap_empty(td->data);
            while (piece != NULL) {
                free(piece);
                piece = hashmap_empty(td->data);
            }

            hashmap_free(td->data);
        }

        struct TorrentDataClaim * current = td->claims;
        while(current != NULL) {
            struct TorrentDataClaim * next = current->next;
            free(current);
            current = NULL;
            current = next;
        }

        pthread_mutex_destroy(&td->initializer_lock);

        free(td);
        td = NULL;
    }

    return td;
}
