#include "bitfield/bitfield.h"

static void test_bitfield_get_and_set(void **state) {
    struct Bitfield * b = bitfield_new(10);

    assert_int_equal(bitfield_get_bit(b, 0), 0);
    assert_int_equal(bitfield_get_bit(b, 1), 0);
    assert_int_equal(bitfield_get_bit(b, 2), 0);
    assert_int_equal(bitfield_get_bit(b, 3), 0);
    assert_int_equal(bitfield_get_bit(b, 4), 0);
    assert_int_equal(bitfield_get_bit(b, 5), 0);
    assert_int_equal(bitfield_get_bit(b, 6), 0);
    assert_int_equal(bitfield_get_bit(b, 7), 0);
    assert_int_equal(bitfield_get_bit(b, 8), 0);
    assert_int_equal(bitfield_get_bit(b, 9), 0);
    assert_int_equal(bitfield_get_bit(b, 10), 0);
    assert_int_equal(bitfield_get_bit(b, 11), 0);
    assert_int_equal(bitfield_get_bit(b, 12), 0);

    bitfield_set_bit(b, 2, 1);
    bitfield_set_bit(b, 5, 1);
    bitfield_set_bit(b, 9, 1);

    assert_int_equal(bitfield_get_bit(b, 0), 0);
    assert_int_equal(bitfield_get_bit(b, 1), 0);
    assert_int_equal(bitfield_get_bit(b, 2), 1);
    assert_int_equal(bitfield_get_bit(b, 3), 0);
    assert_int_equal(bitfield_get_bit(b, 4), 0);
    assert_int_equal(bitfield_get_bit(b, 5), 1);
    assert_int_equal(bitfield_get_bit(b, 6), 0);
    assert_int_equal(bitfield_get_bit(b, 7), 0);
    assert_int_equal(bitfield_get_bit(b, 8), 0);
    assert_int_equal(bitfield_get_bit(b, 9), 1);
    assert_int_equal(bitfield_get_bit(b, 10), 0);
    assert_int_equal(bitfield_get_bit(b, 11), 0);
    assert_int_equal(bitfield_get_bit(b, 12), 0);

    bitfield_set_bit(b, 2, 0);
    bitfield_set_bit(b, 5, 0);
    bitfield_set_bit(b, 9, 0);
    bitfield_set_bit(b, 10, 1); //  we only have 10 zero bits to work with, this set should do nothing

    assert_int_equal(bitfield_get_bit(b, 0), 0);
    assert_int_equal(bitfield_get_bit(b, 1), 0);
    assert_int_equal(bitfield_get_bit(b, 2), 0);
    assert_int_equal(bitfield_get_bit(b, 3), 0);
    assert_int_equal(bitfield_get_bit(b, 4), 0);
    assert_int_equal(bitfield_get_bit(b, 5), 0);
    assert_int_equal(bitfield_get_bit(b, 6), 0);
    assert_int_equal(bitfield_get_bit(b, 7), 0);
    assert_int_equal(bitfield_get_bit(b, 8), 0);
    assert_int_equal(bitfield_get_bit(b, 9), 0);
    assert_int_equal(bitfield_get_bit(b, 10), 0);
    assert_int_equal(bitfield_get_bit(b, 10), 0);
    assert_int_equal(bitfield_get_bit(b, 11), 0);
    assert_int_equal(bitfield_get_bit(b, 12), 0);

    bitfield_free(b);
}
