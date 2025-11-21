#ifndef MY_STRING_H_INCLUDED
#define MY_STRING_H_INCLUDED

#include <stdlib.h>
#include <string.h>

struct string_t {
    const char* ptr;
    size_t      len;
};

int my_strcmp(string_t str1, string_t str2);

#endif