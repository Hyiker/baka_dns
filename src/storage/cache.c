#include "storage/cache.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/logging.h"
struct rr_cache cache;
pthread_t timer_thread;
static struct rr_cache_linked_node* create_cache_node(
    struct rr_cache_linked_node* next, struct rr_cache_linked_node* last,
    struct resource_record* rr) {
    struct rr_cache_linked_node* ln =
        malloc(sizeof(struct rr_cache_linked_node));
    memset(ln, 0, sizeof(struct rr_cache_linked_node));
    ln->next = next;
    ln->last = last;
    gettimeofday(&ln->expire_time, NULL);
    if (rr != NULL) {
        ln->expire_time.tv_sec += rr->ttl;
        ln->element = malloc(sizeof(struct resource_record));
        rr_copy(ln->element, rr);
    }
    return ln;
}

int cache_find_rr(struct resource_record* dest, uint8_t* domain, uint16_t type,
                  uint16_t _class) {
    pthread_mutex_lock(&cache.lock);
    // TODO: optimize performance
    struct rr_cache_linked_node* node = cache.dummy->next;
    int n = 0;
    while (node) {
        if (strncmp(domain, node->element->name,
                    domain_len(node->element->name)) == 0 &&
            type == node->element->type && _class == node->element->_class) {
            rr_copy(dest, node->element);
            dest++;
            n++;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&cache.lock);
    return n;
}
// strictly match a specific rr
static struct rr_cache_linked_node* cache_match_ln(struct resource_record* rr) {
    struct rr_cache_linked_node* node = cache.dummy->next;
    while (node) {
        if (rr_cmp(node->element, rr)) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

int cache_put_rr(struct resource_record* rr) {
    pthread_mutex_lock(&cache.lock);
    struct rr_cache_linked_node* old = cache_match_ln(rr);
    // if an old one found, directly put it at the front
    if (old) {
        LOG_INFO("an old RR found, placing it at the front\n");
        old->last->next = old->next;
        if (old->next) {
            old->next->last = old->last;
        }
        old->last = cache.dummy;
        old->next = cache.dummy->next;
        cache.dummy->next = old;
        if (old->next) {
            old->next->last = old;
        }
    } else {
        LOG_INFO("an old RR not found, creating a new one in the cache\n");
        cache.size++;
        // or else, copy the resource record an put it in front of the queue
        struct rr_cache_linked_node* new_node =
            create_cache_node(cache.dummy->next, cache.dummy, rr);
        cache.dummy->next = new_node;
        if (new_node->next) {
            new_node->next->last = new_node;
        }
    }
    pthread_mutex_unlock(&cache.lock);
    return 1;
}
static void free_cache_linked_node(struct rr_cache_linked_node* node) {
    if (!node) {
        LOG_ERR("node ptr is NULL\n");
    }

    free_heap_resource_record(node->element);
    free(node->element);
    node->element = NULL;
}
void refresh_cache() {
    while (1) {
        pthread_mutex_lock(&cache.lock);
        struct timeval now;
        gettimeofday(&now, NULL);

        struct rr_cache_linked_node *node = cache.dummy->next,
                                    *node_to_delete = NULL;
        uint32_t cnt = 0;
        while (node) {
            struct rr_cache_linked_node* next = node->next;

            cnt++;
            if (node->expire_time.tv_sec <= now.tv_sec) {
                cnt--;
                // the node has expired, remove it
                node->last->next = node->next;
                if (node->next) {
                    node->next->last = node->last;
                }
                free_heap_resource_record(node->element);
                free(node->element);
                free(node);
                cache.size--;
                LOG_INFO("removing dead cache node\n");
            } else {
                // or else, update rr's ttl
                node->element->ttl = node->expire_time.tv_sec - now.tv_sec;
            }

            node = next;
            if (cnt == CACHE_SIZE_MAX) {
                // too many cache record, remove all the nodes after _node_
                while (node) {
                    next = node->next;
                    node->last->next = NULL;
                    free_heap_resource_record(node->element);
                    free(node->element);
                    free(node);
                    cache.size--;
                    node = next;
                    LOG_INFO("removing node due to size restriction\n");
                }
                break;
            }
        }
        pthread_mutex_unlock(&cache.lock);
        sleep(1);
    }
}
void init_cache() {
    LOG_INFO("initializing cache...\n");
    cache.dummy = create_cache_node(NULL, NULL, NULL);
    cache.size = 0;
    // enabling timer for updating ttl
    pthread_create(&timer_thread, NULL, refresh_cache, NULL);
    pthread_mutex_init(&cache.lock, NULL);
}