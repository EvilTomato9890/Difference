#include <stdio.h>
#include <stdlib.h>
#include "error_handler.h"
#include "logger.h"
#include "asserts.h"
#include "my_string.h"
#include "file_operations.h"

#include <errno.h>

static long get_file_size(FILE* file) {
    if(file == nullptr) {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return file_size;
}

error_code read_file_to_buffer(FILE* file, string_t* buff_str) {
    HARD_ASSERT(file != nullptr, "file is nullptr");
    HARD_ASSERT(buff_str != nullptr, "buffer_str is nullptr");
    
    
    LOGGER_DEBUG("read_file_to_buffer: started");

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

    if (bytes_read != (size_t)file_size) {
        LOGGER_ERROR("read_file_to_buffer: failed to read entire file");
        free(buffer);
        return ERROR_OPEN_FILE;
    }
    
    buff_str->ptr = buffer;
    buff_str->len = (size_t)file_size;
    return ERROR_NO;
}

error_code read_file_to_buffer_by_name(string_t* buff, const char* filename) {
    HARD_ASSERT(filename != nullptr, "Filename is nullptr");
    HARD_ASSERT(buff     != nullptr, "buff is nullptr");
    FILE* read_file = fopen(filename, "r");
    if(!read_file) {
        LOGGER_ERROR("forest_read_file: Opening file failed");
        errno = 0;
        return ERROR_OPEN_FILE;
    }

    error_code error = read_file_to_buffer(read_file, buff);
    if(error != ERROR_NO) { //FIXME - закрытие и очитска буфера
        LOGGER_ERROR("forest_read_file: read_file_to_buffer failed");
        return error;
    }

    int err = fclose(read_file);
    if(err != 0) {
        LOGGER_ERROR("forest_read_File: failed to close file");
        return ERROR_CLOSE_FILE;
    }

    return error;
}