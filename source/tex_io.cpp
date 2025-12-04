#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>

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

static const size_t TEX_SQUASH_MAX_VARS        = 26; 
static const size_t TEX_SQUASH_MAX_TOTAL_LEN   = 70; 
static const size_t TEX_SQUASH_MAX_SUBEXPR_LEN = 70;  
static const size_t TEX_SQUASH_MIN_SUBEXPR_LEN = 1;  
static const size_t MAX_CONST_LEN              = 64;
static const size_t COST_LETTER                = 1; 
static const double TEX_LEN_REDUCTION          = 2.5;

//================================================================================

struct tex_squash_candidate_t {
    tree_node_t* node;
    size_t       tex_len;
};

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

//================================================================================

static const char* get_pow_diff_pattern(const tree_node_t* node) {
    const tree_node_t* l = node->left;
    const tree_node_t* r = node->right;

    if (is_subree_const(r)) {
        return "%r \\cdot {%l}^{%r - 1} \\cdot %d(%l)";
    }
    if (is_subree_const(l)) {
        return "%p \\cdot \\ln(%l) \\cdot %d(%r)";
    }
    return "%p \\cdot ( %d(%r) \\cdot \\ln(%l) + %r \\cdot %d(%l) / %l )";
}

static const char* get_log_diff_pattern(const tree_node_t* node) {
    const tree_node_t* l = node->left;
    const tree_node_t* r = node->right;

    if (r == nullptr && l != nullptr) {
        return "%d(%l) / %l";
    }
    if (is_subree_const(l)) {
        return "%d(%r) / (%r \\cdot \\ln(%l))";
    }
    if (is_subree_const(r)) {
        return "-\\ln(%r) \\cdot %d(%l) / (%l \\cdot {\\ln(%l)}^{2})";
    }
    return " %d(%r) / %r - %d(%l) / %l ) / \\ln(%l)";
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
                                   int parent_prec, assoc_pos_t pos, bool ignore_this_flag);

static void print_node_tex_leaf      (FILE* tex, const tree_t* tree, const tree_node_t* node);

static void print_node_tex_function  (FILE* tex, const tree_t* tree, tree_node_t* node,
                                       int parent_prec, assoc_pos_t pos);

static void print_node_tex_pattern_impl(FILE* tex, const tree_t* tree, tree_node_t* node,
                                         const char* fmt,
                                         int my_prec);

static void tex_add_candidate(tree_t* tree, tree_node_t* node);

static void tex_collect_candidates(tree_t* tree, tree_node_t* node);

static error_code print_tex_squashes(const tree_t* tree);

static void tex_try_take_candidate(tree_t* tree, tree_node_t* node,
                                   size_t* used, size_t* curr_len);

static error_code tex_prepare_squash(tree_t* tree, tree_node_t* root);

static error_code tex_prepare_squash_ex(tree_t* tree, tree_node_t* root, size_t min_total_len);

//--------------------------------------------------------------------------------

static void print_node_tex_const(FILE* tex, const tree_t* tree,
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
    print_node_tex_impl(tex, tree, node, TEX_PREC_LOWEST, ASSOC_ROOT, false);
}

static void print_node_tex_full(FILE* tex, const tree_t* tree, tree_node_t* node) {
    print_node_tex_impl(tex, tree, node, TEX_PREC_LOWEST, ASSOC_ROOT, true);
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
            
            print_node_tex_impl(tex, tree, child, child_parent_prec, pos, false);

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
                                int parent_prec, assoc_pos_t pos,
                                bool ignore_this_flag) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");

    if (!node) {
        fprintf(tex, "\\varnothing");
        return;
    }

    ON_TEX_SQUASH(
    if (!ignore_this_flag && node->squash_id >= 0  && tree->squash_bindings && //TODO: FIXME
        (size_t)node->squash_id < tree->squash_count) {

        char letter = tree->squash_bindings[node->squash_id].letter;
        fputc(letter, tex);
        return;
    }
    )

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
        "\\usepackage{graphicx}\n"
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

void print_tex_image(FILE* tex) {
    if(!tex) return ;
    fprintf(tex, "\\includegraphics[height=7.5cm]{./graphs/teylor_plot.png}");
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
static error_code print_tex_expr_impl(const tree_t* tree, tree_node_t*  node, bool ignore_root_flag,
                                      const char* fmt, va_list args) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");

    if (tree->tex_file == nullptr || *tree->tex_file == nullptr) {
        LOGGER_WARNING("print_tex_expr_impl: tex is nullptr");
        return ERROR_NO;
    }

    FILE* tex = *tree->tex_file;
    error_code error = ERROR_NO;

    if(fprintf(tex, TEXT_HEAD) < 0) {
        LOGGER_ERROR("print_tex_expr_impl: fprintf begin center failed");
        error |= ERROR_OPEN_FILE;
    }
    if (fmt && *fmt) {
        vfprintf(tex, fmt, args);
    }
    fprintf(tex, EXPR_HEAD);

    if (ignore_root_flag) {
        print_node_tex_full(tex, tree, node);
    } else {
        print_node_tex(tex, tree, node);
    }

    fprintf(tex, EXPR_TAIL TEXT_TAIL);

    fflush(tex);
    return error;
}

error_code print_tex_expr(const tree_t* tree, tree_node_t* node, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    error_code error = print_tex_expr_impl(tree, node, false, fmt, args);
    va_end(args);
    return error;
}

error_code print_tex_expr_full(const tree_t* tree, tree_node_t* node, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    error_code error = print_tex_expr_impl(tree, node, true, fmt, args);
    va_end(args);
    return error;
}

error_code print_diff_step(tree_t* tree, tree_node_t* node, const char* pattern) {
    HARD_ASSERT(tree  != nullptr, "tree is nullptr");
    HARD_ASSERT(node  != nullptr, "node is nullptr");

    if (tree->tex_file == nullptr || *tree->tex_file == nullptr) {
        LOGGER_WARNING("print_diff_step: tex is nullptr");
        return ERROR_NO;
    }
    
    FILE* tex = *tree->tex_file;
    error_code error = ERROR_NO;

    error |= tex_prepare_squash_ex(tree, node, TEX_SQUASH_MAX_TOTAL_LEN / TEX_LEN_REDUCTION);
    RETURN_IF_ERROR(error);

    if (tree->squash_bindings && tree->squash_count > 0) {
        error |= print_tex_squashes(tree);
        RETURN_IF_ERROR(error);
    }

    if (fprintf(tex, TEXT_HEAD EXPR_HEAD) < 0) {
        LOGGER_ERROR("print_diff_step: begin failed");
        error |= ERROR_OPEN_FILE;
    }
    fprintf(tex, DIFFERENTIAL "(");
    print_node_tex(tex, tree, node);
    fprintf(tex, ") = ");
    tex_clear_squash(tree);

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
            //if(sub->subtree_tex_len > TEX_SQUASH_MAX_SUBEXPR_LEN / TEX_LEN_REDUCTION)
            print_node_tex(tex, tree, sub);
        }
    }

    fprintf(tex, EXPR_TAIL TEXT_TAIL);
    
    fflush(tex);
    return error;
}

error_code print_diff_step_tex_fmt(tree_t* tree, tree_node_t* node) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");

    const char* pattern = nullptr;

    if (node->type == FUNCTION) {
        switch (node->value.func) {
            case POW:
                pattern = get_pow_diff_pattern(node);
                break;

            case LOG:
                pattern = get_log_diff_pattern(node);
                break;

            default:
                pattern = get_tex_fmt_diff(node->value.func);
                break;
        }
    } else {
        pattern = get_tex_fmt_diff(node->value.func);
    }

    return print_diff_step(tree, node, pattern);
}

//--------------------------------------------------------------------------------

void tex_clear_squash(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");

    ON_TEX_SQUASH(
    if (tree->squash_bindings && tree->squash_count > 0) {
        for (size_t i = 0; i < tree->squash_count; ++i) {
            tree_node_t* node = tree->squash_bindings[i].expr_subtree;
            if (node) {
                node->squash_id = -1;
            }
        }
        free(tree->squash_bindings);
        tree->squash_bindings = nullptr;
        tree->squash_count    = 0;
    }
    )
}

static void tex_add_candidate(tree_t* tree, tree_node_t* node) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");
    HARD_ASSERT(tree->squash_bindings != nullptr, "tree->squash_bindings is nullptr");

    size_t len   = node->subtree_tex_len;
    size_t count = tree->squash_count;

    if (count < TEX_SQUASH_MAX_VARS) {
        tree->squash_bindings[count].expr_subtree = node;
        tree->squash_bindings[count].letter       = 0;  
        tree->squash_count = count + 1;
        return;
    }

    size_t min_i   = 0;
    size_t min_len = 0;

    if (tree->squash_bindings[0].expr_subtree)
        min_len = tree->squash_bindings[0].expr_subtree->subtree_tex_len;

    for (size_t i = 1; i < count; ++i) {
        tree_node_t* cand = tree->squash_bindings[i].expr_subtree;
        size_t clen = cand ? cand->subtree_tex_len : 0;
        if (clen < min_len) {
            min_len = clen;
            min_i   = i;
        }
    }

    if (len > min_len) {
        tree->squash_bindings[min_i].expr_subtree = node;
        tree->squash_bindings[min_i].letter       = 0;
    }
}

static void tex_collect_candidates(tree_t* tree, tree_node_t* node) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");
    HARD_ASSERT(tree->squash_bindings != nullptr, "tree->squash_bindings is nullptr");

    if (node->squash_id >= 0)
        return;

    size_t len = node->subtree_tex_len;

    if (len > TEX_SQUASH_MAX_SUBEXPR_LEN) {
        tex_collect_candidates(tree, node->left);
        tex_collect_candidates(tree, node->right);
        return;
    }

    if (len < TEX_SQUASH_MIN_SUBEXPR_LEN) return;

    tex_add_candidate(tree, node);
}

static int tex_binding_cmp_desc(const void* a, const void* b) {
    const tex_squash_binding_t* ba = (const tex_squash_binding_t*)a;
    const tex_squash_binding_t* bb = (const tex_squash_binding_t*)b;

    size_t la = 0;
    size_t lb = 0;

    if (ba->expr_subtree)
        la = ba->expr_subtree->subtree_tex_len;
    if (bb->expr_subtree)
        lb = bb->expr_subtree->subtree_tex_len;

    if (la < lb) return 1;   // по убыванию
    if (la > lb) return -1;
    return 0;
}

static error_code print_tex_squashes(const tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(tree->squash_bindings != nullptr, "tree->squash_bindings is nullptr");

    error_code error = ERROR_NO;
    for (size_t i = 0; i < tree->squash_count; ++i) {
        char         letter = tree->squash_bindings[i].letter;
        tree_node_t* sub    = tree->squash_bindings[i].expr_subtree;
        error |= print_tex_expr_full(tree, sub, "%c = ", letter);
    }   
    return error;
}

static void tex_try_take_candidate(tree_t* tree, tree_node_t* node,
                                   size_t* used, size_t* curr_len) {
    HARD_ASSERT(tree     != nullptr, "tree is nullptr");
    HARD_ASSERT(used     != nullptr, "used is nullptr");
    HARD_ASSERT(curr_len != nullptr, "curr_len is nullptr");

    if (!node) return;

    if (node->squash_id >= 0)return;

    size_t len = node->subtree_tex_len;

    if (len <= COST_LETTER) return;

    size_t idx = *used;

    node->squash_id = (ssize_t)idx;

    tree->squash_bindings[idx].letter       = (char)('A' + (char)idx);
    tree->squash_bindings[idx].expr_subtree = node;

    *curr_len -= (len - COST_LETTER);
    ++(*used);
}

static error_code tex_prepare_squash_ex(tree_t* tree, tree_node_t* root, size_t min_total_len) {
    if (!tree || !root) return ERROR_NO;

    tex_clear_squash(tree);

    ssize_t total_len_signed = get_tex_len(tree, root);
    if (total_len_signed < 0) {
        LOGGER_ERROR("tex_prepare_squash: get_tex_len(root) failed");
        return ERROR_UNKNOWN_FUNC;
    }

    size_t total_len = (size_t)total_len_signed;

    if (total_len <= min_total_len) {
        return ERROR_NO;
    }
    tree->squash_bindings = (tex_squash_binding_t*)calloc(TEX_SQUASH_MAX_VARS, sizeof(tex_squash_binding_t));
    if (!tree->squash_bindings) {
        LOGGER_ERROR("tex_prepare_squash: calloc squash_bindings failed");
        return ERROR_MEM_ALLOC;
    }
    tree->squash_count = 0;

    tex_collect_candidates(tree, root);

    if (tree->squash_count == 0) {
        free(tree->squash_bindings);
        tree->squash_bindings = nullptr;
        return ERROR_NO;
    }

    qsort(tree->squash_bindings,
          tree->squash_count,
          sizeof(tex_squash_binding_t),
          tex_binding_cmp_desc);

    size_t curr_len      = total_len;
    size_t used          = 0;

    for (size_t i = 0;
         i < tree->squash_count && curr_len > min_total_len && used < TEX_SQUASH_MAX_VARS;
         ++i) {

        tree_node_t* node = tree->squash_bindings[i].expr_subtree;
        tex_try_take_candidate(tree, node, &used, &curr_len);
    }

    if (used == 0) {
        tex_clear_squash(tree);
        return ERROR_NO;
    }
    tree->squash_count = used;
    return ERROR_NO;
}

static error_code tex_prepare_squash(tree_t* tree, tree_node_t* root) {
    return tex_prepare_squash_ex(tree, root, TEX_SQUASH_MAX_TOTAL_LEN);
}

error_code print_tex_expr_with_squashes(tree_t* tree, tree_node_t* node, const char* fmt, ...) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(node != nullptr, "node is nullptr");

    error_code error = ERROR_NO;

    LOGGER_DEBUG("print_tex_expr_with_squashes: preparing squashes");

    error |= tex_prepare_squash(tree, node);
    RETURN_IF_ERROR(error);

    if (tree->squash_bindings && tree->squash_count > 0) {
        error |= print_tex_squashes(tree);
        RETURN_IF_ERROR(error);
    }

    va_list args;
    va_start(args, fmt);
    error |= print_tex_expr_impl(tree, node, false, fmt, args);
    va_end(args);
    RETURN_IF_ERROR(error);

    tex_clear_squash(tree);
    return error;
}

//================================================================================

static ssize_t max_ss(ssize_t a, ssize_t b) {
    return (a > b) ? a : b;
}

static size_t get_double_len(double target) {
    char buf[MAX_CONST_LEN] = {};
    int n = snprintf(buf, sizeof(buf), "%.2f", target);
    return (n > 0) ? (size_t)n : 0;
}

ssize_t get_tex_len(const tree_t* tree, tree_node_t* node) {
    HARD_ASSERT(tree != nullptr, "Tree is nullptr");
    HARD_ASSERT(node != nullptr, "Node is nullptr");

    ON_TEX_SQUASH(
        node->subtree_tex_len = 0;
        node->squash_id       = -1;
    )

    if (node->type == CONSTANT) {
        size_t len = get_double_len(node->value.constant);
        ON_TEX_SQUASH(
        node->subtree_tex_len = len;
        )
        return (ssize_t)len;
    }

    if (node->type == VARIABLE) {
        c_string_t name = get_var_name(tree, node);
        size_t len = 0;
        if (name.ptr && name.len > 0) len = name.len; 
        else                          len = 1; 
        
        ON_TEX_SQUASH(
        node->subtree_tex_len = len;
        )
        return (ssize_t)len;
    }

    if (node->type != FUNCTION) {
        LOGGER_ERROR("Unknown type");
        return -1;
    }

    ssize_t a = 0;
    ssize_t b = 0;

    if (node->left) {
        a = get_tex_len(tree, node->left);
        if (a < 0) return -1;
    }
    if (node->right) {
        b = get_tex_len(tree, node->right);
        if (b < 0) return -1;
    }

    ssize_t res = 0;

    switch (node->value.func) {
    #define HANDLE_FUNC(op_code, str_name, priority, latex_len_code, ...) \
        case op_code: {                                              \
            res = (latex_len_code);                                  \
            break;                                                   \
        }

        #include "copy_past_file_tex"

    #undef HANDLE_FUNC

    default:
        LOGGER_ERROR("Unknown function");
        return -1;
    }

    ON_TEX_SQUASH(
        node->subtree_tex_len = (res > 0) ? (size_t)res : 0;
    )

    return res;
}
