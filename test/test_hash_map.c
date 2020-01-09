#include "hash_map/hash_map.h"

static void test_hashmap_get_and_set(void **state) {
    (void) state;

    struct HashMap * hm = hashmap_new(100);

    int a = 10;
    int b = 20;
    int c = 30;

    hashmap_set(hm, "a", &a);
    hashmap_set(hm, "b", &b);
    hashmap_set(hm, "c", &c);

    int a_get = * (int *) hashmap_get(hm, "a");
    int b_get = * (int *) hashmap_get(hm, "b");
    int c_get = * (int *) hashmap_get(hm, "c");

    assert_int_equal(a_get, a);
    assert_int_equal(b_get, b);
    assert_int_equal(c_get, c);

    hashmap_free(hm);
}

static void test_hashmap_get_and_set_collision(void **state) {
    (void) state;

    struct HashMap * hm = hashmap_new(1);

    int a = 10;
    int b = 20;
    int c = 30;

    hashmap_set(hm, "a", &a);
    hashmap_set(hm, "b", &b);
    hashmap_set(hm, "c", &c);

    int b_get = * (int *) hashmap_get(hm, "b");
    int a_get = * (int *) hashmap_get(hm, "a");
    int c_get = * (int *) hashmap_get(hm, "c");

    assert_int_equal(a_get, a);
    assert_int_equal(b_get, b);
    assert_int_equal(c_get, c);

    hashmap_free(hm);
}

static void test_hashmap_empty_collision(void **state) {
    (void) state;

    struct HashMap * hm = hashmap_new(1);

    int a = 10;
    int b = 20;
    int c = 30;

    hashmap_set(hm, "a", &a);
    hashmap_set(hm, "b", &b);
    hashmap_set(hm, "c", &c);

    int * value = (int *) hashmap_empty(hm);
    while (value != NULL) {
        value = (int *) hashmap_empty(hm);
    }

    hashmap_free(hm);
}

static void test_hashmap_empty_malloc(void **state) {
    struct HashMap * hm = hashmap_new(1);

    int * a = malloc(sizeof(int));
    *a = 10;
    int * b = malloc(sizeof(int));
    *b = 20;
    int * c = malloc(sizeof(int));
    *c = 30;

    hashmap_set(hm, "c", c);
    hashmap_set(hm, "a", a);
    hashmap_set(hm, "b", b);

    int * value = (int *) hashmap_empty(hm);
    while (value != NULL) {
        free(value);
        value = (int *) hashmap_empty(hm);
    }

    hashmap_free(hm);
}