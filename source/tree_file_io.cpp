#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "asserts.h"
#include "logger.h"
#include "tree_operations.h"
#include "tree_verification.h"
#include "error_handler.h"
#include "tree_info.h"

static char* skip_whitespace(char* curr) {
    HARD_ASSERT(curr != nullptr, "curr is nullptr");
    while (*curr != '\0' && isspace((unsigned char)*curr)) {
        curr++;
    }
    return curr;
}

static tree_node_t* allocate_node_from_buffer(node_value_t value, tree_node_t* parent) {
    HARD_ASSERT(parent != nullptr, "parent is nullptr");
    
    LOGGER_DEBUG("allocate_node_from_buffer: started");
    tree_node_t* node = (tree_node_t*)calloc(1, sizeof(tree_node_t));
    if (node == nullptr) {
        LOGGER_ERROR("allocate_node_from_buffer: calloc failed");
        return nullptr;
    }
    if (value != nullptr) {
        node->value = (char*)value;
    } else {
        node->value = nullptr;
    }
    node->parent = parent;
    node->left = nullptr;
    node->right = nullptr;
    return node;
}

static void cleanup_failed_node(tree_t* tree, tree_node_t* failed_node, tree_node_t* parent) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    
    if (failed_node == nullptr) {
        return;
    }
    
    size_t removed = 0;
    destroy_node_recursive(failed_node, &removed);
    
    if (parent == tree->head) {
        tree->root = nullptr;
        tree->head->right = nullptr;
    } else if (parent != nullptr && parent->left == failed_node) {
        parent->left = nullptr;
    } else if (parent != nullptr && parent->right == failed_node) {
        parent->right = nullptr;
    }
}

static void attach_node_to_parent(tree_t* tree, tree_node_t* new_node, tree_node_t* parent) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(new_node != nullptr, "new_node is nullptr");
    HARD_ASSERT(parent != nullptr, "parent is nullptr");
    
    if (parent == tree->head) {
        tree->root = new_node;
        tree->head->right = new_node;
    } else if (parent->left == nullptr) {
        parent->left = new_node;
    } else {
        parent->right = new_node;
    }
}

static void debug_print_buffer_remainder(tree_t* tree, const char* value_start, char* value_end, char* buffer_start, size_t buffer_size) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(value_start != nullptr, "value_start is nullptr");
    
    ON_DEBUG({
        if (tree->dump_file != nullptr) {
            if (value_end != nullptr && buffer_start != nullptr && value_end >= buffer_start && value_end < buffer_start + buffer_size) {
                char original_char = *value_end;
                *value_end = '"';
                
                size_t remainder_len = buffer_size - (value_end - buffer_start);
                if (remainder_len > 200) {
                    remainder_len = 200;
                }
                char* remainder_copy = (char*)calloc(remainder_len + 1, sizeof(char));
                if (remainder_copy != nullptr) {
                    memcpy(remainder_copy, value_end, remainder_len);
                    *value_end = original_char;

                    remainder_copy[remainder_len] = '\0';
                    tree_dump(tree, VER_INIT, true, "After reading node: %s \nBuffer remainder: %s", value_start, remainder_copy);
                    free(remainder_copy);
                } else {
                    *value_end = original_char;

                    tree_dump(tree, VER_INIT, true, "After reading node: %s", value_start);
                } 
            } else {
                tree_dump(tree, VER_INIT, true, "After reading node: %s", value_start);
            }
        }
    })
}



static tree_node_t* read_node(tree_t* tree, char** curr_ptr, tree_node_t* parent, bool* error_flag, char* buffer_start, size_t buffer_size) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(curr_ptr != nullptr, "curr_ptr is nullptr");
    HARD_ASSERT(error_flag != nullptr, "error_flag is nullptr");
    
    char* curr = skip_whitespace(*curr_ptr);
    
    if (*curr == '(') {
        curr++;
        curr = skip_whitespace(curr);
        
        if (*curr != '"') {
            LOGGER_ERROR("read_node: expected '\"' after '('");
            *error_flag = true;
            return nullptr;
        }
        
        curr++;
        char* value_start = curr;
        
        char* value_end = value_start;
        while (*value_end != '\0' && *value_end != '"') {
            value_end++;
        }
        
        if (*value_end != '"') {
            LOGGER_ERROR("read_node: missing closing '\"'");
            *error_flag = true;
            return nullptr;
        }
        
        *value_end = '\0';
        
        tree_node_t* new_node = allocate_node_from_buffer(value_start, parent);
        if (new_node == nullptr) {
            *error_flag = true;
            return nullptr;
        }
        
        attach_node_to_parent(tree, new_node, parent);
        
        debug_print_buffer_remainder(tree, value_start, value_end, buffer_start, buffer_size);
        
        curr = value_end + 1;
        curr = skip_whitespace(curr);
        
        new_node->left = read_node(tree, &curr, new_node, error_flag, buffer_start, buffer_size);
        if (*error_flag) {
            cleanup_failed_node(tree, new_node, parent);
            return nullptr;
        }
        
        curr = skip_whitespace(curr);
        
        new_node->right = read_node(tree, &curr, new_node, error_flag, buffer_start, buffer_size);
        if (*error_flag) {
            cleanup_failed_node(tree, new_node, parent);
            return nullptr;
        }
        
        curr = skip_whitespace(curr);
        
        if (*curr != ')') {
            LOGGER_ERROR("read_node: expected ')' after children");
            cleanup_failed_node(tree, new_node, parent);
            *error_flag = true;
            return nullptr;
        }
        
        curr++;
        *curr_ptr = curr;
        return new_node;
    }
    
    if (*curr == 'n') {
        if (strncmp(curr, "nil", 3) == 0) {
            curr += 3;
            *curr_ptr = curr;
            return nullptr;
        }
        LOGGER_ERROR("read_node: expected 'nil'");
        *error_flag = true;
        return nullptr;
    }
    
    LOGGER_ERROR("read_node: unexpected character '%c'", *curr);
    *error_flag = true;
    return nullptr;
}

static size_t count_nodes(tree_node_t* node) {
    if (node == nullptr) return 0;
    return 1 + count_nodes(node->left) + count_nodes(node->right);
}

static error_code ensure_tree_initialized(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    
    if (tree->head == nullptr) {
        error_code init_error = tree_init(tree ON_DEBUG(, VER_INIT));
        if (init_error != ERROR_NO) {
            LOGGER_ERROR("ensure_tree_initialized: tree_init failed");
            return init_error;
        }
    }
    return ERROR_NO;
}

static long get_file_size(FILE* file) {
    if(file == nullptr) {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return file_size;
}
static error_code read_file_to_buffer(const char* filename, char** buffer_out, size_t* buffer_size_out) {
    HARD_ASSERT(filename != nullptr, "filename is nullptr");
    HARD_ASSERT(buffer_out != nullptr, "buffer_out is nullptr");
    HARD_ASSERT(buffer_size_out != nullptr, "buffer_size_out is nullptr");
    
    FILE* file = fopen(filename, "r");
    if (file == nullptr) {
        LOGGER_ERROR("read_file_to_buffer: failed to open file '%s'", filename);
        return ERROR_OPEN_FILE;
    }
    
    long file_size = get_file_size(file);
    if (file_size < 0) {
        LOGGER_ERROR("read_file_to_buffer: failed to get file size");
        fclose(file);
        return ERROR_OPEN_FILE;
    }
    
    char* buffer = (char*)calloc(file_size + 1, sizeof(char));
    if (buffer == nullptr) {
        LOGGER_ERROR("read_file_to_buffer: failed to allocate buffer");
        fclose(file);
        return ERROR_MEM_ALLOC;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        LOGGER_ERROR("read_file_to_buffer: failed to read entire file");
        free(buffer);
        return ERROR_OPEN_FILE;
    }
    
    buffer[file_size] = '\0';
    *buffer_out = buffer;
    *buffer_size_out = (size_t)file_size;
    return ERROR_NO;
}

static error_code cleanup_existing_tree(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    
    if (tree->root != nullptr) {
        error_code cleanup_error = tree_remove_subtree(tree, tree->root);
        if (cleanup_error != ERROR_NO) {
            LOGGER_WARNING("cleanup_existing_tree: failed to cleanup existing tree");
        }
        tree->root = nullptr;
        tree->size = 0;
    }
    return ERROR_NO;
}

static error_code parse_tree_from_buffer(tree_t* tree, char* buffer, size_t buffer_size) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(buffer != nullptr, "buffer is nullptr");
    
    char* curr = skip_whitespace(buffer);
    
    if (*curr == '\0') {
        LOGGER_DEBUG("parse_tree_from_buffer: empty file, returning empty tree");
        return ERROR_NO;
    }
    
    bool parse_error = false;
    tree_node_t* root = read_node(tree, &curr, tree->head, &parse_error, buffer, buffer_size);
    if (parse_error) {
        LOGGER_ERROR("parse_tree_from_buffer: failed to parse tree");
        return ERROR_INVALID_STRUCTURE;
    }
    
    if (root == nullptr) {
        tree->root = nullptr;
        tree->head->right = nullptr;
        tree->size = 0;
        return ERROR_NO;
    }
    
    tree->size = count_nodes(root);
    
    LOGGER_DEBUG("parse_tree_from_buffer: successfully parsed tree with %zu nodes", tree->size);
    return ERROR_NO;
}

error_code tree_read_from_file(tree_t* tree, const char* filename) {
    HARD_ASSERT(tree != nullptr, "tree pointer is nullptr");
    HARD_ASSERT(filename != nullptr, "filename is nullptr");
    
    LOGGER_DEBUG("tree_read_from_file: started, filename=%s", filename);
    
    error_code error = ensure_tree_initialized(tree);
    if (error != ERROR_NO) {
        return error;
    }
    
    char* buffer = nullptr;
    size_t buffer_size = 0;
    error = read_file_to_buffer(filename, &buffer, &buffer_size);
    if (error != ERROR_NO) {
        return error;
    }
    
    if (tree->file_buffer != nullptr) {
        free(tree->file_buffer);
    }
    tree->file_buffer = buffer;
    
    error = cleanup_existing_tree(tree);
    if (error != ERROR_NO) {
        return error;
    }
    
    error = parse_tree_from_buffer(tree, buffer, buffer_size);
    if (error != ERROR_NO) {
        return error;
    }
    
    LOGGER_DEBUG("tree_read_from_file: successfully read tree with %zu nodes", tree->size);
    return ERROR_NO;
}

static error_code write_node(FILE* file, tree_node_t* node) {
    HARD_ASSERT(file != nullptr, "file is nullptr");
    
    if (node == nullptr) {
        if (fprintf(file, "nil") < 0) {
            LOGGER_ERROR("write_node: fprintf failed for nil");
            return ERROR_OPEN_FILE;
        }
        return ERROR_NO;
    }
    
    if (fprintf(file, "(\"") < 0) {
        LOGGER_ERROR("write_node: fprintf failed for opening");
        return ERROR_OPEN_FILE;
    }
    
    if (node->value != nullptr) {
        if (fprintf(file, "%s", node->value) < 0) {
            LOGGER_ERROR("write_node: fprintf failed for value");
            return ERROR_OPEN_FILE;
        }
    }
    
    if (fprintf(file, "\" ") < 0) {
        LOGGER_ERROR("write_node: fprintf failed for closing quote");
        return ERROR_OPEN_FILE;
    }
    
    error_code error = write_node(file, node->left);
    if (error != ERROR_NO) return error;
    
    if (fprintf(file, " ") < 0) {
        LOGGER_ERROR("write_node: fprintf failed for space");
        return ERROR_OPEN_FILE;
    }
    
    error = write_node(file, node->right);
    if (error != ERROR_NO) return error;
    
    if (fprintf(file, ")") < 0) {
        LOGGER_ERROR("write_node: fprintf failed for closing paren");
        return ERROR_OPEN_FILE;
    }
    
    return ERROR_NO;
}

error_code tree_write_to_file(const tree_t* tree, const char* filename) {
    HARD_ASSERT(tree != nullptr, "tree pointer is nullptr");
    HARD_ASSERT(filename != nullptr, "filename is nullptr");
    
    LOGGER_DEBUG("tree_write_to_file: started, filename=%s", filename);
    
    FILE* file = fopen(filename, "w");
    if (file == nullptr) {
        LOGGER_ERROR("tree_write_to_file: failed to open file '%s'", filename);
        return ERROR_OPEN_FILE;
    }
    
    error_code error = ERROR_NO;
    if (tree->root != nullptr) {
        error = write_node(file, tree->root);
    }
    
    if (fclose(file) != 0) {
        LOGGER_ERROR("tree_write_to_file: failed to close file");
        if (error == ERROR_NO) error = ERROR_OPEN_FILE;
    }
    
    if (error == ERROR_NO) {
        LOGGER_DEBUG("tree_write_to_file: successfully wrote tree to file");
    }
    
    return error;
}
