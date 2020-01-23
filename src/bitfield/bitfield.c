//
// Created by vongalixor on 1/23/20.
//
#include "../macros.h"
#include "bitfield.h"
#include <stdint.h>
#include <stdlib.h>

struct Bitfield * bitfield_new(size_t bit_count) {
    struct Bitfield * b = NULL;

    size_t bytes_count = (bit_count + (sizeof(int8_t) - 1)) / sizeof(int8_t);

    b = malloc(bytes_count);
    if(b == NULL) {
        throw("bitfield failed to malloc");
    }
    b->bytes_count = bytes_count;

    // default all bits unset
    memset(b->bytes, 0x00, bytes_count);

    return b;
    error:
    return bitfield_free(b);
}

struct Bitfield * bitfield_free(struct Bitfield * b) {
    if (b != NULL) {
        free(b);
        b = NULL;
    }

    return b;
}
void bitfield_set_bit(struct Bitfield * b, int bit, int val) {
    int byte_index = bit / sizeof(int8_t);
    int bit_index = bit % sizeof(int8_t);
    int8_t mask = (1 << bit_index);

    if (val == 0) {
        // unset bit (see: https://en.wikipedia.org/wiki/Bit_field)
        b->bytes[byte_index] &= ~mask;
    } else if (val == 1) {
        // set bit (see: https://en.wikipedia.org/wiki/Bit_field)
        b->bytes[byte_index] |= mask;
    }
}
int bitfield_get_bit(struct Bitfield * b, int bit) {

    int byte_index = bit / sizeof(int8_t);
    int bit_index = bit % sizeof(int8_t);
    int8_t mask = (1 << bit_index);

    return (int) (b->bytes[byte_index] & mask);
}