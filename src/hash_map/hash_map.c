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

struct HashMapItem * hashmap_item_free(struct HashMapItem * item) {
    if (item != NULL) {
        if(item->key != NULL) {
            free(item->key);
            item->key = NULL;
        }
        free(item);
        item = NULL;
    }

    return item;
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
            void * value = item->value;
            hashmap_item_free(item);

            return value;
        }
        item = item->next;
    }

    return NULL;
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

int hashmap_set(struct HashMap * hm, char * key, void * value) {
    if(hashmap_has_key(hm, key)) {
        throw("hash_map already has key %s set", key);
    }

    /* create item */
    struct HashMapItem * item = NULL;

    item = malloc(sizeof(struct HashMapItem));
    if (item == NULL) {
        throw("HashMapItem failed to malloc");
    }

    item->key = NULL;
    item->key = strndup(key, strlen(key));
    item->value = value;

    uint32_t hash = jenkins_one_at_a_time_hash(key, (size_t) strlen(key));
    int index = hash % hm->max_buckets;

    struct HashMapItem * existing_item = hm->buckets[index];
    if (existing_item == NULL) {
        hm->buckets[index] = item;
    } else {
        while (item->next != NULL) {
            item = item->next;
        }
        item->next = item;
    }

    return EXIT_SUCCESS;

    error:
    hashmap_item_free(item);
    return EXIT_FAILURE;
}


struct HashMap * hashmap_free(struct HashMap * hm) {
    if (hm != NULL) {
        free(hm);

    }
}