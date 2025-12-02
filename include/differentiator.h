#ifndef DIFFERENTIATOR_H_INCLUDED
#define DIFFERENTIATOR_H_INCLUDED

#include "tree_info.h"

const int MAX_NEUTRAL_ARGS = 2;

struct args_arr_t {
    size_t* arr;
    size_t  size;
};

tree_node_t* get_diff(tree_node_t* node, args_arr_t args_arr ON_TEX_CREATION_DEBUG(, tree_t* tree));

var_val_type calculate_tree(tree_t* tree, bool is_vars_given);

error_code tree_optimize(tree_t* tree);

tree_node_t* optimize_subtree_recursive(tree_node_t* node, error_code* error_ptr);

var_val_type calculate_nodes_recursive(tree_t* tree, tree_node_t* curr_node, error_code* error);
#endif