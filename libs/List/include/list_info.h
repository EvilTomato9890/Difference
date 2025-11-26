#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED

#include "error_handler.h"
#include "debug_meta.h"
#include "tree_info.h"

#include <stdio.h>
#include <stdlib.h>

typedef tree_t* list_type;


static const size_t    MIN_LIST_SIZE    = 10;
static list_type const POISON_VAL       = (list_type)1111;
static const int       POISON_IDX       = -100;
static const float     REDUCTION_FACTOR = 4.0f;
static const float     GROWTH_FACTOR    = 2.0f;
static list_type const CANARY_NUM       = (list_type)0xEBA1DEDA;



struct node_t {
    ssize_t next;
    ssize_t prev;
    list_type val;
};

struct list_t {
    node_t* arr;
    size_t  capacity;
    size_t  size;

    ssize_t head;
    ssize_t tail;
    ssize_t free_head;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

static inline bool list_node_is_free(const node_t* node) {
    return node->prev == POISON_IDX || node->val == POISON_VAL;
}


#endif /* LIST_H_INCLUDED */