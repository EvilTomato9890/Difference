#ifndef TREE_OPERATIONS_H_INCLUDED
#define TREE_OPERATIONS_H_INCLUDED

#include "tree_info.h"
#include "error_handler.h"

unsigned long tree_hash_string(const char* str);

error_code tree_init(tree_t* tree ON_DEBUG(, ver_info_t ver_info));
error_code tree_destroy(tree_t* tree);

bool tree_is_empty(const tree_t* tree);

tree_node_t* tree_init_root(tree_t* tree, const char* value);
tree_node_t* tree_insert_left(tree_t* tree, tree_node_t* parent, const char* value);
tree_node_t* tree_insert_right(tree_t* tree, tree_node_t* parent, const char* value);

error_code tree_replace_value (tree_node_t* node, const char* value);
error_code tree_remove_subtree(tree_t* tree, tree_node_t* subtree_root);

error_code destroy_node_recursive(tree_node_t* node, size_t* removed_out);

tree_node_t* tree_find_preorder(tree_node_t* node, const char* value);

tree_node_t* tree_find_by_hash(const tree_t* tree, const char* value);

struct tree_path_stack_t {
    tree_node_t** nodes;
    size_t size;
    size_t capacity;
};

error_code tree_path_to_root(tree_node_t* node, tree_path_stack_t* path_out);

error_code tree_print_node_path(tree_node_t* node);

error_code tree_read_from_file(tree_t* tree, const char* filename);
error_code tree_write_to_file(const tree_t* tree, const char* filename);

#endif

