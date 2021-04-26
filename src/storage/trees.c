#include "storage/trees.h"

#include <stdlib.h>

#include "string.h"
#include "utils/logging.h"
#define HASH_SEED 131
static struct tree_node* create_tree_node() {
    struct tree_node* tn = malloc(sizeof(struct tree_node));
    memset(tn, 0, sizeof(struct tree_node));
    return tn;
}
static struct linked_node* create_linked_node() {
    struct linked_node* ln = malloc(sizeof(struct linked_node));
    memset(ln, 0, sizeof(struct linked_node));
    return ln;
}
uint32_t rr_hash(const uint8_t* ptr) {
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
struct bucket_tree* tree_init(uint32_t (*hash_fun)(const uint8_t*)) {
    struct bucket_tree* bt = malloc(sizeof(struct bucket_tree));
    bt->hash_fun = hash_fun;
    bt->root = create_tree_node();
    return bt;
}
static struct linked_node* hash_ll_find(
    struct linked_node* bucket[HASH_BUCKET_SIZE], const uint8_t* target) {
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
int tree_insert(struct bucket_tree* tree, const uint8_t* rev_domain,
                struct resource_record* rr) {
    // pointer to next domain
    uint32_t next = *(rev_domain + (*rev_domain) + 1);
    // current tree node focusing
    struct tree_node* tn = tree->root;
    while (*rev_domain) {
        struct linked_node* ln = hash_ll_find(tn->bucket, rev_domain);
        if (ln) {
            // node is found
            if (!next) {
                // final node found
                if (ln->element.data) {
                    LOG_ERR("duplicated resource record\n");
                    return -1;
                } else {
                    ln->element.data = rr;
                    strncpy(ln->element.domain, rev_domain, (*rev_domain) + 1);
                    return 1;
                }
            } else {
                // continue to ln's child
                rev_domain = rev_domain + (*rev_domain) + 1;
                next = *(rev_domain + (*rev_domain) + 1);
                if (!ln->element.child) {
                    ln->element.child = create_tree_node();
                }
                tn = ln->element.child;
            }
        } else {
            // node is not found, create it
            uint32_t index = tree->hash_fun(rev_domain) % HASH_BUCKET_SIZE;
            struct linked_node* new_node = create_linked_node();
            new_node->next = tn->bucket[index];
            tn->bucket[index] = new_node;
            memcpy(new_node->element.domain, rev_domain, (*rev_domain) + 1);
            if (!next) {
                // directly create a new linked node in the bucket
                new_node->element.data = rr;
                return 1;
            } else {
                // create child for it otherwise
                new_node->next = create_linked_node();
                new_node->element.child = create_tree_node();
            }
        }
    }
    return 1;
}

// search a tree by domain and its length
struct resource_record* tree_search(struct bucket_tree* tree, uint8_t* domain,
                                    uint32_t dlen) {
    // pointer to next domain
    uint32_t next = *(domain + (*domain) + 1);
    // current tree node focusing
    struct tree_node* tn = tree->root;
    while (*domain) {
        struct linked_node* ln = hash_ll_find(tn->bucket, domain);
        if (!ln) {
            return NULL;
        } else {
            if (!next) {
                return ln->element.data;
            } else {
                domain = domain + (*domain) + 1;
                next = *(domain + (*domain) + 1);
                tn = ln->element.child;
            }
        }
    }
    return NULL;
}