#ifndef LIST_VERIFICATION_H_INCLUDED
#define LIST_VERIFICATION_H_INCLUDED


#include <stdbool.h>
#include "list_info.h"
#include "error_handler.h"

enum dump_mode_t {
    DUMP_NO   = 0,
    DUMP_TEXT = 1,
    DUMP_IMG  = 2,
};

error_code list_verify(list_t* list,
                       ver_info_t ver_info,
                       dump_mode_t mode,
                       const char* fmt, ...);
void list_dump(list_t* list,
               ver_info_t ver_info,
               bool is_visual,
               const char* fmt, ...);


#endif 
