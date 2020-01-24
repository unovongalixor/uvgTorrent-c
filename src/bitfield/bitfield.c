//
// Created by vongalixor on 1/23/20.
//
#include "../macros.h"
#include "bitfield.h"
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#define BITS_PER_INT 8

struct Bitfield * bitfield_new(size_t bit_count) {
    struct Bitfield * b = NULL;

    size_t bytes_count = (bit_count + (BITS_PER_INT - 1)) / BITS_PER_INT;
    b = malloc(sizeof(struct Bitfield) + bytes_count);
    if(b == NULL) {
        throw("bitfield failed to malloc");
    }
    b->bit_count = bit_count;


    pthread_mutex_init(&b->mutex, NULL);

    // default all bits unset
    memset(&b->bytes, 0x00, bytes_count);

    return b;
    error:
    return bitfield_free(b);
}

struct Bitfield * bitfield_free(struct Bitfield * b) {
    if (b != NULL) {
        pthread_mutex_destroy(&b->mutex);
        free(b);
        b = NULL;
    }

    return b;
}

void bitfield_set_bit(struct Bitfield * b, int bit, int val) {
    pthread_mutex_lock(&b->mutex);
    if (bit < b->bit_count) {
        int byte_index = bit / BITS_PER_INT;
        int bit_index = bit % BITS_PER_INT;
        int8_t mask = (1 << bit_index);

        if (val == 0) {
            // unset bit (see: https://en.wikipedia.org/wiki/Bit_field)
            b->bytes[byte_index] &= ~mask;
        } else if (val == 1) {
            // set bit (see: https://en.wikipedia.org/wiki/Bit_field)
            b->bytes[byte_index] |= mask;
        }
    }
    pthread_mutex_unlock(&b->mutex);
}

int bitfield_get_bit(struct Bitfield * b, int bit) {
    pthread_mutex_lock(&b->mutex);
    int byte_index = bit / BITS_PER_INT;
    int bit_index = bit % BITS_PER_INT;
    int8_t mask = (1 << bit_index);
    int return_value = (int) ((b->bytes[byte_index] & mask) != 0);
    pthread_mutex_unlock(&b->mutex);
    return return_value;
}