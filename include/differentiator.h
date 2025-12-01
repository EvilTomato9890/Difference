#ifndef DIFFERENTIATOR_H_INCLUDED
#define DIFFERENTIATOR_H_INCLUDED

#include "tree_info.h"

const int MAX_NEUTRAL_ARGS = 2;

struct args_arr_t {
    size_t* arr;
    size_t  size;
};

tree_node_t* get_diff(tree_node_t* node, args_arr_t args_arr);

var_val_type calculate_tree(tree_t* tree);

error_code tree_optimize(tree_t* tree);

#endif