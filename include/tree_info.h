#ifndef TREE_INFO_H_INCLUDED
#define TREE_INFO_H_INCLUDED

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "debug_meta.h"
#include "node_info.h"

struct tree_node_t {
    node_type_t  type;
    value_t      value;
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

