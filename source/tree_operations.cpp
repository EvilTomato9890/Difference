#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "asserts.h"
#include "logger.h"
#include "tree_operations.h"
#include "tree_verification.h"
#include "tree_info.h"
#include "error_handler.h"
#include "../libs/StackDead-main/stack.h" //КАК
#include "forest_operations.h"
#include "forest_info.h"
#include "debug_meta.h"

//================================================================================

value_t make_union(node_type_t type, ...) {
    value_t val = {};

    va_list ap = {};
    va_start(ap, type);

    switch (type) {
        case CONSTANT: {
            const_val_type constant = va_arg(ap, const_val_type);
            val.constant = constant;
            break;
        }
        case VARIABLE: {
            int var = va_arg(ap, int);
            val.var_idx = var;
            break;
        }
        case FUNCTION: {
            int func = va_arg(ap, int);
            val.func = (func_type_t)func;
            break;
        }
        default:
            LOGGER_ERROR("make_union: unknown node_type_t %d", type);
            break;
    }
    va_end(ap);
    return val;
}

tree_node_t* init_node(node_type_t node_type, value_t value, tree_node_t* left, tree_node_t* right) {
    tree_node_t* node = (tree_node_t*)calloc(1, sizeof(tree_node_t));
    if (node == nullptr) {
        LOGGER_ERROR("allocate_node: calloc failed");
        return nullptr;
    }
    node->type   = node_type;
    node->value  = value;
    node->left   = left;
    node->right  = right;
    return node;
}

tree_node_t* init_node_with_dump(node_type_t node_type, value_t value, tree_node_t* left, tree_node_t* right, tree_t* tree) {
    HARD_ASSERT(tree  != nullptr, "tree is nullptr");

    tree_node_t* node = init_node(node_type, value, left, right);
    tree_t tree_clone = {};
    tree_clone = *tree;
    tree_change_root(&tree_clone, node);
    tree_dump(&tree_clone, VER_INIT, true, "Diff dumps");
    
    return node;
}

error_code destroy_node_recursive(tree_node_t* node, size_t* removed_out) {
    error_code error = ERROR_NO;
    size_t removed_local = 0;
    if (node == nullptr) {
        if (removed_out != nullptr) *removed_out = 0;
        return error;
    }
    size_t left_removed = 0;
    error |= destroy_node_recursive(node->left, &left_removed);

    size_t right_removed = 0;
    error |= destroy_node_recursive(node->right, &right_removed);

    free(node);
    removed_local = 1 + left_removed + right_removed;
    if (removed_out != nullptr) *removed_out = removed_local;
    return error;
}

error_code tree_init(tree_t* tree, stack_t* stack ON_DEBUG(, ver_info_t ver_info)) {
    HARD_ASSERT(tree != nullptr, "tree pointer is nullptr");

    LOGGER_DEBUG("tree_init: started");

    error_code error = 0;
    tree->root = nullptr;
    tree->size = 0;
    tree->buff = {nullptr, 0};

    error = stack_init(stack, 10 ON_DEBUG(, VER_INIT));
    tree->var_stack = stack;
    if(error != ERROR_NO) {
        LOGGER_ERROR("Tree_init: stack_init failed");
        return ERROR_MEM_ALLOC;
    }
    ON_DEBUG({
        tree->ver_info  = ver_info;
        tree->dump_file = nullptr;
    })
    return ERROR_NO;
}

error_code tree_destroy(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree pointer is nullptr");

    LOGGER_DEBUG("tree_dest: started");

    size_t removed = 0;
    error_code error = ERROR_NO;
    error |= destroy_node_recursive(tree->root, &removed);
    tree->root = nullptr;
    tree->size = 0;

    return error;
}

bool tree_is_empty(const tree_t* tree) {
    HARD_ASSERT(tree != nullptr,      "tree pointer is nullptr");
    return tree->root == nullptr;
}

size_t count_nodes_recursive(const tree_node_t* node) {
    if (node == nullptr) return 0;
    return 1 + count_nodes_recursive(node->left) + count_nodes_recursive(node->right);
}
//================================================================================

tree_node_t* tree_init_root(tree_t* tree, node_type_t node_type, value_t value) {
    HARD_ASSERT(tree   != nullptr,    "tree pointer is nullptr");

    LOGGER_DEBUG("tree_init_roo: started");

    if (tree->root != nullptr) {
        LOGGER_ERROR("tree_init_root: root already exists");
        return nullptr;
    }
    tree_node_t* node = init_node(node_type, value, nullptr, nullptr);
    if (node == nullptr) return nullptr;

    tree_change_root(tree, node);

    return node;
}

tree_node_t* tree_change_root(tree_t* tree, tree_node_t* node) {
    HARD_ASSERT(tree != nullptr, "Tree is nullptr");
    if(!node) LOGGER_WARNING("New root is nullptr");

    tree->root = node;
    tree->size = count_nodes_recursive(node);

    return node;
}

tree_node_t* tree_insert_left(tree_t* tree, node_type_t node_type, value_t value, tree_node_t* parent) {
    HARD_ASSERT(tree   != nullptr,    "tree pointer is nullptr");
    HARD_ASSERT(parent != nullptr,    "parent pointer is nullptr");

    if (parent->left != nullptr) {
        LOGGER_ERROR("tree_insert_left: left child already exists");
        return nullptr;
    }
    tree_node_t* node = init_node(node_type, value, nullptr, nullptr);
    if (node == nullptr) return nullptr;
    parent->left = node;
    tree->size += 1;
    return node;
}

tree_node_t* tree_insert_right(tree_t* tree, node_type_t node_type, value_t value, tree_node_t* parent) {
    HARD_ASSERT(tree   != nullptr,    "tree pointer is nullptr");
    HARD_ASSERT(parent != nullptr,    "parent pointer is nullptr");

    if (parent->right != nullptr) {
        LOGGER_ERROR("tree_insert_right: right child already exists");
        return nullptr;
    }
    tree_node_t* node = init_node(node_type, value, nullptr, nullptr);
    if (node == nullptr) return nullptr;
    parent->right = node;
    tree->size += 1;

    return node;
}

error_code tree_replace_value(tree_node_t* node, node_type_t node_type, value_t value) {
    HARD_ASSERT(node     != nullptr,  "node pointer is nullptr");
    
    LOGGER_DEBUG("tree_replace_value: started");
    
    node->type  = node_type;
    node->value = value;
    return ERROR_NO;
}

ssize_t get_var_idx(const string_t var, const stack_t* var_stack) {
    HARD_ASSERT(var_stack != nullptr, "var_stack is nullptr");

    for(size_t i = 0; i < var_stack->size; i++) {
        if(my_ssstrcmp(var_stack->data[i].str, var) == 0) {
            return i;
        }
    }
    return -1;
}

size_t add_var(const string_t str, const var_val_type val, stack_t* var_stack, error_code* error) {
    HARD_ASSERT(var_stack != nullptr, "var_stack is nulltpr");

    if(error == nullptr) stack_push(var_stack, {str, val});
    else        *error = stack_push(var_stack, {str, val});
    return var_stack->size - 1;
}

size_t get_or_add_var_idx(const string_t str, const var_val_type val, stack_t* var_stack, error_code* error) {
    HARD_ASSERT(var_stack != nullptr, "var_stack is nullptr");

    ssize_t idx = get_var_idx(str, var_stack);
    if(idx == -1) {
        return add_var(str, val, var_stack, error);
    } 
    return (size_t)idx;
}