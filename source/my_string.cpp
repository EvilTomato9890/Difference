#include "../include/my_string.h"

int my_ssstrcmp(string_t str1, string_t str2) {
    if(str1.len > str2.len) return strncmp(str1.ptr, str2.ptr, str1.len);
    return strncmp(str1.ptr, str2.ptr, str1.len);
}

int my_scstrcmp(string_t str1, const char* str2) {
    my_ssstrcmp(str1, {str2, strlen(str2)});
}
