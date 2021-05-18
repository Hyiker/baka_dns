#include "storage/trees.h"

#include <stdlib.h>
#include <sys/types.h>

#include "string.h"
#include "utils/logging.h"
#define HASH_SEED 131
static struct tree_node* create_tree_node() {
    struct tree_node* tn = malloc(sizeof(struct tree_node));
    memset(tn, 0, sizeof(struct tree_node));
    tn->type = LINKED_LIST;
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
    for (size_t i = 0; i < n; i++, ptr++) {
        hsh = hsh * HASH_SEED + *ptr;
    }
    return hsh;
}
struct bucket_tree* tree_init(uint32_t (*hash_fun)(const uint8_t*)) {
    struct bucket_tree* bt = malloc(sizeof(struct bucket_tree));
    bt->hash_fun = hash_fun;
    bt->root = create_tree_node();
    return bt;
}
static struct linked_node* _hash_ll_find(
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
static struct linked_node* _ll_find(struct linked_node* node,
                                    const uint8_t* target) {
    while (node) {
        if (strncmp(target, node->element.domain, (*target) + 1) == 0) {
            break;
        }

        node = node->next;
    }
    return node;
}
static struct linked_node* tree_node_find(struct tree_node* tn,
                                          const uint8_t* target) {
    switch (tn->type) {
        case LINKED_LIST:
            return _ll_find(tn->container.linked_list, target);
            break;
        case HASH_BUCKET:
            return _hash_ll_find(tn->container.bucket, target);
            break;
        default:
            LOG_ERR("unknown container type\n");
            return NULL;
            break;
    };
}
static struct linked_node* tree_node_insert(
    struct tree_node* tn, const uint8_t* rev_domain,
    uint32_t (*hash_fun)(const uint8_t*)) {
    struct linked_node* new_node = create_linked_node();
    memcpy(new_node->element.domain, rev_domain, (*rev_domain) + 1);
    switch (tn->type) {
        case HASH_BUCKET:;
            uint32_t index = hash_fun(rev_domain) % HASH_BUCKET_SIZE;
            new_node->next = tn->container.bucket[index];
            tn->container.bucket[index] = new_node;
            break;
        case LINKED_LIST:
            new_node->next = tn->container.linked_list;
            tn->container.linked_list = new_node;
            break;
        default:
            LOG_ERR("unknown container type\n");
            free(new_node);
            return NULL;
            break;
    }
    tn->size++;
    if (tn->size > HASH_SWITCH_THRESHOLD && tn->type == LINKED_LIST) {
        LOG_INFO("switching to hash bucket\n");
        // linked list too long, switch to hash list
        tn->type = HASH_BUCKET;
        struct linked_node* node = new_node;
        tn->container.bucket =
            malloc(sizeof(struct linked_node*) * HASH_BUCKET_SIZE);
        memset(tn->container.bucket, 0,
               sizeof(struct linked_node*) * HASH_BUCKET_SIZE);
        while (node) {
            uint32_t index = hash_fun(node->element.domain) % HASH_BUCKET_SIZE;
            struct linked_node* tmp = node->next;
            node->next = tn->container.bucket[index];
            tn->container.bucket[index] = node;
            node = tmp;
        }
    }

    return new_node;
}
int tree_insert(struct bucket_tree* tree, const uint8_t* rev_domain,
                struct resource_record* rr) {
    // pointer to next domain
    uint32_t next = *(rev_domain + (*rev_domain) + 1);
    // current tree node focusing
    struct tree_node* tn = tree->root;
    int element_index = -1;
    switch (rr->type) {
        case RRTYPE_A:
            element_index = RR_ARR_A;
            break;
        case RRTYPE_CNAME:
            element_index = RR_ARR_CNAME;
            break;
        case RRTYPE_AAAA:
            element_index = RR_ARR_AAAA;
            break;
        default:
            LOG_ERR("unsupported resource record insertion\n");
            return -1;
    }
    while (*rev_domain) {
        struct linked_node* ln = tree_node_find(tn, rev_domain);
        if (ln) {
            // node is found
            if (!next) {
                // final node found
                if (ln->element.data_cnt[element_index] >= RR_CNT_MAX) {
                    LOG_ERR(
                        "resource record of this kind count has hit the "
                        "limit\n");
                    return -1;
                }
                for (size_t i = 0; i < ln->element.data_cnt[element_index];
                     i++) {
                    if (memcmp(rr->rdata,
                               ln->element.data[element_index][i]->rdata,
                               rr->rdlength) == 0) {
                        LOG_ERR("duplicated resource record\n");
                        return -1;
                    }
                }

                ln->element.data[element_index]
                                [ln->element.data_cnt[element_index]++] = rr;
                memcpy(ln->element.domain, rev_domain, (*rev_domain) + 1);
                return 1;

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
            struct linked_node* new_node =
                tree_node_insert(tn, rev_domain, tree->hash_fun);
            if (!next) {
                // directly create a new linked node in the bucket
                new_node->element
                    .data[element_index]
                         [new_node->element.data_cnt[element_index]++] = rr;
                return 1;
            } else {
                // create child for it otherwise
                // new_node->next = create_linked_node();
                new_node->element.child = create_tree_node();
            }
        }
    }
    return 1;
}

// search a tree by domain and its length
ssize_t tree_search(struct bucket_tree* tree, struct resource_record* dest,
                    uint8_t* domain, uint32_t dlen, uint16_t rr_type) {
    int element_index = -1;
    switch (rr_type) {
        case RRTYPE_A:
            element_index = RR_ARR_A;
            break;
        case RRTYPE_AAAA:
            element_index = RR_ARR_AAAA;
            break;
        case RRTYPE_CNAME:
            element_index = RR_ARR_CNAME;
            break;
        default:
            return 0;
    }
    // pointer to next domain
    uint32_t next = *(domain + (*domain) + 1);
    // current tree node focusing
    struct tree_node* tn = tree->root;
    while (*domain && tn) {
        struct linked_node* ln = tree_node_find(tn, domain);
        if (!ln) {
            return 0;
        } else {
            if (!next) {
                for (size_t i = 0; i < ln->element.data_cnt[element_index];
                     i++) {
                    rr_copy(dest, ln->element.data[element_index][i]);
                    dest++;
                }

                return ln->element.data_cnt[element_index];
            } else {
                domain = domain + (*domain) + 1;
                next = *(domain + (*domain) + 1);
                tn = ln->element.child;
            }
        }
    }
    return 0;
}