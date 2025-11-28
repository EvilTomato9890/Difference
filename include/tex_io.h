#ifndef TEST_IO_H_INCLUDED
#define TEST_IO_H_INCLUDED

#include <stdio.h>

void print_tex_header(FILE* tex);
void print_tex_footer(FILE* tex);
error_code tree_print_tex_expr(const tree_t* tree,
                                tree_node_t*  node,
                                const char*   fmt, ...);
                                
#endif