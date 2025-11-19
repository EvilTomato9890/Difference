#ifndef DEBUG_META_H_INCLUDED
#define DEBUG_META_H_INCLUDED

#include <stddef.h>

#ifdef VERIFY_DEBUG
    #define ON_DEBUG(...) __VA_ARGS__
#else
    #define ON_DEBUG(...)
#endif

struct ver_info_t {
    const char* file;
    const char* func;
    int         line;
};

#define VER_INIT ver_info_t{__FILE__, __func__, __LINE__}

#endif

