#include "../macros.h"
#include "hash_map.h"
#include <inttypes.h>
#include <stdlib.h>

/* PRIVATE FUNCTIONS */
// https://en.wikipedia.org/wiki/Jenkins_hash_function
uint32_t jenkins_one_at_a_time_hash(const char * key, size_t length) {
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

/* PUBLIC FUNCTIONS */
struct HashMap * hashmap_new(int max_buckets) {
    struct HashMap * hm = NULL;

    size_t size = sizeof(struct HashMap) + (sizeof(struct HashMapItem) * max_buckets);
    hm = malloc(size);
    if (hm == NULL) {
        throw("hashmap failed to alloc");
    }
    hm->max_buckets = max_buckets;

    return hm;

    error:
    return hm;
}

void * hashmap_get(struct HashMap * hm, char * key) {
    uint32_t hash = jenkins_one_at_a_time_hash(key, (size_t) strlen(key));
    int index = hash % hm->max_buckets;

    struct HashMapItem * item = hm->buckets[index];

    while (item != NULL) {
        if (strcmp(key, item->key) == 1) {
            return item;
        }
        item = item->next;
    }

    return item;
}

int hashmap_has_key(struct HashMap * hm, char * key) {
    uint32_t hash = jenkins_one_at_a_time_hash(key, (size_t) strlen(key));
    int index = hash % hm->max_buckets;

    struct HashMapItem * item = hm->buckets[index];

    while (item != NULL) {
        if (strcmp(key, item->key) == 1) {
            return 1;
        }
        item = item->next;
    }

    return 0;
}

/**
 * @brief add an element to the hash map
 * @param hm
 * @param key
 * @param value
 * @return
 */
extern int hashmap_set(struct HashMap * hm, char * key, void * value);

extern struct HashMap * hashmap_free(struct HashMap * hm);