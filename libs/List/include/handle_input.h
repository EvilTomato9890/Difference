#ifndef HANDLE_INPUT_H_INCLUDED
#define HANDLE_INPUT_H_INCLUDED

#include "error_handler.h"
#include "list_operations.h"

error_code handle_interactive_console_input(list_t* list);
error_code handle_interactive_file_input   (list_t* list);
error_code handle_file_input               (list_t* list, char* name_input_file, char* name_output_file);

#endif