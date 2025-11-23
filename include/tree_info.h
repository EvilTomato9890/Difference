#ifndef TREE_INFO_H_INCLUDED
#define TREE_INFO_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>

#include "debug_meta.h"
#include "node_info.h"
#include "../StackDead-main/stack.h"
#include "my_string.h"


struct tree_t {
    tree_node_t* root;
    tree_node_t* head;
    size_t       size;
    stack_t*     var_stack;
    char*  buff;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

#endif

