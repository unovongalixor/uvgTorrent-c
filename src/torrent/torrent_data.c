#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include "torrent_data.h"
#include "../log.h"
#include "../bitfield/bitfield.h"
#include "../hash_map/hash_map.h"
#include "../deadline/deadline.h"
#include "../sha1/sha1.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct TorrentData * torrent_data_new(char * root_path) {
    struct TorrentData * td = malloc(sizeof(struct TorrentData));
    if(td == NULL) {
        throw("torrent_data failed to malloc");
    }

    td->root_path = root_path;
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
    td->completed_pieces = 0;
    td->chunk_count = 0;

    td->claims = NULL; // linked list of claims to different parts of this data

    td->downloaded = ATOMIC_VAR_INIT(0);
    td->left = ATOMIC_VAR_INIT(0);
    td->uploaded = ATOMIC_VAR_INIT(0);

    td->data = NULL;
    pthread_mutex_init(&td->initializer_lock, NULL);

    td->sha1_hashes = NULL;
    td->sha1_hashes_len = 0;

    return td;
    error:

    return NULL;
}

/* initialization */
void torrent_data_set_piece_size(struct TorrentData * td, size_t piece_size) {
    if(td->initialized == 1) {
        log_error("can't set piece size after setting data size");
    }
    td->piece_size = piece_size;
}

void torrent_data_set_chunk_size(struct TorrentData * td, size_t chunk_size) {
    if(td->initialized == 1) {
        log_error("can't set chunk size after setting data size");
    }
    td->chunk_size = chunk_size;
}

int torrent_data_add_file(struct TorrentData * td, char * path, uint64_t length) {
    struct TorrentDataFileInfo * file = malloc(sizeof(struct TorrentDataFileInfo));
    if(file == NULL) {
        throw("failed to add file :: %s", path);
    }
    file->fp = NULL;

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

void torrent_data_set_sha1_hashes(struct TorrentData * td, char * sha1_hashes, size_t sha1_hashes_len) {
    td->sha1_hashes_len = (size_t) sha1_hashes_len;
    log_info("td->sha1_hashes_len %zu", td->sha1_hashes_len);
    td->sha1_hashes = malloc(td->sha1_hashes_len);
    memcpy(td->sha1_hashes, sha1_hashes, td->sha1_hashes_len);
}

int torrent_data_validate_piece(struct TorrentData * td, struct PieceInfo piece_info, void * piece_data) {
    if(td->sha1_hashes == NULL) {
        return EXIT_SUCCESS;
    }

    if (piece_info.piece_id*20 > td->sha1_hashes_len-20) {
        throw("tried to get sha1 hash from beyond bounds");
    }

    SHA1_CTX sha;
    uint8_t hash[20] = {0x00};

    SHA1Init(&sha);
    SHA1Update(&sha, piece_data, piece_info.piece_size);
    SHA1Final(hash, &sha);

    size_t sha1_offset = piece_info.piece_id*20;
    if(strncmp((char *)&hash, td->sha1_hashes + sha1_offset, 20) != 0){
        throw("piece validation failed");
    }

    return EXIT_SUCCESS;

    error:
    return EXIT_FAILURE;
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
int torrent_data_claim_chunk(struct TorrentData * td, struct Bitfield * interested_chunks, int timeout_seconds, int num_chunks, int * out) {
    int found_a_chunk = 0;

    if(td->initialized == 1) {
        bitfield_lock(td->claimed);
        for (int chunk = 0; chunk < num_chunks; chunk++) {
            for (int i = 0; i < (td->claimed->bit_count); i++) {
                if (bitfield_get_bit(td->claimed, i) == 0 && bitfield_get_bit(interested_chunks, i) == 1) {
                    if (timeout_seconds != 0) {
                        bitfield_set_bit(td->claimed, i, 1);

                        struct TorrentDataClaim *claim = malloc(sizeof(struct TorrentDataClaim));
                        claim->deadline = now() + (timeout_seconds * 1000);
                        claim->chunk_id = i;
                        if (td->claims == NULL) {
                            claim->next = NULL;
                        } else {
                            claim->next = td->claims;
                        }
                        td->claims = claim;
                    }

                    found_a_chunk = 1;
                    *(out + chunk) = i;
                    break;
                }
            }
        }
        bitfield_unlock(td->claimed);

        if(found_a_chunk == 1) {
            return EXIT_SUCCESS;
        } else {
            return EXIT_FAILURE;
        }
    }

    return EXIT_FAILURE;
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
int mkpath(char* file_path, mode_t mode) {
    assert(file_path && *file_path);
    for (char* p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}


int torrent_data_write_chunk(struct TorrentData * td, int chunk_id, void * data, size_t data_size) {
    int return_value = EXIT_FAILURE;
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

    log_debug("got chunk %i / %i", chunk_id, td->chunk_count);

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

    int piece_already_completed = torrent_data_is_piece_complete(td, piece_info.piece_id);
    bitfield_set_bit(td->completed, chunk_info.chunk_id, 1);

    // check if entire piece is done
    if(torrent_data_is_piece_complete(td, piece_info.piece_id) == 1 && piece_already_completed == 0) {
        if(torrent_data_validate_piece(td, piece_info, piece) == EXIT_SUCCESS) {
            return_value = EXIT_SUCCESS;
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

                    if(current_file->fp == NULL) {
                        if(access(current_file->file_path, F_OK) == -1) {
                            // create folders in needed file path
                            mkpath(current_file->file_path, 0755);
                            FILE *fp = fopen(current_file->file_path, "wb+");
                            fclose(fp);
                        }
                        current_file->fp = fopen(current_file->file_path, "rb+");
                        if(current_file->fp == NULL) {
                            throw("failed to open file %s", current_file->file_path);
                        }
                    }

                    fseek(current_file->fp, relative_offset, SEEK_SET);

                    size_t expected_bytes_written = fwrite(piece + data_written, 1, bytes_to_write, current_file->fp);
                    fflush(current_file->fp);
                    if (expected_bytes_written != bytes_to_write) {
                        fclose(current_file->fp);
                        current_file->fp = NULL;
                        throw("wrong number of bytes written");
                    }

                    data_written += bytes_to_write;
                }

                current_file = current_file->next;
            }

            if (data_written != piece_info.piece_size) {
                throw("failed to write piece %i to disk", piece_info.piece_id);
            }

            free(piece);

            td->completed_pieces++;
            if(torrent_data_is_complete(td) == 1) {
                td->needed = 0;
            }
        } else {
            return_value = EXIT_FAILURE;
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
    return return_value;
    error:
    bitfield_unlock(td->completed);
    return EXIT_FAILURE;
}

/* reading data */
int torrent_data_read_data(struct TorrentData * td, void * buff, size_t offset, size_t length) {
    size_t read_length = 0;

    uint8_t * piece_buffer = NULL;

    while(read_length < length) {
        // for current offset, get piece and relative offset within piece
        int piece_id = offset / td->piece_size;

        struct PieceInfo piece_info;
        torrent_data_get_piece_info(td, piece_id, &piece_info);

        // get piece
        piece_buffer = malloc(piece_info.piece_size + 1);
        memset(piece_buffer, 0x00, piece_info.piece_size + 1);

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

                size_t expected_bytes_read = fread(piece_buffer + piece_data_read, 1, bytes_to_read, fp);
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
        memcpy(buff + read_length, piece_buffer + relative_offset, bytes_to_read);

        // update read_length and read_offset
        read_length += bytes_to_read;

        free(piece_buffer);
    }

    return EXIT_SUCCESS;
    error:

    if(piece_buffer != NULL) {
        free(piece_buffer);
    }
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
        int byte_index = (piece_info.piece_id * uints_per_piece) + i;
        if(byte_index < td->completed->bytes_count) {
            if (td->completed->bytes[byte_index] != 0xFF) {
                piece_complete = 0;
                break;
            }
        }
    }

    return piece_complete;
}

int torrent_data_is_complete(struct TorrentData *td) {
    if(td->piece_count == 0) {
        return 0;
    }
    return td->completed_pieces == td->piece_count;
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

        if(td->sha1_hashes != NULL) {
            free(td->sha1_hashes);
            td->sha1_hashes = NULL;
        }

        if(td->files != NULL) {
            struct TorrentDataFileInfo * file = td->files;
            while (file != NULL) {
                struct TorrentDataFileInfo * next_file = file->next;
                if(file->fp != NULL) {
                    fclose(file->fp);
                }
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
