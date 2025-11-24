#include <stdarg.h>
#include <time.h>

#include "asserts.h"
#include "colors.h"
#include "logger.h"
#include "../include/tree_info.h"



static FILE *output_stream = NULL;
static logger_output_type output_type = EXTERNAL_STREAM;
static int color_enabled = 1;



#if defined(WinV)
    #define localtime_r(T,Tm) (localtime_s(Tm,T) ? NULL : Tm)
#endif

const char* logger_mode_string(const logger_mode_type type) {
    switch (type) {
        case LOGGER_MODE_DEBUG:                   return "DEBUG";
        case LOGGER_MODE_INFO:                    return "INFO";
        case LOGGER_MODE_WARNING:                 return "WARNING";
        case LOGGER_MODE_ERROR:                   return "ERROR";
        default: SOFT_ASSERT(false, "WRONG MODE");return "?";
    }
}

void logger_time_string(char *buff, size_t n) {
    HARD_ASSERT(buff != nullptr, "buff is nullptr");
    // HH:MM:SS:YYYY-MM-DD 
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    if(!strftime(buff, n, "%H:%M:%S:%Y-%m-%d", &tmv)) {
        SOFT_ASSERT(false, "Invalid time input");
    }
}

const char* logger_color_on(const logger_mode_type mode) {
    if (!color_enabled) return "";
    switch (mode) {
        case LOGGER_MODE_DEBUG:   return CYAN;
        case LOGGER_MODE_INFO:    return BLUE; 
        case LOGGER_MODE_WARNING: return YELLOW;
        case LOGGER_MODE_ERROR:   return RED; 
        default:                  return "";
    }
}

void logger_initialize_stream(FILE *stream) {
    if (output_type == OWNED_FILE && output_stream) {
        fclose(output_stream);
    }
    output_stream = stream ? stream : stderr;
    output_type = EXTERNAL_STREAM;
}

int logger_initialize_file(const char *path) {
    HARD_ASSERT(path != nullptr, "File path is empty");
    FILE *f = fopen(path, "a");
    HARD_ASSERT(path != nullptr, "Incorrect read of logger file");

    if (output_type == OWNED_FILE && output_stream) {
        fclose(output_stream);
    }
    output_stream = f;
    output_type = OWNED_FILE;
    return 0;
}

void logger_close() {
    if (output_type == OWNED_FILE && output_stream) fclose(output_stream);
}

void logger_log_message(const logger_mode_type mode,
                        const char *file, int line,
                        const char *format, ...) {
    if (!output_stream) {
        color_enabled = 0;
        output_stream = stderr;
    }
    char Time[32] = "";
    logger_time_string(Time, sizeof Time);
    if(output_type != EXTERNAL_STREAM) {
        fprintf(output_stream, "%s. %s:%d. %s. ",
            Time, file, line, logger_mode_string(mode));
    } else {
        const char *color_on  = logger_color_on(mode);
        fprintf(output_stream, "[%s] %s:%d. %s%s%s. ",
            Time, file, line, color_on, logger_mode_string(mode), RESET);
    }

    va_list ap;
    va_start(ap, format);
    vfprintf(output_stream, format, ap);
    va_end(ap);
    
    fputc('\n', output_stream);
}

ON_DEBUG(
void stack_dumb(const stack_t* stack) {
    HARD_ASSERT(stack != nullptr, "Stack is nullptr");
    HARD_ASSERT(stack->is_constructed, "Stack is not constructed");
    if (!output_stream) {
        color_enabled = 0;
        output_stream = stderr;
    }
    if(output_type == EXTERNAL_STREAM) {
        fprintf(output_stream,                  
            "Stack_t<%s> [%p] \"%s\" from %s at %s:%d\n"
            "{\n"
            "\tcapacity  = %lu\n"
            "\tsize      = %lu\n"
            "\tdata[%lu] = [%p]\n"
            "\t{\n", stack->ver_info.var_type, stack, \
            stack->ver_info.var_name, stack->ver_info.creation_func, stack->ver_info.creation_file, stack->ver_info.creation_line, \
            stack->capacity, stack->size, stack->capacity, stack->data);
        ON_CANARY_DEBUG(
            fprintf(output_stream, "  CANARY [-1] = %p\n", (void*)stack->data[-1]);
        )
        for(size_t i = 0; i < stack->capacity; i++) {
            if(i < stack->size) {
                tree_node_t* node = stack->data[i];
                if (node != nullptr && node->value != nullptr) {
                    fprintf(output_stream, "        *[%lu] = %p (\"%s\")\n", i, (void*)node, node->value);
                } else {
                    fprintf(output_stream, "        *[%lu] = %p\n", i, (void*)node);
                }
            } else {
                fprintf(output_stream, "         [%lu] = %p (poison)\n", i, (void*)stack->data[i]);
            }
        }
        ON_CANARY_DEBUG(
            fprintf(output_stream, "  CANARY [%lu] = %p\n", stack->capacity, (void*)stack->data[stack->capacity]);
        )
        fprintf(output_stream, "\t}\n}\n");
    } 
}
)