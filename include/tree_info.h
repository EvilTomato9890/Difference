#ifndef TREE_INFO_H_INCLUDED
#define TREE_INFO_H_INCLUDED

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "debug_meta.h"

struct tree_node_t {
    char*        value;
    unsigned long hash;
    bool         can_free;
    tree_node_t* parent;
    tree_node_t* left;
    tree_node_t* right;
};

struct tree_t {
    tree_node_t* root;
    tree_node_t* head;
    size_t       size;
    char*        file_buffer;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

#endif

