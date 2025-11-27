#ifndef DIFFERENTIATOR_H_INCLUDED
#define DIFFERENTIATOR_H_INCLUDED

#include "tree_info.h"

tree_node_t* get_diff(const tree_t* tree, tree_node_t* node);
var_val_type calculate_tree(tree_t* tree);

#endif