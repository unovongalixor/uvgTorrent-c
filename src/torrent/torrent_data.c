#include <stdlib.h>
#include<stdio.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include "torrent_data.h"
#include "../macros.h"
#include "../bitfield/bitfield.h"
#include "../hash_map/hash_map.h"
#include "../deadline/deadline.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct TorrentData * torrent_data_new(char * root_path) {
    struct TorrentData * td = malloc(sizeof(struct TorrentData));
    if(td == NULL) {
        throw("torrent_data failed to malloc");
    }

    td->root_path = root_path;
    td->is_completed = ATOMIC_VAR_INIT(0);
    td->needed = ATOMIC_VAR_INIT(0); // are there chunks of this data that peers should be requesting?
    td->initialized = ATOMIC_VAR_INIT(0);
    td->claimed = NULL; // bitfield indicating whether each chunk is currently claimed by someone else.
    td->completed = NULL; // bitfield indicating whether each chunk is completed yet or not

    td->files = NULL;
    td->files_size = 0;

    td->piece_size = 0; // number of bytes that make up a piece of this data.
    td->chunk_size = 0; // number of bytes that make up a chunk of a piece of this data.
    td->data_size = 0;  // size of data.
    td->piece_count = 0;
    td->chunk_count = 0;

    td->claims = NULL; // linked list of claims to different parts of this data

    td->downloaded = ATOMIC_VAR_INIT(0);
    td->left = ATOMIC_VAR_INIT(0);
    td->uploaded = ATOMIC_VAR_INIT(0);

    td->validate_piece = NULL;

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

int torrent_data_add_file(struct TorrentData * td, char * path, uint64_t length) {
    struct TorrentDataFileInfo * file = malloc(sizeof(struct TorrentDataFileInfo));
    if(file == NULL) {
        throw("failed to add file :: %s", path);
    }
    char file_path[4096]; // 4096 unix max path size
    memset(&file_path, 0x00, sizeof(file_path));

    strncat((char *) &file_path, td->root_path, strlen(td->root_path));
    strncat((char *) &file_path, path, strlen(path));

    file->file_path = strndup(file_path, strlen(file_path));
    if (file->file_path == NULL) {
        throw("failed to add file :: %s", path);
    }
    file->file_size = (size_t) length;
    file->file_offset = td->files_size;
    file->next = NULL;

    struct TorrentDataFileInfo ** current = &td->files;
    while(*current != NULL) {
        current = &((*current)->next);
    }
    td->files_size += file->file_size;
    *current = file;

    log_info("added file :: %s", file->file_path);
    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
}

int torrent_data_set_validate_piece_function(struct TorrentData * td, int (*validate_piece)(struct TorrentData * td, struct PieceInfo piece_info, void * piece_data)) {
    td->validate_piece = validate_piece;

    return EXIT_SUCCESS;
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

    // determine how many chunks are needed
    td->piece_count = (td->data_size + (td->piece_size - 1)) / td->piece_size;
    td->chunk_count = (td->data_size + (td->chunk_size - 1)) / td->chunk_size;

    // initialize bitfields
    td->claimed = bitfield_new((int) td->chunk_count, 0, 0xFF);
    td->completed = bitfield_new((int) td->chunk_count, 0, 0xFF);

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
int torrent_data_claim_chunk(struct TorrentData * td, struct Bitfield * interested_chunks) {
    if(td->initialized == 1) {
        bitfield_lock(td->claimed);
        for (int i = 0; i < (td->claimed->bit_count); i++) {
            if (bitfield_get_bit(td->claimed, i) == 0 && bitfield_get_bit(interested_chunks, i) == 1) {
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
        piece = malloc(td->piece_size);
        memset(piece, 0x00, td->piece_size);
    }

    int relative_chunk_offset = chunk_info.chunk_offset - piece_info.piece_offset;
    memcpy(piece + relative_chunk_offset, data, chunk_info.chunk_size);

    bitfield_set_bit(td->completed, chunk_info.chunk_id, 1);

    // check if entire piece is done
    if(torrent_data_is_piece_complete(td, piece_info.piece_id) == 1) {
        log_info("piece %i complete", piece_info.piece_id);

        // if it is then validate
        int valid = 1;
        if(td->validate_piece != NULL) {
            valid = td->validate_piece(td, piece_info, piece);
        }

        if(valid == 1) {
            // find first file overlapping with the piece
            struct TorrentDataFileInfo * current_file = td->files;
            size_t data_written = 0;

            size_t piece_begin = piece_info.piece_offset;
            size_t piece_end = piece_begin + piece_info.piece_size;

            while(data_written != piece_info.piece_size) {
                if (current_file == NULL) {
                    break;
                }

                size_t file_begin = current_file->file_offset;
                size_t file_end = file_begin + current_file->file_size;

                if (file_end >= piece_begin && file_begin <= piece_end) {

                    size_t relative_offset = (piece_begin + data_written) - file_begin;
                    int bytes_to_write = MIN(current_file->file_size - relative_offset, piece_info.piece_size - data_written);

                    if(access(current_file->file_path, F_OK) == -1) {
                        FILE *fp = fopen(current_file->file_path, "w+");
                        fseek(fp, current_file->file_size, SEEK_SET);
                        fputc(0x00, fp);
                        fclose(fp);
                    }

                    FILE *fp = fopen(current_file->file_path, "rb+");
                    if(fp == NULL) {
                        throw("failed to open file %s", current_file->file_path);
                    }
                    fseek(fp, relative_offset, SEEK_SET);

                    size_t expected_bytes_written = fwrite(piece + data_written, 1, bytes_to_write, fp);
                    if (expected_bytes_written != bytes_to_write) {
                        fclose(fp);
                        throw("wrong number of bytes written");
                    }
                    fclose(fp);

                    data_written += bytes_to_write;
                }

                current_file = current_file->next;
            }

            if (data_written != piece_info.piece_size) {
                throw("failed to write piece %i to disk", piece_info.piece_id);
            }

            free(piece);
        } else {
            // hold unfinished pieces in memory
            hashmap_set(td->data, (char *) &piece_key, piece);
        }
    } else {
        // hold unfinished pieces in memory
        hashmap_set(td->data, (char *) &piece_key, piece);
    }

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
        uint8_t piece_buffer[piece_info.piece_size + 1];
        memset(&piece_buffer, 0x00, piece_info.piece_size);
        void * piece = &piece_buffer;
        size_t piece_data_read = 0;

        size_t piece_begin = piece_info.piece_offset;
        size_t piece_end = piece_begin + piece_info.piece_size;

        struct TorrentDataFileInfo * current_file = td->files;

        while(piece_data_read != piece_info.piece_size) {
            if (current_file == NULL) {
                break;
            }

            size_t file_begin = current_file->file_offset;
            size_t file_end = file_begin + current_file->file_size;

            if (file_end >= piece_begin && file_begin <= piece_end) {
                size_t relative_offset = (piece_begin + piece_data_read) - file_begin;
                int bytes_to_read = MIN(current_file->file_size - relative_offset, piece_info.piece_size - piece_data_read);

                FILE *fp = fopen(current_file->file_path, "rb");
                if(fp == NULL) {
                    throw("failed to open file %s", current_file->file_path);
                }
                fseek(fp, relative_offset, SEEK_SET);

                size_t expected_bytes_read = fread(piece + piece_data_read, 1, bytes_to_read, fp);
                if (expected_bytes_read != bytes_to_read) {
                    fclose(fp);
                    throw("wrong number of bytes read %zu %i %s", expected_bytes_read, bytes_to_read, current_file->file_path);
                }
                fclose(fp);

                piece_data_read += expected_bytes_read;
            }

            current_file = current_file->next;
        }

        if (piece_data_read != piece_info.piece_size) {
            throw("failed to read piece %i from disk", piece_info.piece_id);
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
    chunk_info->piece_id = chunk_info->chunk_offset / td->piece_size;

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int torrent_data_get_piece_info(struct TorrentData * td, int piece_id, struct PieceInfo * piece_info) {
    if(td->piece_size == 0 || td->data_size == 0) {
        throw("can't get any piece info while piece_size or data_size is set to 0.");
    }
    piece_info->piece_id = piece_id;
    piece_info->piece_offset = td->piece_size * piece_info->piece_id;
    piece_info->piece_size = MIN(td->piece_size, td->data_size - piece_info->piece_offset);

    return EXIT_SUCCESS;
    error:
    return EXIT_FAILURE;
}

int torrent_data_is_piece_complete(struct TorrentData *td, int piece_id) {
    // get piece info
    struct PieceInfo piece_info;
    torrent_data_get_piece_info(td, piece_id, &piece_info);

    int chunks_per_piece = td->piece_size / td->chunk_size;
    int uints_per_piece = chunks_per_piece / 8;
    int piece_complete = 1;

    for(int i = 0; i < uints_per_piece; i++) {
        int byte_index = piece_info.piece_id + i;
        if(byte_index < td->completed->bytes_count) {
            if (td->completed->bytes[piece_info.piece_id + i] != 0xFF) {
                piece_complete = 0;
                break;
            }
        }
    }

    return piece_complete;
}

int torrent_data_check_if_complete(struct TorrentData *td) {
    if(td->is_completed == 1) {
        return td->is_completed;
    }

    for(int i = 0; i < td->piece_count; i++) {
        if(torrent_data_is_piece_complete(td, i) == 0) {
            return EXIT_FAILURE;
        }
    }

    td->is_completed = 1;
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

        if(td->files != NULL) {
            struct TorrentDataFileInfo * file = td->files;
            while (file != NULL) {
                struct TorrentDataFileInfo * next_file = file->next;
                free(file->file_path);
                free(file);
                file = NULL;
                file = next_file;
            }
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
