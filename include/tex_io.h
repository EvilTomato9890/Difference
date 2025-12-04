#ifndef TEST_IO_H_INCLUDED
#define TEST_IO_H_INCLUDED

#include <stdio.h>
#include "tree_info.h"

//================================================================================

void print_tex_header(FILE* tex);
void print_tex_footer(FILE* tex);
void print_tex_introduction(FILE* tex);

error_code print_tex_expr     (const tree_t* tree, tree_node_t* node, const char* fmt, ...);  

error_code print_tex_expr_full(const tree_t* tree, tree_node_t* node, const char* fmt, ...);

error_code print_diff_step(tree_t* tree, tree_node_t* node, const char* pattern);

error_code print_diff_step_tex_fmt(tree_t* tree, tree_node_t* node);

error_code print_tex_expr_with_squashes(tree_t* tree, tree_node_t* node, const char* fmt, ...);

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

ssize_t get_tex_len(const tree_t* tree, tree_node_t* node);

void tex_clear_squash(tree_t* tree);

//================================================================================

#define EXPR_HEAD "\n\\begin{math}\n" //REVIEW - Так или функцию
#define EXPR_TAIL "\n\\end{math}\n\n"
#define DIFFERENTIAL "\\frac{d}{dx}"

//================================================================================

#endif