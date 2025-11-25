#ifndef FOREST_INFO_H_INCLUDED
#define FOREST_INFO_H_INCLUDED

#include "../libs/List/include/list_info.h"
#include "../libs/StackDead-main/stack.h"
#include "debug_meta.h"

struct forest_t {
    list_t*  tree_list;
    string_t buff;
    stack_t* var_stack;
    ON_DEBUG(
        ver_info_t ver_info;
        FILE* dump_file;
    )
};

#endif
