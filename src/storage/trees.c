#include "storage/trees.h"

#include "string.h"
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
}
static struct linked_node* hash_ll_find(
    struct linked_node* bucket[HASH_BUCKET_SIZE], uint8_t* target) {
    uint32_t index = rr_hash(target) % HASH_BUCKET_SIZE;
    struct linked_node* ln = bucket[index];
    while (ln) {
        if (strncmp(target, ln->element->domain, *target) == 0) {
            break;
        }
        ln = ln->next;
    }
    return ln;
}
int tree_insert(struct btree* tree, struct resource_record* rr) {
    uint8_t* dptr = rr->name;
    uint32_t next = *(dptr + (*dptr) + 1);
    struct tree_node* tn = tree->root;
    while (*dptr) {
        struct linked_node* ln = hash_ll_find(tn->bucket, dptr);
        if (ln) {
            if (!next) {
                // no more left subdomain
                // insert after ln
            }else{
                // continue finding next level
            }
        }
    }
}

// search a tree by domain and its length
struct resource_record* tree_search(struct btree*, uint8_t*, uint32_t);