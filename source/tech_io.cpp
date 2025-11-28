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

//================================================================================

struct func_struct {
    func_type_t func_type;
    const char* func_name;   
};

#define HANDLE_FUNC(op_code, str_name, tex_pattern) \
    { op_code, #str_name },

static func_struct op_codes[] = {
    #include "copy_past_file"
};

#undef HANDLE_FUNC

const int op_codes_num = sizeof(op_codes) / sizeof(func_struct);

//--------------------------------------------------------------------------------

static func_type_t get_op_code(string_t func_str, error_code* error) {
    HARD_ASSERT(func_str.ptr != nullptr, "func_name nullptr");
    HARD_ASSERT(error       != nullptr, "error nullptr");

    for (size_t i = 0; i < (size_t)op_codes_num; i++) {
        if (my_scstrcmp(func_str, op_codes[i].func_name) == 0)
            return op_codes[i].func_type;
    }

    LOGGER_ERROR("get_op_code: func not found: '%.*s'",
                 (int)func_str.len, func_str.ptr);
    *error |= ERROR_UNKNOWN_FUNC;
    return ADD;
}

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

#define HANDLE_FUNC(op_code, symbol, pattern) \
    { op_code, #symbol, pattern },

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

    switch (node->value.func) {
        case ADD:
        case SUB:
            return TEX_PREC_ADD;

        case MUL:
        case DIV:
            return TEX_PREC_MUL;

        case POW:
        case LOG:
            return TEX_PREC_POW;

        default:
            return TEX_PREC_ATOM; 
    }
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

static void print_node_tech_const (FILE* tex, const tree_t* tree, const tree_node_t* node);
static void print_node_tech_var   (FILE* tex, const tree_t* tree, const tree_node_t* node);


static void print_node_tech_impl  (FILE* tex, const tree_t* tree, tree_node_t* node,
                                   int parent_prec, assoc_pos_t pos);

static void print_node_tech_leaf      (FILE* tex, const tree_t* tree, const tree_node_t* node);

static void print_node_tech_function  (FILE* tex, const tree_t* tree, tree_node_t* node,
                                       int parent_prec, assoc_pos_t pos);

static void print_node_tech_pattern_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                         const tex_func_fmt* fmt,
                                         int my_prec);


//--------------------------------------------------------------------------------

static void print_node_tech_const(FILE* tex, const tree_t* /*tree*/,
                                  const tree_node_t* node)
{
    fprintf(tex, "%.6g", (double)node->value.constant);
}

//--------------------------------------------------------------------------------

static void print_node_tech_var(FILE* tex, const tree_t* tree,
                                const tree_node_t* node)
{
    if (!tree || !tree->var_stack) {
        fprintf(tex, "v_{%zu}", node->value.var_idx);
        return;
    }

    size_t idx = node->value.var_idx;
    if (idx < tree->var_stack->size) {
        string_t name = tree->var_stack->data[idx].str;
        if (name.ptr && name.len > 0) {
            fprintf(tex, "%.*s", (int)name.len, name.ptr);
            return;
        }
    }

    fprintf(tex, "v_{%zu}", idx);
}

//--------------------------------------------------------------------------------

static void print_node_tech(FILE* tex, const tree_t* tree, tree_node_t* node) {
    print_node_tech_impl(tex, tree, node, TEX_PREC_LOWEST, ASSOC_ROOT);
}

//--------------------------------------------------------------------------------

static void print_node_tech_leaf(FILE* tex, const tree_t* tree,
                                 const tree_node_t* node) {
    if (!node) {
        fprintf(tex, "\\varnothing");
        return;
    }

    switch (node->type) {
        case CONSTANT:
            print_node_tech_const(tex, tree, node);
            break;

        case VARIABLE:
            print_node_tech_var(tex, tree, node);
            break;

        default:
            LOGGER_ERROR("print_node_tech_leaf: unexpected node type %d", node->type);
            fprintf(tex, "??");
            break;
    }
}

//--------------------------------------------------------------------------------

static void print_node_tech_pattern_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                         const tex_func_fmt* fmt, int my_prec) {
    const char* pattern = fmt->pattern ? fmt->pattern : "";

    for (const char* p = pattern; *p != '\0'; ++p) {
        if (*p == '%' && (p[1] == 'a' || p[1] == 'b')) {
            bool is_a = (p[1] == 'a');

            tree_node_t* child = is_a ? node->left : node->right;
            assoc_pos_t  pos   = is_a ? ASSOC_LEFT : ASSOC_RIGHT;

            print_node_tech_impl(tex, tree, child, my_prec, pos);

            ++p; 
        } else {
            fputc(*p, tex);
        }
    }
}


//--------------------------------------------------------------------------------

static void print_node_tech_function(FILE* tex, const tree_t* tree,tree_node_t* node,
                                     int parent_prec, assoc_pos_t pos) {
    int my_prec = get_tex_prec(node);
    bool need_paren = tex_need_parens(node, parent_prec, pos);

    if (need_paren) fputc('(', tex);

    const tex_func_fmt* fmt = get_tex_fmt_by_type(node->value.func);

    if (fmt) {
        print_node_tech_pattern_impl(tex, tree, node, fmt, my_prec);
    } else {
        const char* name = get_func_name_by_type(node->value.func);
        if (!name) name = "f";

        fprintf(tex, "\\operatorname{%s}", name);

        if (node->left || node->right) {
            fputc('(', tex);
            if (node->left) {
                print_node_tech_impl(tex, tree, node->left,
                                     TEX_PREC_LOWEST, ASSOC_LEFT);
                if (node->right) {
                    fprintf(tex, ", ");
                    print_node_tech_impl(tex, tree, node->right,
                                         TEX_PREC_LOWEST, ASSOC_RIGHT);
                }
            }
            fputc(')', tex);
        }
    }

    if (need_paren) fputc(')', tex);
}

//--------------------------------------------------------------------------------

static void print_node_tech_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                 int parent_prec, assoc_pos_t pos) {
    if (!node) {
        fprintf(tex, "\\varnothing");
        return;
    }

    if (node->type == CONSTANT || node->type == VARIABLE) {
        print_node_tech_leaf(tex, tree, node);
        return;
    }

    print_node_tech_function(tex, tree, node, parent_prec, pos);
}

//================================================================================

void print_tech_header(FILE* tex) {
    HARD_ASSERT(tex != nullptr, "tex is nullptr");

    fprintf(tex,
                "\\documentclass[a4paper,12pt]{article}\n"
                "\\usepackage{amsmath}\n"
                "\\usepackage{autobreak}\n"
                "\\allowdisplaybreaks\n"
                "\n"
                "\\begin{document}\n"
                "\\begin{align}\n"
                "\\begin{autobreak}\n");
    fflush(tex);
}

void print_tech_footer(FILE* tex) {
    HARD_ASSERT(tex != nullptr, "tex is nullptr");

    fprintf(tex,
                "\\end{autobreak}\n"
                "\\end{align}\n"
                "\\end{document}\n");
    fflush(tex);
}

error_code tree_print_tech_expr(const tree_t* tree,
                                tree_node_t*  node,
                                const char*   fmt, ...)
{
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");

    error_code error = ERROR_NO;

    ON_DEBUG({
        if (tree->tex_file == nullptr || *tree->tex_file == nullptr) {
            LOGGER_WARNING("tree_print_tech_expr: tex is nullptr");
            return ERROR_NO;
        }

        FILE* tex = *tree->tex_file;

        if (fmt && *fmt) {
            va_list args = {};
            va_start(args, fmt);
            vfprintf(tex, fmt, args);
            va_end(args);
        }

        //fprintf(tex, "\\[");
        print_node_tech(tex, tree, node);
        //fprintf(tex, "\\]");

        if (fprintf(tex, " \\\n") < 0) {
            LOGGER_ERROR("tree_print_tech_expr: fprintf failed");
            error = ERROR_OPEN_FILE;
        }

        fflush(tex);
    });

    return error;
}