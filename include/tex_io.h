#ifndef TEST_IO_H_INCLUDED
#define TEST_IO_H_INCLUDED

#include <stdio.h>

//================================================================================

void print_tex_header(FILE* tex);
void print_tex_footer(FILE* tex);
void print_tex_introduction(FILE* tex);

error_code print_tex_expr(const tree_t* tree,
                                tree_node_t*  node,
                                const char*   fmt, ...);  

error_code print_diff_step(const tree_t* tree, tree_node_t*  node, const char* pattern);

//================================================================================

void print_tex_H1(FILE* tex, const char* fmt, ...);

void print_tex_H2(FILE* tex, const char* fmt, ...);

void print_tex_P(FILE* tex, const char* fmt, ...);

void print_tex_delimeter(FILE* tex);

//================================================================================

void print_tex_const_diff_comment(FILE* tex);

void print_tex_var_diff_comment(FILE* tex);

void print_tex_basic_diff_comment(FILE* tex);

//================================================================================

#define EXPR_HEAD "\\begin{align*}\n\\begin{autobreak}\n" //REVIEW - Так или функцию
#define EXPR_TAIL "\n\\end{autobreak}\n\\end{align*}\n\n"
#define DIFFERENTIAL "\\frac{d}{dx}"

//================================================================================
#endif