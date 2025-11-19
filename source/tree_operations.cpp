#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asserts.h"
#include "logger.h"
#include "tree_operations.h"
#include "tree_verification.h"


static tree_node_t* allocate_node(const char* value, tree_node_t* parent, bool can_free) {
    tree_node_t* node = (tree_node_t*)calloc(1, sizeof(tree_node_t));
    if (node == nullptr) {
        LOGGER_ERROR("allocate_node: calloc failed");
        return nullptr;
    }
    if(value != nullptr) {
        if (can_free) {
            char* owned_value = strdup(value);
            if (owned_value == nullptr) {
                free(node);
                return nullptr;
            }
            node->value = owned_value;
        } else {
            node->value = (char*)value;
        }
        node->hash = tree_hash_string(value);
    } else {
        node->value = nullptr;
        node->hash = 0;
    }
    node->can_free = can_free;
    node->parent = parent;
    node->left   = nullptr;
    node->right  = nullptr;
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

    if (node->can_free && node->value != nullptr) {
        free(node->value);
    }
    free(node);
    removed_local = 1 + left_removed + right_removed;
    if (removed_out != nullptr) *removed_out = removed_local;
    return error;
}

static void detach_from_parent(tree_t* tree, tree_node_t* node) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");

    LOGGER_DEBUG("detach: started");
    if (node == nullptr) return;
    tree_node_t* parent = node->parent;
    if (parent == nullptr) {
        tree->root = nullptr;
        return;
    }
    if (parent->left == node) {
        parent->left = nullptr;
    } else if (parent->right == node) {
        parent->right = nullptr;
    } else {
        LOGGER_WARNING("detach_from_parent: node %p not attached to parent %p",
                       (void*)node, (void*)parent);
    }
}

error_code tree_init(tree_t* tree ON_DEBUG(, ver_info_t ver_info)) {
    HARD_ASSERT(tree != nullptr,      "tree pointer is nullptr");

    LOGGER_DEBUG("tree_init: started");
    tree->root = nullptr;
    tree->size = 0;
    tree->file_buffer = nullptr;
    tree->head = allocate_node(nullptr, nullptr, false);
    if(!tree->head) {
        LOGGER_ERROR("Tree_init: failed calloc head");
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
    
    if (tree->head != nullptr) {
        free(tree->head);
        tree->head = nullptr;
    }
    
    if (tree->file_buffer != nullptr) {
        free(tree->file_buffer);
        tree->file_buffer = nullptr;
    }
    
    return error;
}

bool tree_is_empty(const tree_t* tree) {
    HARD_ASSERT(tree != nullptr,      "tree pointer is nullptr");
    return tree->root == nullptr;
}

tree_node_t* tree_init_root(tree_t* tree, const char* value) {
    HARD_ASSERT(tree   != nullptr,    "tree pointer is nullptr");
    HARD_ASSERT(value  != nullptr,    "value is nullptr");
    HARD_ASSERT(value[0] != '\0',     "value is empty");

    LOGGER_DEBUG("tree_init_roo: started");
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "Before set_root(\"%s\")", value);
        if (verify_error != ERROR_NO) {
            LOGGER_ERROR("tree_init_root: verify before failed");
            return nullptr;
        }
    })
    if (tree->root != nullptr) {
        LOGGER_ERROR("tree_init_root: root already exists");
        return nullptr;
    }
    tree_node_t* node = allocate_node(value, tree->head, true);
    if (node == nullptr) return nullptr;
    tree->root = node;
    tree->size = 1;
    tree->head->right = node;
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "After set_root");
        if (verify_error != ERROR_NO) {
            LOGGER_ERROR("tree_init_root: verify after failed");
            return nullptr;
        }
    })
    return node;
}

tree_node_t* tree_insert_left(tree_t* tree, tree_node_t* parent, const char* value) {
    HARD_ASSERT(tree   != nullptr,    "tree pointer is nullptr");
    HARD_ASSERT(parent != nullptr,    "parent pointer is nullptr");
    HARD_ASSERT(value  != nullptr,    "value pointer is nullptr");
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "Before insert_left(\"%s\")", value ? value : "(null)");
        if (verify_error != ERROR_NO) return nullptr;
    })
    if (parent->left != nullptr) {
        LOGGER_ERROR("tree_insert_left: left child already exists");
        return nullptr;
    }
    tree_node_t* node = allocate_node(value, parent, true);
    if (node == nullptr) return nullptr;
    parent->left = node;
    tree->size += 1;
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "After insert_left");
        (void)verify_error;
    })
    return node;
}

tree_node_t* tree_insert_right(tree_t* tree, tree_node_t* parent, const char* value) {
    HARD_ASSERT(tree   != nullptr,    "tree pointer is nullptr");
    HARD_ASSERT(parent != nullptr,    "parent pointer is nullptr");
    HARD_ASSERT(value  != nullptr,    "value pointer is nullptr");
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "Before insert_right(\"%s\")", value ? value : "(null)");
        if (verify_error != ERROR_NO) return nullptr;
    })
    if (parent->right != nullptr) {
        LOGGER_ERROR("tree_insert_right: right child already exists");
        return nullptr;
    }
    tree_node_t* node = allocate_node(value, parent, true);
    if (node == nullptr) return nullptr;
    parent->right = node;
    tree->size += 1;
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "After insert_right");
        (void)verify_error;
    })
    return node;
}



error_code tree_replace_value(tree_node_t* node, const char* value) {
    HARD_ASSERT(node     != nullptr,  "node pointer is nullptr");
    HARD_ASSERT(value    != nullptr,  "value is nullptr");
    HARD_ASSERT(value[0] != '\0',     "value is empty");
    
    LOGGER_DEBUG("tree_replace_value: started");
    
    if (node->can_free && node->value != nullptr) {
        free(node->value);
    }
    
    char* newv = strdup(value);
    if (newv == nullptr) {
        LOGGER_ERROR("tree_replace_value: strdup failed");
        return ERROR_MEM_ALLOC;
    }
    node->value = newv;
    node->can_free = true;
    node->hash = tree_hash_string(value);
    return ERROR_NO;
}

error_code tree_remove_subtree(tree_t* tree, tree_node_t* subtree_root) {
    HARD_ASSERT(tree         != nullptr, "tree pointer is nullptr");
    HARD_ASSERT(subtree_root != nullptr, "subtree_root is nullptr");
    
    LOGGER_DEBUG("tree_remove_subtree: started");
    
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "Before remove_subtree(%p)", (void*)subtree_root);
        if (verify_error != ERROR_NO) return verify_error;
    })
    detach_from_parent(tree, subtree_root);
    size_t removed = 0;
    error_code error = ERROR_NO;
    error |= destroy_node_recursive(subtree_root, &removed);
    if (tree->size >= removed) tree->size -= removed;
    else tree->size = 0;
    ON_DEBUG({
        error_code verify_error = ERROR_NO;
        verify_error = tree_verify(tree, VER_INIT, TREE_DUMP_IMG, "After remove_subtree");
        (void)verify_error;
    })
    return error;
}


