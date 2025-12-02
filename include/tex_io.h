#ifndef TEST_IO_H_INCLUDED
#define TEST_IO_H_INCLUDED

#include <stdio.h>

//================================================================================

void print_tex_header(FILE* tex);
void print_tex_footer(FILE* tex);
error_code tree_print_tex_expr(const tree_t* tree,
                                tree_node_t*  node,
                                const char*   fmt, ...);

error_code print_merge_tex_expr(tree_t* left_tree,  tree_node_t* left_node, 
                                tree_t* right_tree, tree_node_t* right_node,
                                const char* pattern);      

error_code print_diff_step(const tree_t* tree, tree_node_t*  node, const char* pattern);

//================================================================================

#define EXPR_HEAD "\\begin{align}\n\\begin{autobreak}\n"
#define EXPR_TAIL "\n\\end{autobreak}\n\\end{align}\n\n"
#define DIFFERENTIAL "\\frac{d}{dx}"

//================================================================================
#endif