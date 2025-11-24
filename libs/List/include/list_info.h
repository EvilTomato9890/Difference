#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED

#include "error_handler.h"
#include "debug_meta.h"
#include <stdio.h>
#include <stdlib.h>

static const size_t MIN_LIST_SIZE    = 10;
static const int    POISON           = -100;
static const float  REDUCTION_FACTOR = 4.0f;
static const float  GROWTH_FACTOR    = 2.0f;
static const float  CANARY_NUM       = 0xDEADBEEF;


struct node_t {
    ssize_t next;
    ssize_t prev;
    double val;
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
 //На будущее новые фугкции дял поулчения и вставки элементов
 //Полная задача структуры node
static inline bool list_node_is_free(const node_t* node) {
    return node->prev == POISON || node->val == POISON;
}


#endif /* LIST_H_INCLUDED */