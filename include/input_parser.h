#ifndef INPUT_PARSER_H_INCLUDED
#define INPUT_PARSER_H_INCLUDED

#include "tree_info.h"

#define CONSTANT_init(val) \
    init_node(CONSTANT, make_union_const(val), nullptr, nullptr);

tree_node_t* get_g(tree_t* tree, char** str);
#endif