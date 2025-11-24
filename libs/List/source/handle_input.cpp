#include "handle_input.h"
#include "error_handler.h"
#include "logger.h"
#include "list_info.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

const int MAX_FILE_REQUESTS = 10;
const int MAX_WORD_LENGTH   = 128;
#define SCAN_FORMAT "%127s"

//==============================================================================

static error_code read_ssize_t(ssize_t* value);
static void clear_input_buffer();
static error_code read_double(double* value);
static error_code perform_auto_insert(list_t* list, ssize_t index, double value);
static error_code perform_manual_insert(list_t* list, ssize_t index, double value);
static error_code handle_auto_insert_console(list_t* list);
static error_code handle_auto_remove_console(list_t* list);
static error_code handle_manual_insert_console(list_t* list);
static error_code handle_manual_remove_console(list_t* list);
static FILE* open_file_console(const char* mode);
static error_code handle_auto_insert_file(list_t* list, FILE* file);
static error_code handle_auto_remove_file(list_t* list, FILE* file);
static error_code handle_manual_insert_file(list_t* list, FILE* file);
static error_code handle_manual_remove_file(list_t* list, FILE* file);
//------------------------------------------------------------------------------
typedef error_code (*file_command_handler_t)(list_t*, FILE*);
static file_command_handler_t get_file_command_handler(const char* command);

//==============================================================================

static void clear_input_buffer() {
    int c = 0;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

//==============================================================================

static error_code read_ssize_t(ssize_t* value) {
    if (scanf("%ld", value) != 1) {
        clear_input_buffer();
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return ERROR_NO;
}

static error_code read_double(double* value) {
    if (scanf("%lf", value) != 1) {
        clear_input_buffer();
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return ERROR_NO;
}

//==============================================================================

static error_code perform_auto_insert(list_t* list, ssize_t index, double value) {
    ssize_t physical_idx = list_insert_auto(list, index, value);
    if (physical_idx == -1) {
        LOGGER_ERROR("Auto insert failed");
        return ERROR_INSERT_FAIL;
    }
    LOGGER_DEBUG("%lf inserted at logical index %ld (physical: %ld)", 
                 value, index, physical_idx);
    return ERROR_NO;
}

static error_code perform_manual_insert(list_t* list, ssize_t index, double value) {
    ssize_t physical_idx = list_insert_after(list, index, value);
    if (physical_idx == -1) {
        LOGGER_ERROR("Manual insert failed");
        return ERROR_INSERT_FAIL;
    }
    LOGGER_DEBUG("%lf inserted after %ld (physical: %ld)", 
                 value, index, physical_idx);
    return ERROR_NO;
}

//==============================================================================

static error_code handle_auto_insert_console(list_t* list) {
    ssize_t index = 0;
    double value  = 0;
    
    if (read_ssize_t(&index) != ERROR_NO || read_double(&value) != ERROR_NO) {
        LOGGER_ERROR("Invalid arguments for auto insert");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return perform_auto_insert(list, index, value);
}

static error_code handle_auto_remove_console(list_t* list) {
    ssize_t index = 0;
    if (read_ssize_t(&index) != ERROR_NO) {
        LOGGER_ERROR("Invalid index for auto remove");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return list_remove_auto(list, index);
}

static error_code handle_manual_insert_console(list_t* list) {
    ssize_t index = 0;
    double value  = 0;
    
    if (read_ssize_t(&index) != ERROR_NO || read_double(&value) != ERROR_NO) {
        LOGGER_ERROR("Invalid arguments for manual insert");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return perform_manual_insert(list, index, value);
}

static error_code handle_manual_remove_console(list_t* list) {
    ssize_t index = 0;;
    if (read_ssize_t(&index) != ERROR_NO) {
        LOGGER_ERROR("Invalid index for manual remove");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return list_remove(list, index);
}

//==============================================================================

static FILE* open_file_console(const char* mode) {
    char file_name[100] = {};
    ssize_t cnt = 0;
    FILE* file = NULL;

    while (cnt < MAX_FILE_REQUESTS) {
        printf("Enter filename: ");
        if (scanf("%99s", file_name) != 1) {
            clear_input_buffer();
            continue;
        }

        if (strcmp(file_name, "exit") == 0) {
            return NULL;
        }

        file = fopen(file_name, mode);
        if (file != NULL) {
            return file;
        }

        printf("Error opening file, try again\n"
               "Type \"exit\" to cancel\n");
        cnt++;
    }
    return NULL;
}

//==============================================================================

typedef error_code (*file_command_handler_t)(list_t*, FILE*);

static error_code handle_auto_insert_file(list_t* list, FILE* file) {
    ssize_t index    = 0;
    double value = 0;
    if (fscanf(file, "%ld %lf", &index, &value) != 2) {
        LOGGER_ERROR("Invalid arguments for auto insert in file");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return perform_auto_insert(list, index, value);
}

static error_code handle_auto_remove_file(list_t* list, FILE* file) {
    ssize_t index = 0;
    if (fscanf(file, "%ld", &index) != 1) {
        LOGGER_ERROR("Invalid index for auto remove in file");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return list_remove_auto(list, index);
}

static error_code handle_manual_insert_file(list_t* list, FILE* file) {
    ssize_t index;
    double value;
    if (fscanf(file, "%ld %lf", &index, &value) != 2) {
        LOGGER_ERROR("Invalid arguments for manual insert in file");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return perform_manual_insert(list, index, value);
}

static error_code handle_manual_remove_file(list_t* list, FILE* file) {
    ssize_t index = 0;
    if (fscanf(file, "%ld", &index) != 1) {
        LOGGER_ERROR("Invalid index for manual remove in file");
        return ERROR_INCORRECT_ARGS_CMD;
    }
    return list_remove(list, index);
}

static file_command_handler_t get_file_command_handler(const char* command) {
    if (strcmp(command, "ai") == 0) return handle_auto_insert_file;
    if (strcmp(command, "ar") == 0) return handle_auto_remove_file;
    if (strcmp(command, "mi") == 0) return handle_manual_insert_file;
    if (strcmp(command, "mr") == 0) return handle_manual_remove_file;
    return NULL;
}

static error_code process_file_commands(list_t* list, FILE* input_file, bool use_console_output) {
    char word[MAX_WORD_LENGTH];
    
    while (fscanf(input_file, SCAN_FORMAT, word) == 1) {
        if (strcmp(word, "exit") == 0) {
            break;
        }

        file_command_handler_t handler = get_file_command_handler(word);
        if (handler == NULL) {
            LOGGER_ERROR("Unknown command in file: %s", word);
            continue;
        }
        error_code result = handler(list, input_file);
        if (result != ERROR_NO && use_console_output) {
            LOGGER_ERROR("Command %s failed with error %ld", word, result);
        }
    }
    
    return ERROR_NO;
}

//==============================================================================

error_code handle_interactive_console_input(list_t* list) {
    char word[MAX_WORD_LENGTH];
    
    while (true) {
        printf("Enter command (ai/ar/mi/mr/exit): ");
        if (scanf(SCAN_FORMAT, word) != 1) {
            clear_input_buffer();
            continue;
        }

        if (strcmp(word, "exit") == 0) {
            break;
        }

        error_code error = 0;
        if (strcmp(word, "ai") == 0) {
            error |= handle_auto_insert_console(list);
        } else if (strcmp(word, "ar") == 0) {
            error |= handle_auto_remove_console(list);
        } else if (strcmp(word, "mi") == 0) {
            error |= handle_manual_insert_console(list);
        } else if (strcmp(word, "mr") == 0) {
            error |= handle_manual_remove_console(list);
        } else {
            LOGGER_ERROR("Unknown command: %s", word);
            clear_input_buffer();
            continue;
        }
        clear_input_buffer();
        RETURN_IF_ERROR(error);
    }
    
    return ERROR_NO;
}

error_code handle_interactive_file_input(list_t* list) {
    printf("Input name of file\n");
    FILE* input_file = open_file_console("r");
    if (input_file == NULL) {
        LOGGER_ERROR("Failed to open input file");
        return ERROR_OPEN_FILE;
    }

    error_code result = process_file_commands(list, input_file, true);
    fclose(input_file);
    return result;
}

error_code handle_file_input(list_t* list, char* name_input_file, char* name_output_file) {
    FILE* input_file = fopen(name_input_file, "r");
    if (input_file == NULL) {
        LOGGER_ERROR("Error open file: %s", name_input_file);
        return ERROR_OPEN_FILE;
    }

    FILE* output_file = fopen(name_output_file, "w");
    if (output_file == NULL) {
        LOGGER_ERROR("Error open fils: %s", name_output_file);
        fclose(input_file);
        return ERROR_OPEN_FILE;
    }

    char word[MAX_WORD_LENGTH];
    while (fscanf(input_file, SCAN_FORMAT, word) == 1) {
        if (strcmp(word, "exit") == 0) {
            break;
        }

        file_command_handler_t handler = get_file_command_handler(word);
        if (handler == NULL) {
            fprintf(output_file, "Unknown command: %s\n", word);
            continue;
        }

        error_code result = handler(list, input_file);
        if (result != ERROR_NO) {
            fprintf(output_file, "Command %s failed with error %ld\n", word, result);
        } else {
            fprintf(output_file, "Command %s executed successfully\n", word);
        }
    }

    fclose(input_file);
    fclose(output_file);
    return ERROR_NO;
}