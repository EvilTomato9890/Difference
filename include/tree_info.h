#ifndef TREE_INFO_H_INCLUDED
#define TREE_INFO_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>

#include "debug_meta.h"
#include "node_info.h"
#include "../libs/StackDead-main/stack.h"
#include "my_string.h"
#include "../libs/List/include/list_info.h"

const int MAX_FOREST_CAP = 10;

struct tree_t {
    tree_node_t* root;
    tree_node_t* head;
    size_t       size;
    stack_t*     var_stack;
    const char*  buff;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

struct forest_t {
    list_t*  tree_list;
    char*    buff;
    stack_t* var_stack;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

#endif

