#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "asserts.h"
#include "logger.h"
#include "tree_operations.h"
#include "tree_verification.h"
#include "error_handler.h"
#include "tree_info.h"

//================================================================================

struct func_struct {
    func_type_t func_type;
    const char* func_name;
};

#define HANDLE_COMMAND(op_code, str_name) \
    {op_code, str_name},

static func_struct op_codes[] = {
    #include "copy_past_file"
};

#undef HANDLE_COMMAND

const int op_codes_num = sizeof(op_codes) / sizeof(func_struct);

//================================================================================

static func_type_t get_op_code(const char* func_name, error_code* error) {
    HARD_ASSERT(func_name != nullptr, "func_name nullptr");

    for(size_t i = 0; i < op_codes_num; i++) {
        if(strcmp(func_name, op_codes[i].func_name) == 0) return op_codes[i].func_type;
    }

    LOGGER_ERROR("Func doesn`t found");
    *error |= ERROR_UNKNOWN_FUNC;
    return ADD;
}

static char* skip_whitespace(char* curr) {
    HARD_ASSERT(curr != nullptr, "curr is nullptr");
    while (*curr != '\0' && isspace((unsigned char)*curr)) {
        curr++;
    }
    return curr;
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

//================================================================================

static int is_integer_token(const char* text_ptr) {
    if (text_ptr == NULL || *text_ptr == '\0') {
        return 0;
    }

    const unsigned char* scan_pointer = (const unsigned char*)text_ptr;
    while (*scan_pointer != '\0') {
        if (!isdigit(*scan_pointer)) {
            return 0;
        }
        scan_pointer++;
    }
    return 1;
}

static int try_parse_node_value(char** current_ptr_ref, node_type_t* node_type_ptr, value_t* node_value_ptr,
                            char** value_start_ptr_ref, char** value_end_ptr_ref,
                            error_code* error_code_ptr)
{
    HARD_ASSERT(current_ptr_ref      != nullptr, "current_ptr_ref is nullptr");
    HARD_ASSERT(node_type_ptr        != nullptr, "node_type_ptr is nullptr");
    HARD_ASSERT(node_value_ptr       != nullptr, "node_value_ptr is nullptr");
    HARD_ASSERT(value_start_ptr_ref  != nullptr, "value_start_ptr_ref is nullptr");
    HARD_ASSERT(value_end_ptr_ref    != nullptr, "value_end_ptr_ref is nullptr");
    HARD_ASSERT(error_code_ptr       != nullptr, "error_code_ptr is nullptr");

    char* current_ptr     = *current_ptr_ref;
    char* value_start_ptr = nullptr;
    char* value_end_ptr   = nullptr;

    if (*current_ptr == '\"') {
        current_ptr++;
        value_start_ptr = current_ptr;
        char* scan_pointer = current_ptr;
        while (*scan_pointer != '\0' && *scan_pointer != '\"') {
            scan_pointer++;
        }
        if (*scan_pointer != '\"') {
            LOGGER_ERROR("read_node: missing closing '\"' for variable");
            *error_code_ptr |= ERROR_READ_FILE;
            return 0;
        }
        *scan_pointer = '\0';
        value_end_ptr = scan_pointer;

        *node_type_ptr = VARIABLE;
        node_value_ptr->var_name = value_start_ptr;

        current_ptr = scan_pointer + 1;
    } else {
        value_start_ptr = current_ptr;
        char* scan_pointer = current_ptr;

        while (*scan_pointer != '\0' &&
               !isspace((unsigned char)*scan_pointer)) {
            scan_pointer++;
        }

        value_end_ptr = scan_pointer;
        char delimiter_char = *value_end_ptr;
        *value_end_ptr = '\0';

        int is_number_flag = is_integer_token(value_start_ptr);
        if (is_number_flag) {
            errno = 0;
            char* number_end_ptr = nullptr;
            unsigned long long numeric_val = strtoull(value_start_ptr, &number_end_ptr, 10);
            if (errno != 0 || number_end_ptr == value_start_ptr || *number_end_ptr != '\0') {
                LOGGER_ERROR("read_node: invalid constant '%s'", value_start_ptr);
                *error_code_ptr |= ERROR_READ_FILE;
                return 0;
            }
            *node_type_ptr = CONSTANT;
            node_value_ptr->constant = (const_val_type)numeric_val;
        } else {
            *node_type_ptr = FUNCTION;
            node_value_ptr->func = get_op_code(value_start_ptr, error_code_ptr);
            if(errno != 0) {
                LOGGER_ERROR("AAA");
                return 0;
            }
        }

        if (delimiter_char == '\0') {
            current_ptr = value_end_ptr;
        } else {
            current_ptr = value_end_ptr + 1;
        }
    }

    *current_ptr_ref     = current_ptr;
    *value_start_ptr_ref = value_start_ptr;
    *value_end_ptr_ref   = value_end_ptr;
    return 1;
}

static tree_node_t* read_node(tree_t* tree_ptr, char** current_ptr_ref, tree_node_t* parent_node_ptr,
                              error_code* error_code_ptr,
                              char* buffer_start_ptr, size_t buffer_size_val)
{
    HARD_ASSERT(tree_ptr        != nullptr, "tree_ptr is nullptr");
    HARD_ASSERT(current_ptr_ref != nullptr, "current_ptr_ref is nullptr");
    HARD_ASSERT(error_code_ptr  != nullptr, "error_code_ptr is nullptr");
    
    char* current_ptr = skip_whitespace(*current_ptr_ref);

    if (*current_ptr == '(') {
        current_ptr++;
        current_ptr = skip_whitespace(current_ptr);

        if (*current_ptr == '(' || *current_ptr == ')' || *current_ptr == '\0') {
            LOGGER_ERROR("read_node: expected token after '('");
            *error_code_ptr |= ERROR_READ_FILE;
            return nullptr;
        }

        node_type_t node_type  = {};
        value_t     node_value = {};

        char* value_start_ptr = nullptr;
        char* value_end_ptr   = nullptr;

        if (!try_parse_node_value(&current_ptr, &node_type, &node_value,
                              &value_start_ptr, &value_end_ptr,
                              error_code_ptr)) {
            return nullptr;
        }

        tree_node_t* new_node_ptr = init_node(node_type, node_value, parent_node_ptr, nullptr, nullptr);
        if (new_node_ptr == nullptr) {
            *error_code_ptr |= ERROR_READ_FILE;
            return nullptr;
        }

        attach_node_to_parent(tree_ptr, new_node_ptr, parent_node_ptr);

        if (value_start_ptr != nullptr && value_end_ptr != nullptr) {
            debug_print_buffer_remainder(tree_ptr,
                                         value_start_ptr,
                                         value_end_ptr,
                                         buffer_start_ptr,
                                         buffer_size_val);
        }

        current_ptr = skip_whitespace(current_ptr);
        new_node_ptr->left = read_node(tree_ptr, &current_ptr, new_node_ptr, error_code_ptr, buffer_start_ptr, buffer_size_val);
        if (*error_code_ptr) {
            cleanup_failed_node(tree_ptr, new_node_ptr, parent_node_ptr);
            return nullptr;
        }

        current_ptr = skip_whitespace(current_ptr);
        new_node_ptr->right = read_node(tree_ptr, &current_ptr, new_node_ptr, error_code_ptr, buffer_start_ptr, buffer_size_val);
        if (*error_code_ptr) {
            cleanup_failed_node(tree_ptr, new_node_ptr, parent_node_ptr);
            return nullptr;
        }

        current_ptr = skip_whitespace(current_ptr);
        if (*current_ptr != ')') {
            LOGGER_ERROR("read_node: expected ')' after children");
            cleanup_failed_node(tree_ptr, new_node_ptr, parent_node_ptr);
            *error_code_ptr |= ERROR_READ_FILE;
            return nullptr;
        }

        current_ptr++;
        *current_ptr_ref = current_ptr;
        return new_node_ptr;
    }

    if (*current_ptr == 'n') {
        if (strncmp(current_ptr, "nil", 3) == 0) {
            current_ptr += 3;
            *current_ptr_ref = current_ptr;
            return nullptr;
        }
        if (strncmp(current_ptr, "nill", 4) == 0) {
            current_ptr += 4;
            *current_ptr_ref = current_ptr;
            return nullptr;
        }
        LOGGER_ERROR("read_node: expected 'nil'");
        *error_code_ptr |= ERROR_READ_FILE;
        return nullptr;
    }

    LOGGER_ERROR("read_node: unexpected character '%c'", *current_ptr);
    *error_code_ptr |= ERROR_READ_FILE;
    return nullptr;
}

//================================================================================

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
    
    error_code parse_error = 0;
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
    
    if (node->type == VARIABLE && node->value.var_name != nullptr) {
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
