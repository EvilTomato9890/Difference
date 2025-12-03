#ifndef TREE_FILE_H_INCLUDED
#define TREE_FILE_H_INCLUDED

#include "error_handler.h"
#include "tree_info.h"

error_code tree_read_from_file(tree_t* tree, const char* filename);
error_code tree_write_to_file(const tree_t* tree, const char* filename);

const char* get_func_name_by_type(func_type_t func_type_value); //TODO что делать

error_code tree_parse_from_buffer(tree_t* tree);



#endif