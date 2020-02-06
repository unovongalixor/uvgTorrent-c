#include <stdlib.h>
#include <inttypes.h>
#include "torrent_data.h"
#include "../macros.h"
#include "../bitfield/bitfield.h"
#include "../deadline/deadline.h"

struct TorrentData * torrent_data_new() {
    struct TorrentData * td = malloc(sizeof(struct TorrentData));
    if(td == NULL) {
        throw("torrent_data failed to malloc");
    }

    td->needed = 0; // are there chunks of this data that peers should be requesting?
    td->claimed = NULL; // bitfield indicating whether each chunk is currently claimed by someone else.
    td->completed = NULL; // bitfield indicating whether each chunk is completed yet or not

    td->piece_size = 0; // number of bytes that make up a piece of this data.
    td->chunk_size = 0; // number of bytes that make up a chunk of a piece of this data.
    td->data_size = 0;  // size of data.
    td->chunk_count = 0; // number of chunks in this data

    td->claims = NULL; // linked list of claims to different parts of this data

    td->data = NULL;

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
    if(td->data != NULL) {
        throw("data already malloced, cant set data size");
    }

    td->data_size = data_size;

    // determine how many chunks are needed
    td->chunk_count = (int) (td->data_size + (td->chunk_size - 1)) / td->chunk_size;

    // initialize bitfields
    td->claimed = bitfield_new(td->chunk_count, 0);
    td->completed = bitfield_new(td->chunk_count, 0);

    // initialize data
    td->data = malloc(td->data_size);
    if(td->data == NULL) {
        throw("failed to malloc ")
    }
    memset(td->data, 0x00, td->data_size);

    return EXIT_SUCCESS;
    error:

    return EXIT_FAILURE;
};

/* claiming data */
int torrent_data_claim_chunk(struct TorrentData * td) {
    bitfield_lock(td->claimed);
    for (int i = 0; i < (td->claimed->bit_count); i++) {
        if (bitfield_get_bit(td->claimed, i) == 0) {
            bitfield_set_bit(td->claimed, i, 1);

            struct TorrentDataClaim ** claim = &td->claims;
            while(*claim != NULL) {
                claim = &(*claim)->next;
            }

            *claim = malloc(sizeof(struct TorrentDataClaim));
            (*claim)->deadline = now() + 2000;
            (*claim)->chunk_id = i;
            (*claim)->next = NULL;

            bitfield_unlock(td->claimed);
            return EXIT_SUCCESS;
        }
    }

    bitfield_unlock(td->claimed);
    return EXIT_FAILURE;
}

int torrent_data_release_claims(struct TorrentData * td) {
    bitfield_lock(td->claimed);

    struct TorrentDataClaim ** last_claim = NULL;
    struct TorrentDataClaim ** claim = &td->claims;

    while(*claim != NULL) {
        if((*claim)->deadline < now()) {
            bitfield_set_bit(td->claimed, (*claim)->chunk_id, 0);

            if(last_claim != NULL) {
                (*last_claim)->next = (*claim)->next;
            } else {
                td->claims = (*claim)->next;
            }
            struct TorrentDataClaim ** next_claim = &(*claim)->next;
            free(*claim);
            claim = next_claim;
        } else {
            *claim = (*claim)->next;
            last_claim = claim;
        }
    }

    bitfield_unlock(td->claimed);
    return EXIT_SUCCESS;
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

        struct TorrentDataClaim ** claim = &td->claims;
        while(*claim != NULL) {
            struct TorrentDataClaim ** next = &(*claim)->next;
            free(*claim);
            *claim = NULL;
            claim = next;
        }
        td->claims = NULL;

        free(td);
        td = NULL;
    }
}
