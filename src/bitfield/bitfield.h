#include <stdint.h>

#ifndef UVGTORRENT_C_BITFIELD_H
#define UVGTORRENT_C_BITFIELD_H

struct Bitfield {
    size_t bytes_count;
    int8_t bytes[];
};

extern struct Bitfield * bitfield_new(size_t bit_count);
extern struct Bitfield * bitfield_free(struct Bitfield * b);
extern void bitfield_set_bit(struct Bitfield * b, int bit, int val);
extern int bitfield_get_bit(struct Bitfield * b, int bit);

#endif //UVGTORRENT_C_BITFIELD_H
