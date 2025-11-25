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
#include "tree_file_io.h"
#include "../libs/StackDead-main/stack.h"
#include "my_string.h"
#include "file_operations.h"

//================================================================================

struct func_struct {
    func_type_t func_type;
    const char* func_name;
};

#define HANDLE_FUNC(op_code, str_name) \
    {op_code, #str_name},

static func_struct op_codes[] = {
    #include "copy_past_file"
};

#undef HANDLE_FUNC

const int op_codes_num = sizeof(op_codes) / sizeof(func_struct);

//================================================================================

static func_type_t get_op_code(const char* func_name, error_code* error) {
    HARD_ASSERT(func_name != nullptr, "func_name nullptr");

    LOGGER_DEBUG("A: %d", op_codes_num);
    for(size_t i = 0; i < op_codes_num; i++) {
        LOGGER_DEBUG("AA: %d и %s", op_codes[i].func_type, op_codes[i].func_name);
        if(strcmp(func_name, op_codes[i].func_name) == 0) return op_codes[i].func_type;
    }

    LOGGER_ERROR("Func doesn`t found");
    *error |= ERROR_UNKNOWN_FUNC;
    return ADD;
}

static char* skip_whitespace(char* buff) {
    HARD_ASSERT(buff != nullptr, "curr is nullptr");

    char* curr = buff;
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

static void debug_print_buffer_remainder(tree_t* tree, const char* value_start, char* value_end, string_t buff_str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(value_start != nullptr, "value_start is nullptr");
    
    ON_DEBUG({
        if (tree->dump_file != nullptr) {
            if (value_end != nullptr && buff_str.ptr != nullptr && value_end >= buff_str.ptr && value_end < buff_str.ptr + buff_str.len) {
                char original_char = *value_end;
                *value_end = '"';
                
                size_t remainder_len = buff_str.len - (value_end - buff_str.ptr);
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

static int try_parse_node_value(tree_t* tree_ptr, node_type_t* node_type_ptr, value_t* node_value_ptr,
                                char** value_start_ptr_ref,  char** value_end_ptr_ref,  char** current_ptr_ref,
                                error_code* error)
{
    HARD_ASSERT(current_ptr_ref      != nullptr, "current_ptr_ref is nullptr");
    HARD_ASSERT(node_type_ptr        != nullptr, "node_type_ptr is nullptr");
    HARD_ASSERT(node_value_ptr       != nullptr, "node_value_ptr is nullptr");
    HARD_ASSERT(value_start_ptr_ref  != nullptr, "value_start_ptr_ref is nullptr");
    HARD_ASSERT(value_end_ptr_ref    != nullptr, "value_end_ptr_ref is nullptr");
    HARD_ASSERT(error       != nullptr, "error is nullptr");

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
            *error |= ERROR_READ_FILE;
            return 0;
        }
        value_end_ptr = scan_pointer;

        *node_type_ptr = VARIABLE;
        node_value_ptr->var_idx = get_or_add_var_idx({value_start_ptr, (unsigned long)(value_end_ptr - value_start_ptr)}, 0, 
                                                      tree_ptr->var_stack, error);
        
        if(*error != ERROR_NO) {
            LOGGER_ERROR("Add_var error");
            return 0;
        }

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

        int is_number_flag = is_integer_token(value_start_ptr);
        if (is_number_flag) {
            errno = 0;
            char* number_end_ptr = nullptr;
            unsigned long long numeric_val = strtoull(value_start_ptr, &number_end_ptr, 10);
            if (errno != 0 || number_end_ptr == value_start_ptr || *number_end_ptr != '\0') {
                LOGGER_ERROR("read_node: invalid constant '%s'", value_start_ptr);
                *error |= ERROR_READ_FILE;
                return 0;
            }
            *node_type_ptr = CONSTANT;
            node_value_ptr->constant = (const_val_type)numeric_val;
        } else {
            *node_type_ptr = FUNCTION;
            node_value_ptr->func = get_op_code(value_start_ptr, error);
            if(errno != 0) {
                LOGGER_ERROR("try_parse_node_value: invalid function '%s'", value_start_ptr);
                *error |= ERROR_READ_FILE;
                return 0;
            }
        }
        current_ptr = value_end_ptr;
    }

    *current_ptr_ref     = current_ptr;
    *value_start_ptr_ref = value_start_ptr;
    *value_end_ptr_ref   = value_end_ptr;
    return 1;
}

static tree_node_t* read_node(tree_t* tree_ptr, tree_node_t* parent_node_ptr,
                              error_code* error,
                              char** current_ptr_ref, string_t buff_str)
{
    HARD_ASSERT(tree_ptr        != nullptr, "tree_ptr is nullptr");
    HARD_ASSERT(current_ptr_ref != nullptr, "current_ptr_ref is nullptr");
    HARD_ASSERT(error  != nullptr, "error is nullptr");
    
    char* current_ptr = skip_whitespace(*current_ptr_ref);

    if (*current_ptr == '(') {
        current_ptr++;
        current_ptr = skip_whitespace(current_ptr);

        if (*current_ptr == '(' || *current_ptr == ')' || *current_ptr == '\0') {
            LOGGER_ERROR("read_node: expected token after '('");
            *error |= ERROR_READ_FILE;
            return nullptr;
        }

        node_type_t node_type  = {};
        value_t     node_value = {};

        char* value_start_ptr = nullptr;
        char* value_end_ptr   = nullptr;

        if (!try_parse_node_value(tree_ptr, &node_type, &node_value,
                                  &value_start_ptr, &value_end_ptr, &current_ptr,
                                  error)) {
            return nullptr;
        }

        tree_node_t* new_node_ptr = init_node(node_type, node_value, nullptr, nullptr);
        if (new_node_ptr == nullptr) {
            *error |= ERROR_READ_FILE;
            return nullptr;
        }

        attach_node_to_parent(tree_ptr, new_node_ptr, parent_node_ptr);

        if (value_start_ptr != nullptr && value_end_ptr != nullptr) {
            debug_print_buffer_remainder(tree_ptr,
                                         value_start_ptr,
                                         value_end_ptr,
                                         buff_str);
        }

        current_ptr = skip_whitespace(current_ptr);
        new_node_ptr->left = read_node(tree_ptr, new_node_ptr, error, &current_ptr, buff_str);
        if (*error) {
            cleanup_failed_node(tree_ptr, new_node_ptr, parent_node_ptr);
            return nullptr;
        }

        current_ptr = skip_whitespace(current_ptr);
        new_node_ptr->right = read_node(tree_ptr, new_node_ptr, error,  &current_ptr, buff_str);
        if (*error) {
            cleanup_failed_node(tree_ptr, new_node_ptr, parent_node_ptr);
            return nullptr;
        }

        current_ptr = skip_whitespace(current_ptr);
        if (*current_ptr != ')') {
            LOGGER_ERROR("read_node: expected ')' after children");
            cleanup_failed_node(tree_ptr, new_node_ptr, parent_node_ptr);
            *error |= ERROR_READ_FILE;
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
        *error |= ERROR_READ_FILE;
        return nullptr;
    }

    LOGGER_ERROR("read_node: unexpected character '%c'", *current_ptr);
    *error |= ERROR_READ_FILE;
    return nullptr;
}

//================================================================================

error_code tree_parse_from_buffer(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(tree->buff.ptr != nullptr, "buffer is nullptr");
    
    LOGGER_DEBUG("parse_tree_from_buffer: started");

    char* curr = skip_whitespace(tree->buff.ptr);
    
    if (*curr == '\0') {
        LOGGER_DEBUG("parse_tree_from_buffer: empty file, returning empty tree");
        return ERROR_NO;
    }

    
    
    error_code parse_error = 0;
    tree_node_t* root = read_node(tree, tree->head, &parse_error, &curr, tree->buff);

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
    tree->size = count_nodes_recursive(root);
    
    LOGGER_DEBUG("parse_tree_from_buffer: successfully parsed tree with %zu nodes", tree->size);
    return ERROR_NO;
}

error_code tree_read_from_file(tree_t* tree, FILE* file) {
    HARD_ASSERT(tree != nullptr, "tree pointer is nullptr");
    HARD_ASSERT(file != nullptr, "filename is nullptr");
    
    LOGGER_DEBUG("tree_read_from_file: started");
    
    if (tree->head == nullptr) {
        LOGGER_ERROR("ensure_tree_initialized: tree_init failed");
        return ERROR_NO_INIT;
    }    

    error_code error = ERROR_NO;

    string_t buff_str = {};
    error = read_file_to_buffer(file, &buff_str);
    if (error != ERROR_NO) {
        return error;
    }

    tree->buff = buff_str;

    error = tree_parse_from_buffer(tree);
    if (error != ERROR_NO) {
        return error;
    }
    
    LOGGER_DEBUG("tree_read_from_file: successfully read tree with %zu nodes", tree->size);
    return ERROR_NO;
}

const char* get_func_name_by_type(func_type_t func_type_value) {
    for (size_t index_value = 0; index_value < (size_t)op_codes_num; ++index_value) {
        if (op_codes[index_value].func_type == func_type_value) {
            return op_codes[index_value].func_name;
        }
    }

    LOGGER_ERROR("write_node: unknown func_type %d", (int)func_type_value);
    return "";
}

static error_code write_node(const tree_t* tree_ptr, FILE* file_ptr, tree_node_t* node_ptr) {
    HARD_ASSERT(file_ptr != nullptr, "file is nullptr");
    
    if (node_ptr == nullptr) {
        if (fprintf(file_ptr, "nil") < 0) {
            LOGGER_ERROR("write_node: fprintf failed for nil");
            return ERROR_OPEN_FILE;
        }
        return ERROR_NO;
    }
    
    if (fprintf(file_ptr, "(") < 0) {
        LOGGER_ERROR("write_node: fprintf failed for opening");
        return ERROR_OPEN_FILE;
    }
    
    if (node_ptr->type == VARIABLE) {
                LOGGER_WARNING("AAAAAA, %ld", node_ptr->value.var_idx);

        string_t curr_str = tree_ptr->var_stack->data[node_ptr->value.var_idx].str;
        LOGGER_WARNING("AAAAAA");
        if (fprintf(file_ptr, "\"%.*s\"", (int)curr_str.len, curr_str.ptr) < 0) {
            LOGGER_ERROR("write_node: fprintf failed for variable");
            return ERROR_OPEN_FILE;
        }
    } else if (node_ptr->type == CONSTANT) {
        if (fprintf(file_ptr, "%llu", (unsigned long long)node_ptr->value.constant) < 0) {
            LOGGER_ERROR("write_node: fprintf failed for constant");
            return ERROR_OPEN_FILE;
        }
    } else if (node_ptr->type == FUNCTION) {
        const char* func_name_ptr = get_func_name_by_type(node_ptr->value.func);
        if (fprintf(file_ptr, "%s", func_name_ptr) < 0) {
            LOGGER_ERROR("write_node: fprintf failed for function");
            return ERROR_OPEN_FILE;
        }
    }

    if (fprintf(file_ptr, " ") < 0) {
        LOGGER_ERROR("write_node: fprintf failed for space after value");
        return ERROR_OPEN_FILE;
    }

    error_code error_value = write_node(tree_ptr, file_ptr, node_ptr->left);
    if (error_value != ERROR_NO) return error_value;
    
    if (fprintf(file_ptr, " ") < 0) {
        LOGGER_ERROR("write_node: fprintf failed for space between children");
        return ERROR_OPEN_FILE;
    }
    
    error_value = write_node(tree_ptr, file_ptr, node_ptr->right);
    if (error_value != ERROR_NO) return error_value;
    
    if (fprintf(file_ptr, ")") < 0) {
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
        error = write_node(tree, file, tree->root);
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
