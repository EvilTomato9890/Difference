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
#include "stack.h"
#include "my_string.h"
#include "file_operations.h"
#include "tex_io.h"
//================================================================================

const int MAX_CONST_LEN = 64;

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

    #define HANDLE_FUNC(op_code, priority, ...) \
        case op_code:                                                          \
            return priority;     

    switch (node->value.func) {
        #include "copy_past_file_tex"
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

    if ((func == DIV) && pos == ASSOC_RIGHT)
        return true;

    if (func == POW && pos == ASSOC_LEFT)
        return true;

    return false;
}

static const char* get_tex_fmt(func_type_t func_type) {
    #define HANDLE_FUNC(op_code, str_name, fmt, ...) \
        if (func_type == op_code) {                                     \
            return fmt;                                                \
        }

    #include "copy_past_file_tex"

    #undef HANDLE_FUNC

    return "%a ? %b";
}

static const char* get_tex_fmt_diff(func_type_t func_type) {
    #define HANDLE_FUNC(op_code, str_name, fmt, latex_len, diff_fmt, ...) \
        if (func_type == op_code) {                                     \
            return diff_fmt;                                                \
        }

    #include "copy_past_file_tex"

    #undef HANDLE_FUNC
    
    return "%a ? %b";
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
                                         const char* fmt,
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

static void print_node_tex_pattern_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                         const char* fmt, int my_prec) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");

    int child_prec = my_prec;
    if (get_tex_prec(node) == TEX_PREC_ATOM) {
        child_prec = TEX_PREC_LOWEST;
    }

    for (const char* p = fmt; *p != '\0'; ++p) {
        if (*p == '%' && (p[1] == 'a' || p[1] == 'b')) {
            bool is_a = (p[1] == 'a');

            tree_node_t* child = is_a ? node->left : node->right;
            assoc_pos_t  pos   = is_a ? ASSOC_LEFT : ASSOC_RIGHT;

            int child_parent_prec = child_prec;

            if ((node->value.func == SUB || node->value.func == DIV) && pos == ASSOC_RIGHT)
                child_parent_prec = child_prec + 1;
            
            print_node_tex_impl(tex, tree, child, child_parent_prec, pos);

            ++p; 
        } else {
            fputc(*p, tex);
        }
    }
}

static void print_node_tex_function(FILE* tex, const tree_t* tree, tree_node_t* node,
                                     int parent_prec, assoc_pos_t pos) {
    HARD_ASSERT(node != nullptr, "node is nullptr");
    HARD_ASSERT(node->type == FUNCTION, "node is not FUNCTION");
    HARD_ASSERT(tree  != nullptr, "tex is nullptr");

    int my_prec = get_tex_prec(node);
    bool need_paren = tex_need_parens(node, parent_prec, pos);

    if (need_paren) fputc('(', tex);

    const char* fmt = get_tex_fmt(node->value.func);

    print_node_tex_pattern_impl(tex, tree, node, fmt, my_prec);

    if (need_paren) fputc(')', tex);
}

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
        "\\setcounter{secnumdepth}{0}\n"
        "\\begin{document}\n"
        "\\title{Сокращение рода боблина}\n"
        "\\maketitle\n"
        "\\newpage\n");
    print_tex_introduction(tex);
    fprintf(tex, 
        "\\tableofcontents\n"
        "\\newpage\n"
    );
    fflush(tex);
}

void print_tex_footer(FILE* tex) {
    HARD_ASSERT(tex != nullptr, "tex is nullptr");

    fprintf(tex,
            "\\end{document}\n");
    fflush(tex);
}
    
void print_tex_H2(FILE* tex, const char* fmt, ...) {
    if(!tex) return ;
    if (fmt && *fmt) {
        va_list args;
        va_start(args, fmt);

        fprintf(tex, "\\subsection{");  
        vfprintf(tex, fmt, args);     
        fprintf(tex, "}\n");          

        va_end(args);
    }
}

void print_tex_H1(FILE* tex, const char* fmt, ...) {
    if(!tex) return ;
    if (fmt && *fmt) {
        va_list args;
        va_start(args, fmt);

        fprintf(tex, "\\section{");  
        vfprintf(tex, fmt, args);     
        fprintf(tex, "}\n");          

        va_end(args);
    }
}

void print_tex_P(FILE* tex,   const char* fmt, ...) {
    if(!tex) return ;
    if (fmt && *fmt) {
        va_list args;
        va_start(args, fmt);

        fprintf(tex, "\\begin{center}\n");  
        vfprintf(tex, fmt, args);     
        fprintf(tex, "\n\\end{center}\n");          

        va_end(args);
    }
}

void print_tex_delimeter(FILE* tex) {
    if(!tex) return ;
    fprintf(tex, "\\noindent\\hrulefill");  
}
//================================================================================

void print_tex_introduction(FILE* tex) {
    HARD_ASSERT(tex != nullptr, "tex is nullptr");
    fprintf(tex, 
        "\\begin{titlepage}\n"
        "    \\centering\n"
        "\n"
        "    {\\Large Уставший волшебник}\\\\[1cm]\n"
        "\n"
        "    {\\huge\\bfseries «Завершение рода Боблина»}\\\\[0.5cm]\n"
        "\n"
        "    \\raggedright\n"
        "\n"
        "    \\textbf{Предыстория} \\\\[0.3cm]\n"
        "\n"
        "    Жил-был самый обычный гоблин по имени Боблин и его очень большая семья. \\\\ \n"
        "    Как-то раз, одним жарким летом они все вместе решили отправиться на пикник.\\\\ \n"
        "    Они нашли великолепную полянку посреди болота: солнышко, зеленая трава, тенеко от непонятно башни, одним словом - благодать. \\\\ \n"
        "    Шел 5-ый час гоблинской пъянки, тут уже нервы волшебника живущего в башне не выдержали. \\\\ \n"
        "    и он решил обрушиить свой праведный гнев не семейство Боблина, истребив некоторую его часть. \\\\ "
        "\n"
        "    \\vspace{0.8cm}\n"
        "    \\textbf{Боевой журнал}\\\\[0.3cm]\n"
        "\n"
        "    В башне стоял особенный артефакт, который записывал ход сражения в виде странного набора символов.\\\\ \n"
        "    Которые лишь сам маг был способен понять, здесь и будет приведет этот боевой журнал. \\\\ "
        "    \\vfill\n"
        "\n"
        "    \\raggedleft\n"
        "    \\textit{«Если на странице стало больше знаков — значит, кто-то из клана Боблина опять что-то натворил.»}\\\\[0.3cm]\n"
        "\n"
        "\\end{titlepage}\n"

    );
}

//================================================================================

struct comment_t {
    const char* const* arr;
    size_t             size;
    size_t             curr_idx;
};

//--------------------------------------------------------------------------------

static const char* get_next_comment(comment_t* comments) {
    if (!comments || !comments->arr || comments->size == 0) return nullptr;

    if (comments->curr_idx >= comments->size)
        comments->curr_idx = 0;

    return comments->arr[comments->curr_idx++];
}

static const char* const comments_arr_zero[] = {
    "Ваше заклинание дезинтегрировало брата Боблина",
    "Ваше колдовство низвело сестру Боблина до атомов",
    "Вы разложили дядю Боблина на молекулы",
    "Ваше волшебство оказалось не по зубам тёте Боблина, кстати, куда она делась?",
    "Огненный шар испарил бабушку Боблина",
    "Заклинание хаоса раскидало части племянника Боблина по разным планам",
    "Ваш портал небытия вежливо удалил тещу Боблина из этого измерения"
};

static comment_t comments_zero = {
    comments_arr_zero,
    sizeof(comments_arr_zero) / sizeof(comments_arr_zero[0]),
    0
};

static const char* const comments_arr_one[] = {
    "Полиморф сработал отлично: зять Боблина теперь лягушка",
    "Поздравляю! От свояка Боблина осталась только полоыина",
    "Ваше заклинание свернуло невестку Боблина в шарик",
    "ААХХАХААХАХ Гоблин-Боблин",
    "Ваше вошшебство откатило деверя Боблина до младенчества"
};

static comment_t comments_one = {
    comments_arr_one,
    sizeof(comments_arr_one) / sizeof(comments_arr_one[0]),
    0
};

static const char* const comments_arr_basic[] = {
    "Битва продолжается, не теряйте духу, они когда-то, наверное, закончаться!",
    "Их не становиться меньше, откуда они только лезут?!",
    "Вот так и рождаются легенды о герое, истребившем половину рода Боблина.",
    "Небольшой взмах посохом — и план сражения выглядит куда приличнее.",
    "Один из гоблинов упал",
    "Родственники Боблина продолжают лезть к вам, держите посох крепче!"
};

static comment_t comments_basic = {
    comments_arr_basic,
    sizeof(comments_arr_basic) / sizeof(comments_arr_basic[0]),
    0
};

//--------------------------------------------------------------------------------

#define PRINT_DIFF_COMMENT_TEMPLATE(arr_name)               \
    const char* comment = get_next_comment(&comments_##arr_name); \
    if (comment) print_tex_P(tex, comment);                 


void print_tex_const_diff_comment(FILE* tex) {
    PRINT_DIFF_COMMENT_TEMPLATE(zero); 
}

void print_tex_var_diff_comment(FILE* tex) {
    PRINT_DIFF_COMMENT_TEMPLATE(one);  
}

void print_tex_basic_diff_comment(FILE* tex) {
    PRINT_DIFF_COMMENT_TEMPLATE(basic); 
}

#undef PRINT_DIFF_COMMENT_TEMPLATE

//================================================================================

error_code print_tex_expr(const tree_t* tree, tree_node_t*  node, const char* fmt, ...){
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");

    LOGGER_DEBUG("print_tex_expr: started");

    error_code error = ERROR_NO;

    if (tree->tex_file == nullptr || *tree->tex_file == nullptr) {
        LOGGER_WARNING("print_tex_expr: tex is nullptr");
        return ERROR_NO;
    }
    FILE* tex = *tree->tex_file;

    if (fmt && *fmt) {
        va_list args = {};
        va_start(args, fmt);
        vfprintf(tex, fmt, args);
        va_end(args);
    }

    if (fprintf(tex, EXPR_HEAD) < 0) {
        LOGGER_ERROR("print_tex_expr: fprintf begin align* failed");
        error |= ERROR_OPEN_FILE;
    }
    print_node_tex(tex, tree, node);
     
    if (fprintf(tex, EXPR_TAIL) < 0) {
        LOGGER_ERROR("print_tex_expr: fprintf end align* failed");
        error |= ERROR_OPEN_FILE;
    }
     
    fflush(tex);

    return error;
}

error_code print_diff_step(const tree_t* tree, tree_node_t* node, const char* pattern) {
    HARD_ASSERT(tree  != nullptr, "tree is nullptr");
    HARD_ASSERT(node  != nullptr, "node is nullptr");

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

error_code print_diff_step_tex_fmt(const tree_t* tree, tree_node_t* node) {
    const char* pattern = get_tex_fmt_diff(node->value.func);
    return print_diff_step(tree, node, pattern);
}
//================================================================================

static size_t get_double_len(double target) {
    char buf[MAX_CONST_LEN] = {};
    int n = snprintf(buf, sizeof(buf), "%.2f", target);
    return (n > 0 ? (size_t)n : 0);
}
/*
ssize_t get_tex_len(const tree_t* tree, const tree_node_t* node) {
    HARD_ASSERT(tree != nullptr, "Tree is nullptr");

    error_code error = 0;

    if(node->type == CONSTANT) return get_double_len(node->value.constant);
    if(node->type == VARIABLE) return strlen(get_var_name(tree, node).ptr);
    if(node->type != FUNCTION) {LOGGER_ERROR("Unknown type"); return -1;}

    #define HANDLE_FUNC(op_code, str_name, func, args_cnt, priority, latex_fmt, ...) \
        case op_code:
            return strlen(#latex_fmt) - args_cnt * 2;
    switch(node->value.func) {
        #include "copy_past_file"
        default:
            LOGGER_ERROR("Unknown function");
            return -1;
    }

}*/