#include <ctype.h>
#include <string.h>

#include "tree_info.h"
#include "node_info.h"
#include "tree_operations.h"
#include "input_parser.h"
#include "logger.h"
#include "my_string.h"

//================================================================================

static void skip_spaces(char** str);
static bool try_parse_identifier(char** str, size_t* len_ptr);
static bool get_func_type_by_name(string_t name, func_type_t* func_out);

static tree_node_t* get_equat  (tree_t* tree, char** str);
static tree_node_t* get_term   (tree_t* tree, char** str);
static tree_node_t* get_primary(tree_t* tree, char** str);
static tree_node_t* get_num    (tree_t* tree, char** str);
static tree_node_t* get_var    (tree_t* tree, char** str);
static tree_node_t* get_func   (tree_t* tree, char** str);

//================================================================================

static void skip_spaces(char** str) {
    HARD_ASSERT(str  != nullptr, "str is nullptr");
    HARD_ASSERT(*str != nullptr, "*str is nullptr");

    while (isspace((unsigned char)**str)) {
        ++*str;
    }
}

static bool try_parse_identifier(char** str, size_t* len_ptr) {
    HARD_ASSERT(str     != nullptr, "str is nullptr");
    HARD_ASSERT(*str    != nullptr, "*str is nullptr");
    HARD_ASSERT(len_ptr != nullptr, "len_ptr is nullptr");

    size_t len = 0;
    while (isalpha((unsigned char)**str)) {
        ++*str;
        ++len;
    }
    *len_ptr = len;
    return len > 0;
}

static bool get_func_type_by_name(string_t name, func_type_t* func_out) {
    HARD_ASSERT(name.ptr != nullptr, "func_name is nulltrp");
    HARD_ASSERT(func_out != nullptr, "func_out is nullptr");

    if (my_scstrcmp(name, "sin") == 0) {
        *func_out = SIN;
        return true;
    } else if (my_scstrcmp(name, "cos") == 0) {
        *func_out = COS;
        return true;
    } else if (my_scstrcmp(name, "ln") == 0) {
        *func_out = LN;
        return true;
    } else if (my_scstrcmp(name, "exp") == 0) {
        *func_out = EXP;
        return true;
    }

    return false;
}

//================================================================================

// G -> E '$'
tree_node_t* get_g(tree_t* tree, char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str != nullptr, "str ptr is nullptr");
    HARD_ASSERT(str* != nullptr, "String is nullptr");

    LOGGER_DEBUG("get_g: started");

    skip_spaces(str);
    tree_node_t* node = get_equat(tree, str); 
    if (node == nullptr) { 
        LOGGER_ERROR("get_g: get_equat returned nullptr"); 
        return nullptr; 
    } 

    skip_spaces(str);
    if (**str != '$') { 
        LOGGER_ERROR("expected '$', but got '%c'", **str); 
        return node; 
    } 
    ++*str; 
    return node;
}

// E -> T { ('+' | '-') T }
static tree_node_t* get_equat(tree_t* tree, char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str != nullptr, "str ptr is nullptr");
    HARD_ASSERT(str* != nullptr, "String is nullptr");

    skip_spaces(str);

    tree_node_t* left = get_term(tree, str); 
    if (!left) return nullptr; 

    skip_spaces(str);
    while (**str == '+' || **str == '-') { 
        char op = **str; 
        ++*str; 

        tree_node_t* right = get_term(tree, str); 
        if (!right) { 
            LOGGER_ERROR("get_equat: right operand is nullptr"); 
            return left; 
        } 

        func_type_t func = (op == '+') ? ADD : SUB; 
        left = init_node(FUNCTION, make_union_func(func), left, right); 
        if (!left) { 
            LOGGER_ERROR("get_equat: init_node failed"); 
            return nullptr; 
        }

        skip_spaces(str);
    } 
    return left; 
}

// T -> P { ('*' | '/') P }
static tree_node_t* get_term(tree_t* tree, char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str != nullptr, "str ptr is nullptr");
    HARD_ASSERT(str* != nullptr, "String is nullptr");

    skip_spaces(str);

    tree_node_t* left = get_primary(tree, str); 
    if (!left) return nullptr;

    skip_spaces(str);
    while (**str == '*' || **str == '/') { 
        char op = **str; 
        ++*str; 

        tree_node_t* right = get_primary(tree, str); 
        if (!right) { 
            LOGGER_ERROR("get_term: right operand is nullptr"); 
            return left; 
        } 

        func_type_t func = (op == '*') ? MUL : DIV; 
        left = init_node(FUNCTION, make_union_func(func), left, right); 
        if (!left) { 
            LOGGER_ERROR("get_term: init_node failed"); 
            return nullptr; 
        }

        skip_spaces(str);
    } 
    return left; 
}

// NUM -> [0-9]+
static tree_node_t* get_num(tree_t* tree, char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str != nullptr, "str ptr is nullptr");
    HARD_ASSERT(str* != nullptr, "String is nullptr");

    skip_spaces(str);

    const char* old_s = *str; 
    int val = 0; 

    while ('0' <= **str && **str <= '9') { 
        val = val * 10 + (**str - '0');
        ++*str;
    }

    if (*str == old_s) { 
        LOGGER_ERROR("expected digit, but got '%c'", **str); 
        return nullptr; 
    } 

    return init_node(CONSTANT, make_union_const(val), nullptr, nullptr); 
}

// P -> '(' E ')' | FUNC | VAR | NUM
static tree_node_t* get_primary(tree_t* tree, char** str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str != nullptr, "str ptr is nullptr");
    HARD_ASSERT(str* != nullptr, "String is nullptr");

    skip_spaces(str);

    if (**str == '(') {
        ++*str;
        tree_node_t* node = get_equat(tree, str);
        if (!node) return nullptr;

        skip_spaces(str);
        if (**str != ')') {
            LOGGER_ERROR("expected ')', but got '%c'", **str);
            return node;
        }
        ++*str;
        return node;
    }

    if (isalpha((unsigned char)**str)) {
        char* save = *str;
        tree_node_t* node = get_func(tree, str);
        if (node != nullptr) {
            return node;
        }
        *str = save;
        return get_var(tree, str);
    }

    return get_num(tree, str);
}

// VAR -> [a-zA-Z]+
static tree_node_t* get_var(tree_t* tree, char** str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str != nullptr, "str ptr is nullptr");
    HARD_ASSERT(str* != nullptr, "String is nullptr");

    skip_spaces(str);

    char*  name_buf = *str;
    size_t var_len  = 0;
    if (!try_parse_identifier(str, &var_len)) {
        LOGGER_ERROR("get_var: expected variable, but got '%c'", **str);
        *str = name_buf;
        return nullptr;
    }

    error_code error = 0;
    size_t var_idx = get_or_add_var_idx((string_t){name_buf, var_len}, 0, tree->var_stack, &error);
    if (error != 0) {
        LOGGER_ERROR("get_var: failed to get var_idx");
        *str = name_buf;
        return nullptr;
    }

    return init_node(VARIABLE, make_union_var(var_idx), nullptr, nullptr);
}

// FUNC -> ( "sin" | "cos" | "ln" | "exp" ) '(' E ')'
static tree_node_t* get_func(tree_t* tree, char** str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str != nullptr, "str ptr is nullptr");
    HARD_ASSERT(str* != nullptr, "String is nullptr");
    
    skip_spaces(str);

    char*  start    = *str;
    size_t func_len = 0;

    if (!try_parse_identifier(str, &func_len)) {
        *str = start;
        return nullptr;
    }

    func_type_t func = (func_type_t)0;
    if (!get_func_type_by_name((string_t){start, func_len}, &func)) {
        *str = start;
        return nullptr;
    }

    skip_spaces(str);
    if (**str != '(') {
        *str = start;
        return nullptr;
    }

    ++*str; 
    tree_node_t* arg = get_equat(tree, str);
    if (!arg) {
        LOGGER_ERROR("get_func: failed to parse argument");
        return nullptr;
    }

    skip_spaces(str);
    if (**str != ')') {
        LOGGER_ERROR("get_func: expected ')', but got '%c'", **str);
        return arg; 
    }
    ++*str; 

    return init_node(FUNCTION, make_union_func(func), arg, nullptr);
}
