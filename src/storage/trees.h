#ifndef TREES_H
#define TREES_H
#include <stdint.h>

#include "server/message.h"
#define HASH_BUCKET_SIZE 1145
#define SUBDOMAIN_MAX 50
struct tree_node;
struct node_element {
    // len + uint8...
    char domain[SUBDOMAIN_MAX];
    struct resource_record* data;
    // ptr to the higher level domain
    struct tree_node* child;
};
// a wrapper for node_element
// used to solve hash confliction
struct linked_node {
    struct node_element* element;
    struct node_element* next;
};
// tree node is a hash bucket of node elements
struct tree_node {
    struct linked_node* bucket[HASH_BUCKET_SIZE];
};

struct btree {
    struct tree_node* root;
    // hash function for a subdomain
    uint32_t (*hash_fun)(uint8_t*);
};

uint32_t rr_hash(uint8_t*);
struct btree* tree_init(uint32_t (*hash_fun)(uint8_t*));

int tree_insert(struct btree*, struct resource_record*);

// search a tree by domain and its length
struct resource_record* tree_search(struct btree*, uint8_t*, uint32_t);

#endif