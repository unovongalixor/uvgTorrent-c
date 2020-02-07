#include <stdlib.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <pthread.h>
#include "torrent_data.h"
#include "../macros.h"
#include "../bitfield/bitfield.h"
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

    td->piece_size = 0; // number of bytes that make up a piece of this data.
    td->chunk_size = 0; // number of bytes that make up a chunk of a piece of this data.
    td->data_size = 0;  // size of data.

    td->claims = NULL; // linked list of claims to different parts of this data

    td->data = NULL;
    pthread_mutex_init(&td->initializer_lock, NULL);

    return td;
    error:

    return NULL;
}

/* initialization */
void torrent_data_set_piece_size(struct TorrentData * td, size_t piece_size) {
    td->piece_size = piece_size;
}

void torrent_data_set_chunk_size(struct TorrentData * td, size_t chunk_size) {
    td->chunk_size = chunk_size;
}

int torrent_data_set_data_size(struct TorrentData * td, size_t data_size) {
    pthread_mutex_lock(&td->initializer_lock);

    if(td->initialized == 1 & td->data_size != data_size) {
        throw("data already malloced, got unexpected data size");
    } else if(td->initialized == 1 & td->data_size == data_size) {
        goto error;
    }

    td->data_size = data_size;

    // determine how many chunks are needed
    int chunk_count = (int) (td->data_size + (td->chunk_size - 1)) / td->chunk_size;

    // initialize bitfields
    td->claimed = bitfield_new(chunk_count, 0);
    td->completed = bitfield_new(chunk_count, 0);

    // initialize stats
    td->downloaded = ATOMIC_VAR_INIT(0);
    td->left = ATOMIC_VAR_INIT(td->data_size);
    td->uploaded = ATOMIC_VAR_INIT(0);

    // initialize data
    td->data = malloc(td->data_size);
    if(td->data == NULL) {
        throw("failed to malloc ")
    }
    memset(td->data, 0x00, td->data_size);

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

    // get chunk offset
    int chunk_offset = td->chunk_size * chunk_id;
    // get chunk length
    int expected_chunk_size = MIN(td->chunk_size, td->data_size - chunk_offset);

    // validate that we received the expected length
    if(data_size != expected_chunk_size) {
        throw("data lengths mismatch %zu %i", data_size, expected_chunk_size);
    }
    memcpy(td->data + chunk_offset, data, expected_chunk_size);

    bitfield_set_bit(td->completed, chunk_id, 1);

    td->downloaded += expected_chunk_size;
    td->left -= expected_chunk_size;

    bitfield_unlock(td->completed);
    return EXIT_SUCCESS;
    error:
    bitfield_unlock(td->completed);
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

        if(td->data != NULL) {
            free(td->data);
            td->data = NULL;
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
