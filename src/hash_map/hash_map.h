#ifndef UVGTORRENT_C_HASH_MAP_H
#define UVGTORRENT_C_HASH_MAP_H

struct HashMapItem {
    char * key;
    void * value;
    struct HashMapItem * next;
};

struct HashMap {
    int max_buckets;
    struct HashMapItem * buckets[];
};

/**
 * @brief alloc a new hash map
 * @param max_buckets maximum number of buckets for storing elements. a higher value reduces hash collisions
 * @return struct HashMap *. NULL on failure.
 */
extern struct HashMap * hashmap_new(int max_buckets);

/**
 * @brief retrieve an element from the hash map
 * @param hm
 * @param key
 * @return void *. NULL if the key doesn't exist in the hash map
 */
extern void * hashmap_get(struct HashMap * hm, char * key);

/**
 * @brief check if a key is set in the given hashmap
 * @param hm
 * @param key
 * @return 1 or 0
 */
extern int hashmap_has_key(struct HashMap * hm, char * key);

/**
 * @brief add an element to the hash map
 * @param hm
 * @param key
 * @param value
 * @return
 */
extern int hashmap_set(struct HashMap * hm, char * key, void * value);

/**
 * @brief free the given hashmap
 * @param hm
 * @return struct HashMap *. NULL on success
 */
extern struct HashMap * hashmap_free(struct HashMap * hm);

#endif //UVGTORRENT_C_HASH_MAP_H
