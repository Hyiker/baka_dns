#include "storage/trees.h"

#include <stdlib.h>

#include "string.h"
#include "utils/logging.h"
#define HASH_SEED 131
uint32_t rr_hash(uint8_t* ptr) {
    // only hash by name
    // BKDR hash function
    uint32_t hsh = 0;
    size_t n = *ptr;
    ptr++;
    for (size_t i = 0; i < n; i++) {
        hsh = hsh * HASH_SEED + (*ptr++);
    }
    return hsh & 0x7FFFFFFF;
}
struct btree* tree_init(uint32_t (*hash_fun)(uint8_t*)) {
    struct btree* bt = malloc(sizeof(struct btree));
    bt->hash_fun = hash_fun;
    bt->root = malloc(sizeof(struct tree_node));
    memset(bt->root, 0, sizeof(bt->root));
    return bt;
}
static struct linked_node* hash_ll_find(
    struct linked_node* bucket[HASH_BUCKET_SIZE], uint8_t* target) {
    uint32_t index = rr_hash(target) % HASH_BUCKET_SIZE;
    struct linked_node* ln = bucket[index];
    while (ln) {
        if (strncmp(target, ln->element.domain, (*target) + 1) == 0) {
            break;
        }
        ln = ln->next;
    }
    return ln;
}
int tree_insert(struct btree* tree, struct resource_record* rr) {
    // current domain name ptr
    uint8_t* dptr = rr->name;
    // pointer to next domain
    uint32_t next = *(dptr + (*dptr) + 1);
    // current tree node focusing
    struct tree_node* tn = tree->root;
    while (*dptr) {
        struct linked_node* ln = hash_ll_find(tn->bucket, dptr);
        if (ln) {
            // node is found
            if (!next) {
                // final node found
                if (ln->element.data) {
                    LOG_ERR("duplicated resource record");
                    return -1;
                } else {
                    ln->element.data = rr;
                    strncpy(ln->element.domain, dptr, (*dptr) + 1);
                    return 1;
                }
            } else {
                // continue to ln's child
                dptr = dptr + (*dptr) + 1;
                next = *(dptr + (*dptr) + 1);
                tn = ln->element.child;
            }
        } else {
            // node is not found, create it
            uint32_t index = rr_hash(dptr) % HASH_BUCKET_SIZE;
            struct linked_node* new_node = malloc(sizeof(struct linked_node));
            memset(new_node, 0, sizeof(struct linked_node));
            new_node->next = tn->bucket[index];
            tn->bucket[index] = new_node;
            memcpy(new_node->element.domain, dptr, (*dptr) + 1);
            if (!next) {
                // directly create a new linked node in the bucket
                new_node->element.data = rr;
                return 1;
            } else {
                // create child for it otherwise
                new_node->next = malloc(sizeof(struct linked_node));
                memset(new_node->next, 0, sizeof(struct linked_node));
                new_node->element.child = malloc(sizeof(struct tree_node));
            }
        }
    }
}

// search a tree by domain and its length
struct resource_record* tree_search(struct btree* tree, uint8_t* domain,
                                    uint32_t dlen) {
    // current domain name ptr
    uint8_t* dptr = domain;
    // pointer to next domain
    uint32_t next = *(dptr + (*dptr) + 1);
    // current tree node focusing
    struct tree_node* tn = tree->root;
    while (*dptr) {
        struct linked_node* ln = hash_ll_find(tn->bucket, dptr);
        if (!ln) {
            return NULL;
        } else {
            if (!next) {
                return ln->element.data;
            } else {
                dptr = dptr + (*dptr) + 1;
                next = *(dptr + (*dptr) + 1);
                tn = ln->element.child;
            }
        }
    }
    return NULL;
}