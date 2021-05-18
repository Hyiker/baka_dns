#ifndef TREES_H
#define TREES_H
#include <stdint.h>

#include "storage/message.h"
#define HASH_BUCKET_SIZE 1024
#define HASH_SWITCH_THRESHOLD 32
#define RR_ARR_LEN 3
#define SUBDOMAIN_MAX 50
#define RR_ARR_A 0
#define RR_ARR_CNAME 1
#define RR_ARR_AAAA 2
#define RR_CNT_MAX 4
struct tree_node;
struct node_element {
    // len + uint8...
    char domain[SUBDOMAIN_MAX];
    struct resource_record* data[RR_ARR_LEN][RR_CNT_MAX];
    uint32_t data_cnt[RR_ARR_LEN];
    // ptr to the higher level domain
    struct tree_node* child;
};
// a wrapper for node_element
// used to solve hash confliction
struct linked_node {
    struct node_element element;
    struct linked_node* next;
};
// tree node is a hash bucket of node elements
struct tree_node {
    enum { HASH_BUCKET, LINKED_LIST } type;
    uint32_t size;
    union {
        // a bucket with size HASH_BUCKET_SIZE
        struct linked_node** bucket;
        struct linked_node* linked_list;
    } container;
};

struct bucket_tree {
    struct tree_node* root;
    // hash function for a subdomain
    uint32_t (*hash_fun)(const uint8_t*);
};

uint32_t rr_hash(const uint8_t*);
struct bucket_tree* tree_init(uint32_t (*hash_fun)(const uint8_t*));

// insert a rr into tree with reversed domain
int tree_insert(struct bucket_tree*, const uint8_t*, struct resource_record*);

// search a tree by domain and its length & type
// store resource records in _dest_
ssize_t tree_search(struct bucket_tree*, struct resource_record*, uint8_t*,
                    uint32_t, uint16_t);

#endif