#ifndef TREE_OPERATIONS_H_INCLUDED
#define TREE_OPERATIONS_H_INCLUDED

#include "tree_info.h"
#include "error_handler.h"
#include "asserts.h"
#include "../libs/StackDead-main/stack.h"


error_code tree_init(tree_t* tree, stack_t* stack ON_DEBUG(, ver_info_t ver_info));

tree_node_t* init_node(node_type_t node_type, value_t value, tree_node_t* left, tree_node_t* right);
tree_node_t* init_node_with_dump(node_type_t node_type, value_t value, tree_node_t* left, tree_node_t* right, tree_t* tree);

error_code tree_destroy(tree_t* tree);

bool tree_is_empty(const tree_t* tree);
size_t count_nodes_recursive(const tree_node_t* node);

tree_node_t* tree_change_root(tree_t* tree, tree_node_t* node); 
tree_node_t* tree_init_root(tree_t* tree, node_type_t node_type, value_t value);
tree_node_t* tree_insert_left(tree_t* tree, node_type_t node_type, value_t value, tree_node_t* parent);
tree_node_t* tree_insert_right(tree_t* tree, node_type_t node_type, value_t value, tree_node_t* parent);

error_code tree_replace_value(tree_node_t* node, node_type_t node_type, value_t value);

error_code destroy_node_recursive(tree_node_t* node, size_t* removed_out);

value_t make_union(node_type_t type, ...);

ssize_t get_var_idx(const string_t var, const stack_t* var_stack);
size_t add_var(const string_t str, const var_val_type val, stack_t* var_stack, error_code* error);
size_t get_or_add_var_idx(const string_t str, const var_val_type val, stack_t* var_stack, error_code* error);

inline tree_node_t* clone_node(const tree_node_t* node) {
    HARD_ASSERT(node != nullptr, "node is nullptr");
    return init_node(node->type, node->value, node->left, node->right);
}

#endif
