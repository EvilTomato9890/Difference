#ifndef TREE_INFO_H_INCLUDED
#define TREE_INFO_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>

#include "debug_meta.h"
#include "node_info.h"
#include "../libs/StackDead-main/stack.h"
#include "my_string.h"

const int MAX_FOREST_CAP = 10;

struct tree_t {
    tree_node_t*   root;
    size_t         size;
    stack_t*       var_stack;
    string_t       buff;
    size_t         list_idx;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* const * dump_file;
        FILE* const * tex_file;
    )
};

#endif
