//
// Created by vongalixor on 1/23/20.
//
#include "../macros.h"
#include "bitfield.h"
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#define BITS_PER_INT 8

struct Bitfield * bitfield_new(size_t bit_count, int default_bit_value, int default_byte_value) {
    struct Bitfield * b = NULL;

    size_t bytes_count = (bit_count + (BITS_PER_INT - 1)) / BITS_PER_INT;
    b = malloc(sizeof(struct Bitfield) + bytes_count);
    if(b == NULL) {
        throw("bitfield failed to malloc");
    }
    b->bit_count = bit_count;
    b->bytes_count = bytes_count;

    pthread_mutex_init(&b->mutex, NULL);

    // set default values. unused bits will be set to 0xFF so we can still easily verify the last byte
    memset(&b->bytes, default_byte_value, b->bytes_count);
    for (int i = 0; i < b->bit_count; i++) {
        bitfield_set_bit(b, i, default_bit_value);
    }

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
    if (bit < b->bit_count) {
        int byte_index = bit / BITS_PER_INT;
        int bit_index = bit % BITS_PER_INT;
        // uint8_t mask = (1 << bit_index);
        uint8_t mask = (0x80 >> bit_index); // highest ordered bit is indexed 0
                                            // 0x80 == 0b10000000

        if (val == 0) {
            // unset bit (see: https://en.wikipedia.org/wiki/Bit_field)
            b->bytes[byte_index] &= ~mask;
        } else if (val == 1) {
            // set bit (see: https://en.wikipedia.org/wiki/Bit_field)
            b->bytes[byte_index] |= mask;
        }
    }
}

int bitfield_get_bit(struct Bitfield * b, int bit) {
    int byte_index = bit / BITS_PER_INT;
    int bit_index = bit % BITS_PER_INT;
    // uint8_t mask = (1 << bit_index);
    uint8_t mask = (0x80 >> bit_index);
    int return_value = (int) ((b->bytes[byte_index] & mask) != 0);
    return return_value;
}

void bitfield_lock(struct Bitfield * b){
    pthread_mutex_lock(&b->mutex);
}

void bitfield_unlock(struct Bitfield * b) {
    pthread_mutex_unlock(&b->mutex);
}