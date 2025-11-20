#ifndef TREE_OPERATIONS_H_INCLUDED
#define TREE_OPERATIONS_H_INCLUDED

#include "tree_info.h"
#include "error_handler.h"
#include "asserts.h"

error_code tree_init(tree_t* tree ON_DEBUG(, ver_info_t ver_info));
tree_node_t* init_node(node_type_t node_type, value_t value, tree_node_t* left, tree_node_t* right);

error_code tree_destroy(tree_t* tree);

bool tree_is_empty(const tree_t* tree);

tree_node_t* tree_init_root(tree_t* tree, node_type_t node_type, value_t value);
tree_node_t* tree_insert_left(tree_t* tree, node_type_t node_type, value_t value, tree_node_t* parent);
tree_node_t* tree_insert_right(tree_t* tree, node_type_t node_type, value_t value, tree_node_t* parent);

error_code tree_replace_value(tree_node_t* node, node_type_t node_type, value_t value);

error_code destroy_node_recursive(tree_node_t* node, size_t* removed_out);

struct tree_path_stack_t {
    tree_node_t** nodes;
    size_t size;
    size_t capacity;
};

error_code tree_read_from_file(tree_t* tree, const char* filename);
error_code tree_write_to_file(const tree_t* tree, const char* filename);

const char* get_func_name_by_type(func_type_t func_type_value);
value_t make_union(node_type_t type, ...);
inline tree_node_t* clone_node(const tree_node_t* node) {
    HARD_ASSERT(node != nullptr, "node is nullptr");
    return init_node(node->type, node->value, node->left, node->right);
}
#endif
