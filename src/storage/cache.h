#ifndef CACHE_H
#define CACHE_H
#include <sys/time.h>

#include "pthread.h"
#include "storage/message.h"
#define CACHE_SIZE_MAX 64
struct rr_cache_linked_node {
    struct rr_cache_linked_node *next, *last;
    struct resource_record* element;
    struct timeval expire_time;
};
struct rr_cache {
    struct rr_cache_linked_node* dummy;
    uint32_t size;
    pthread_mutex_t lock;
};

extern struct rr_cache cache;
void init_cache();
// copy the resource record in cache
// LRU enabled
int cache_put_rr(struct resource_record*);

// find cache records by _domain_, _type_, _class_
// and put it in the _dest_
int cache_find_rr(struct resource_record*, uint8_t*, uint16_t, uint16_t);

// refresh cache for each rr
// ttl = 0 will be removed
void refresh_cache();
#endif