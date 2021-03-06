#include <stdint.h>
#include <pthread.h>

#ifndef UVGTORRENT_C_BITFIELD_H
#define UVGTORRENT_C_BITFIELD_H

#define BITS_PER_INT 8

struct Bitfield {
    pthread_mutex_t mutex;
    size_t bit_count;
    size_t bytes_count;
    uint8_t bytes[];
};

/**
 * @brief alloc a new bitfield
 * @param bit_count
 * @param excess_value value for any trailing bits (should be 0 for bitfield msg, 1 for bitfields used for validation)
 * @return struct Bitfield *. NULL on failure
 */
extern struct Bitfield * bitfield_new(size_t bit_count, int default_bit_value, int default_byte_value);

/**
 * @brief free & null the given bitfield
 * @param b
 * @return NULL on success
 */
extern struct Bitfield * bitfield_free(struct Bitfield * b);

/**
 * @brief set the given bit to val
 * @param b
 * @param bit
 * @param val should be 1 or 0
 */
extern void bitfield_set_bit(struct Bitfield * b, int bit, int val);

/**
 * @brief get the value for a given bit.
 * @param b
 * @param bit
 * @return 1 or 0
 */
extern int bitfield_get_bit(struct Bitfield * b, int bit);

extern void bitfield_lock(struct Bitfield * b);
extern void bitfield_unlock(struct Bitfield * b);

#endif //UVGTORRENT_C_BITFIELD_H
