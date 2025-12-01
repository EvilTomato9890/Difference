#ifndef FILE_OPERATIONS_H_INCLUDED
#define FILE_OPERATIONS_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include "error_handler.h"
#include "my_string.h"

error_code read_file_to_buffer_by_name(string_t* buff, const char* filename);

error_code read_file_to_buffer(FILE* filename, string_t* buff_str);

#endif