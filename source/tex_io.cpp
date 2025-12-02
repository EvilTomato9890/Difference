#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "asserts.h"
#include "logger.h"
#include "tree_operations.h"
#include "tree_verification.h"
#include "error_handler.h"
#include "tree_info.h"
#include "tree_file_io.h"
#include "../libs/StackDead-main/stack.h"
#include "my_string.h"
#include "file_operations.h"
#include "tex_io.h"

//================================================================================

struct func_struct {
    func_type_t func_type;
    const char* func_name;   
};

#define HANDLE_FUNC(op_code, str_name, ...) \
    { op_code, #str_name },

static func_struct op_codes[] = {
    #include "copy_past_file"
};

#undef HANDLE_FUNC

const int op_codes_num = sizeof(op_codes) / sizeof(func_struct);

//--------------------------------------------------------------------------------

const char* get_func_name_by_type(func_type_t func_type_value) {
    for (size_t index_value = 0; index_value < (size_t)op_codes_num; ++index_value) {
        if (op_codes[index_value].func_type == func_type_value) {
            return op_codes[index_value].func_name;
        }
    }

    LOGGER_ERROR("get_func_name_by_type: unknown func_type %d", (int)func_type_value);
    return "";
}
//================================================================================

struct tex_func_fmt {
    func_type_t  func_type;
    const char*  symbol;   
    const char*  pattern;  
};

#define HANDLE_FUNC(op_code, str_name, impl_name, num_args, priority, pattern, ...) \
    { op_code, #str_name, pattern },

static tex_func_fmt tex_fmt_table[] = {
    #include "copy_past_file"
};

#undef HANDLE_FUNC

static const size_t tex_fmt_count = sizeof(tex_fmt_table) / sizeof(tex_func_fmt);

static const tex_func_fmt* get_tex_fmt_by_type(func_type_t type) {
    for (size_t i = 0; i < tex_fmt_count; ++i) {
        if (tex_fmt_table[i].func_type == type) {
            return &tex_fmt_table[i];
        }
    }
    return nullptr;
}

//================================================================================

enum tex_prec_t {
    TEX_PREC_LOWEST = 0,
    TEX_PREC_ADD    = 1,  // +, -
    TEX_PREC_MUL    = 2,  // *, /
    TEX_PREC_POW    = 3,  // ^
    TEX_PREC_ATOM   = 4   // числа, переменные, функции-атомы
};

enum assoc_pos_t {
    ASSOC_ROOT,
    ASSOC_LEFT,
    ASSOC_RIGHT
};

static int get_tex_prec(const tree_node_t* node) {
    if (!node) return TEX_PREC_LOWEST;

    if (node->type != FUNCTION) {
        return TEX_PREC_ATOM;
    }

    #define HANDLE_FUNC(op_code, str_name, impl_func, args_cnt, priority, ...) \
        case op_code:                                                          \
            return priority;     

    switch (node->value.func) {
        #include "copy_past_file"
        default:
            LOGGER_ERROR("Unknown func");
            return TEX_PREC_LOWEST;
    }

    #undef HANDLE_FUNC
}

static bool tex_need_parens(const tree_node_t* node, int parent_prec, assoc_pos_t pos) {
    if (!node) return false;

    int my_prec = get_tex_prec(node);

    if (my_prec < parent_prec)  return true;
    if (my_prec > parent_prec)  return false;
    if (node->type != FUNCTION) return false;

    func_type_t func = node->value.func;

    if ((func == SUB || func == DIV) && pos == ASSOC_RIGHT)
        return true;

    if (func == POW && pos == ASSOC_LEFT)
        return true;

    return false;
}

//================================================================================

static void print_node_tex_const (FILE* tex, const tree_t* tree, const tree_node_t* node);
static void print_node_tex_var   (FILE* tex, const tree_t* tree, const tree_node_t* node);


static void print_node_tex_impl  (FILE* tex, const tree_t* tree, tree_node_t* node,
                                   int parent_prec, assoc_pos_t pos);

static void print_node_tex_leaf      (FILE* tex, const tree_t* tree, const tree_node_t* node);

static void print_node_tex_function  (FILE* tex, const tree_t* tree, tree_node_t* node,
                                       int parent_prec, assoc_pos_t pos);

static void print_node_tex_pattern_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                         const tex_func_fmt* fmt,
                                         int my_prec);


//--------------------------------------------------------------------------------

static void print_node_tex_const(FILE* tex, const tree_t* /*tree*/,
                                  const tree_node_t* node) {
    fprintf(tex, "%.6g", (double)node->value.constant);
}

static void print_node_tex_var(FILE* tex, const tree_t* tree,
                                const tree_node_t* node) {
    if (!tree || !tree->var_stack) {
        fprintf(tex, "v_{%zu}", node->value.var_idx);
        return;
    }

    size_t idx = node->value.var_idx;
    if (idx < tree->var_stack->size) {
        c_string_t name = tree->var_stack->data[idx].str;
        if (name.ptr && name.len > 0) {
            fprintf(tex, "%.*s", (int)name.len, name.ptr);
            return;
        }
    }

    fprintf(tex, "v_{%zu}", idx);
}

static void print_node_tex(FILE* tex, const tree_t* tree, tree_node_t* node) {
    print_node_tex_impl(tex, tree, node, TEX_PREC_LOWEST, ASSOC_ROOT);
}

//--------------------------------------------------------------------------------

static void print_node_tex_leaf(FILE* tex, const tree_t* tree,
                                 const tree_node_t* node) {
    if (!node) {
        fprintf(tex, "\\varnothing");
        return;
    }

    switch (node->type) {
        case CONSTANT:
            print_node_tex_const(tex, tree, node);
            break;

        case VARIABLE:
            print_node_tex_var(tex, tree, node);
            break;

        default:
            LOGGER_ERROR("print_node_tex_leaf: unexpected node type %d", node->type);
            fprintf(tex, "??");
            break;
    }
}

//--------------------------------------------------------------------------------

static void print_node_tex_pattern_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                         const tex_func_fmt* fmt, int my_prec) {
    const char* pattern = fmt->pattern ? fmt->pattern : "";

    int child_prec = my_prec;
    if (get_tex_prec(node) == TEX_PREC_ATOM) {
        child_prec = TEX_PREC_LOWEST;
    }

    for (const char* p = pattern; *p != '\0'; ++p) {
        if (*p == '%' && (p[1] == 'a' || p[1] == 'b')) {
            bool is_a = (p[1] == 'a');

            tree_node_t* child = is_a ? node->left : node->right;
            assoc_pos_t  pos   = is_a ? ASSOC_LEFT : ASSOC_RIGHT;

            print_node_tex_impl(tex, tree, child, child_prec, pos);

            ++p; 
        } else {
            fputc(*p, tex);
        }
    }
}


//--------------------------------------------------------------------------------

static void print_node_tex_function(FILE* tex, const tree_t* tree,tree_node_t* node,
                                     int parent_prec, assoc_pos_t pos) {
    int my_prec = get_tex_prec(node);
    bool need_paren = tex_need_parens(node, parent_prec, pos);

    if (need_paren) fputc('(', tex);

    const tex_func_fmt* fmt = get_tex_fmt_by_type(node->value.func);

    if (fmt) {
        print_node_tex_pattern_impl(tex, tree, node, fmt, my_prec);
    } else {
        const char* name = get_func_name_by_type(node->value.func);
        if (!name) name = "f";

        fprintf(tex, "\\operatorname{%s}", name);

        if (node->left || node->right) {
            fputc('(', tex);
            if (node->left) {
                print_node_tex_impl(tex, tree, node->left,
                                     TEX_PREC_LOWEST, ASSOC_LEFT);
                if (node->right) {
                    fprintf(tex, ", ");
                    print_node_tex_impl(tex, tree, node->right,
                                         TEX_PREC_LOWEST, ASSOC_RIGHT);
                }
            }
            fputc(')', tex);
        }
    }

    if (need_paren) fputc(')', tex);
}

//--------------------------------------------------------------------------------

static void print_node_tex_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                 int parent_prec, assoc_pos_t pos) {
    if (!node) {
        fprintf(tex, "\\varnothing");
        return;
    }

    if (node->type == CONSTANT || node->type == VARIABLE) {
        print_node_tex_leaf(tex, tree, node);
        return;
    }

    print_node_tex_function(tex, tree, node, parent_prec, pos);
}

//================================================================================

void print_tex_header(FILE* tex) {
    HARD_ASSERT(tex != nullptr, "tex is nullptr");

    fprintf(tex,
            "\\documentclass[a4paper,12pt]{article}\n"
            "\\usepackage[T2A]{fontenc}\n"
            "\\usepackage[utf8]{inputenc}\n"
            "\\usepackage[russian]{babel}\n"
            "\\usepackage{amsmath}\n"
            "\\usepackage{amssymb}\n"
            "\\usepackage{autobreak}\n"
            "\\usepackage{hyperref}\n"
            "\\begin{document}\n"
            "\\title{Деградирование}\n"
            "\\author{}\n"
            "\\date{}\n"
            "\\maketitle\n"
            "\\tableofcontents\n"
            "\\newpage\n");
    fflush(tex);
}

void print_tex_footer(FILE* tex) {
    HARD_ASSERT(tex != nullptr, "tex is nullptr");

    fprintf(tex,
            "\\end{document}\n");
    fflush(tex);
}

error_code tree_print_tex_expr(const tree_t* tree, tree_node_t*  node,
                               const char*   fmt, ...){
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");

    LOGGER_DEBUG("tree_print_tex_expr: started");

    error_code error = ERROR_NO;

    if (tree->tex_file == nullptr || *tree->tex_file == nullptr) {
        LOGGER_WARNING("tree_print_tex_expr: tex is nullptr");
        return ERROR_NO;
    }
    FILE* tex = *tree->tex_file;
    
    if (fprintf(tex, EXPR_HEAD) < 0) {
        LOGGER_ERROR("tree_print_tex_expr: fprintf begin align* failed");
        error |= ERROR_OPEN_FILE;
    }
         
    if (fmt && *fmt) {
        va_list args = {};
        va_start(args, fmt);
        vfprintf(tex, fmt, args);
        va_end(args);
    }
     
    print_node_tex(tex, tree, node);
     
    if (fprintf(tex, EXPR_TAIL) < 0) {
        LOGGER_ERROR("tree_print_tex_expr: fprintf end align* failed");
        error |= ERROR_OPEN_FILE;
    }
     
    fflush(tex);

    return error;
}

error_code print_merge_tex_expr(tree_t* left_tree,  tree_node_t* left_node, 
                                tree_t* right_tree, tree_node_t* right_node,
                                const char* pattern) {
    HARD_ASSERT(left_tree != nullptr, "Left_tree is nullptr");
    HARD_ASSERT(right_tree != nullptr, "Right_tree is nullptr");
    HARD_ASSERT(left_node != nullptr, "Left_node is nullptr");
    HARD_ASSERT(right_node != nullptr, "Right_node is nullptr");

    error_code error = ERROR_NO;

    if (left_tree->tex_file == nullptr || *left_tree->tex_file == nullptr) {
        LOGGER_WARNING("print_merge_tex_expr: tex is nullptr");
        return ERROR_NO;
    }
    FILE* tex = *left_tree->tex_file;

    if (fprintf(tex, EXPR_HEAD) < 0) {
        LOGGER_ERROR("print_merge_tex_expr: fprintf begin aligned failed");
        error |= ERROR_OPEN_FILE;
    }

    for (const char* p = pattern; *p != '\0'; ++p) {
        if (*p == '%' && (p[1] == 'a' || p[1] == 'b')) {
            bool is_a = (p[1] == 'a');
            if (is_a) {
                print_node_tex(tex, left_tree, left_node);
            } else {
                print_node_tex(tex, right_tree, right_node);
            }
            ++p; 
        } else {
            fputc(*p, tex);
        }
    }

    if (fprintf(tex, EXPR_TAIL) < 0) {
        LOGGER_ERROR("print_merge_tex_expr: fprintf end aligned failed");
        error |= ERROR_OPEN_FILE;
    }
    fflush(tex);

    return error;

}

error_code print_diff_step(const tree_t* tree, tree_node_t* node, const char* pattern){
    HARD_ASSERT(tree  != nullptr, "tree is nullptr");
    HARD_ASSERT(node  != nullptr, "node is nullptr");
    HARD_ASSERT(pattern != nullptr, "pattern is nullptr");

    if (tree->tex_file == nullptr || *tree->tex_file == nullptr) {
        LOGGER_WARNING("print_diff_step: tex is nullptr");
        return ERROR_NO;
    }

    FILE* tex = *tree->tex_file;
    error_code error = ERROR_NO;

    if (fprintf(tex, EXPR_HEAD) < 0) {
        LOGGER_ERROR("print_diff_step: begin failed");
        error |= ERROR_OPEN_FILE;
    }
    fprintf(tex, DIFFERENTIAL "(");
    print_node_tex(tex, tree, node);
    fprintf(tex, ") = ");
    for (const char* p = pattern; *p; ++p) {
        if (*p != '%') {
            fputc(*p, tex);
            continue;
        }

        ++p;
        tree_node_t* sub = nullptr;

        switch (*p) {
            case 'd': fprintf(tex, DIFFERENTIAL);    break;
            case 'p': sub = node;                    break;
            case 'l': sub = node->left;              break;
            case 'r': sub = node->right;             break;
            default:
                fputc('%', tex);
                fputc(*p, tex);
                continue;
        }

        if (sub) {
            print_node_tex(tex, tree, sub);
        }
    }

    if (fprintf(tex, EXPR_TAIL) < 0) {
        LOGGER_ERROR("print_diff_step: end failed");
        error |= ERROR_OPEN_FILE;
    }

    fflush(tex);
    return error;
}
