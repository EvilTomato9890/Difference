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
static bool get_func_info_by_name(string_t name, func_type_t* func_out, size_t* argc_out);
static bool expect_char(char** str, char expected);
static bool parse_func_header(char** str, func_type_t* func_out, size_t* argc_out);
static tree_node_t* parse_single_argument(tree_t* tree, char** str);
static bool parse_func_args(tree_t* tree, char** str, size_t argc,
                            tree_node_t** left_out, tree_node_t** right_out);

static tree_node_t* get_equat  (tree_t* tree, char** str);
static tree_node_t* get_term   (tree_t* tree, char** str);
static tree_node_t* get_power  (tree_t* tree, char** str);
static tree_node_t* get_primary(tree_t* tree, char** str);
static tree_node_t* get_num    (              char** str);
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

static bool get_func_info_by_name(string_t name, func_type_t* func_out, size_t* argc_out) {
    HARD_ASSERT(name.ptr != nullptr, "func_name is nulltrp");
    HARD_ASSERT(func_out != nullptr, "func_out is nullptr");
    HARD_ASSERT(argc_out != nullptr, "argc_out is nullptr");

    if (name.len == 0 || *name.ptr == '\0') {
        LOGGER_WARNING("get_func_info_by_name: name is empty str");
        return false;
    }

    #define HANDLE_FUNC(op_code, cmd_name, arg_cnt, ...)   \
        if (my_scstrcmp(name, #cmd_name) == 0) {           \
            *func_out = op_code;                           \
            *argc_out = (size_t)(arg_cnt);                 \
            return true;                                   \
        }

    #include "copy_past_file"

    #undef HANDLE_FUNC

    return false;
}

static bool expect_char(char** str, char expected) {
    HARD_ASSERT(str  != nullptr, "str is nullptr");
    HARD_ASSERT(*str != nullptr, "*str is nullptr");

    if (**str != expected) {
        LOGGER_ERROR("expect_char: expected '%c', but got '%c'", expected, **str);
        return false;
    }
    ++*str;
    return true;
}

static bool parse_func_header(char** str, func_type_t* func_out, size_t* argc_out) {
    HARD_ASSERT(str      != nullptr, "str is nullptr");
    HARD_ASSERT(*str     != nullptr, "*str is nullptr");
    HARD_ASSERT(func_out != nullptr, "func_out is nullptr");
    HARD_ASSERT(argc_out != nullptr, "argc_out is nullptr");

    skip_spaces(str);

    char*  start    = *str;
    size_t func_len = 0;

    if (!try_parse_identifier(str, &func_len)) {
        *str = start;
        return false;
    }

    func_type_t func = (func_type_t)0;
    size_t      argc = 0;

    if (!get_func_info_by_name((string_t){ start, func_len }, &func, &argc)) {
        LOGGER_ERROR("parse_func_header: unknown function '%.*s'", (int)func_len, start);
        *str = start;
        return false;
    }

    *func_out = func;
    *argc_out = argc;
    return true;
}

static tree_node_t* parse_single_argument(tree_t* tree, char** str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str is nullptr");
    HARD_ASSERT(*str != nullptr, "str is nullptr");

    skip_spaces(str);
    tree_node_t* arg = get_equat(tree, str);
    if (!arg) {
        LOGGER_ERROR("parse_single_argument: failed to parse expression as argument");
        return nullptr;
    }
    skip_spaces(str);
    return arg;
}

static bool parse_func_args(tree_t* tree, char** str, size_t argc,
                            tree_node_t** left_out, tree_node_t** right_out) {
    HARD_ASSERT(tree      != nullptr, "tree is nullptr");
    HARD_ASSERT(str       != nullptr, "str is nullptr");
    HARD_ASSERT(*str      != nullptr, "str is nullptr");
    HARD_ASSERT(left_out  != nullptr, "left_out is nullptr");
    HARD_ASSERT(right_out != nullptr, "right_out is nullptr");

    if (argc == 0) {
        LOGGER_ERROR("parse_func_args: functions with 0 arguments are not supported");
        return false;
    }

    if (argc > 2) {
        LOGGER_ERROR("parse_func_args: argc=%zu > 2 is not supported with binary tree_node_t", argc);
        return false;
    }

    tree_node_t* first = parse_single_argument(tree, str);
    if (!first) return false;

    tree_node_t* second = nullptr;

    if (argc >= 2) {
        if (!expect_char(str, ',')) {
            return false;
        }
        second = parse_single_argument(tree, str);
        if (!second) {
            return false;
        }
    }

    *left_out  = first;
    *right_out = second;
    return true;
}

//================================================================================

// G -> E '$'
tree_node_t* get_g(tree_t* tree, char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

    LOGGER_DEBUG("get_g: started");

    skip_spaces(str);
    tree_node_t* node = get_equat(tree, str); 
    if (node == nullptr) { 
        LOGGER_ERROR("get_g: get_equat returned nullptr"); 
        return nullptr;
    }

    skip_spaces(str);
    if (!expect_char(str, '$')) {
        return node;
    }
    return node;
}

// E -> T { ('+' | '-') T }
static tree_node_t* get_equat(tree_t* tree, char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

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

// T -> PWR { ('*' | '/') PWR }
static tree_node_t* get_term(tree_t* tree, char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

    skip_spaces(str);

    tree_node_t* left = get_power(tree, str); 
    if (!left) return nullptr;

    skip_spaces(str);
    while (**str == '*' || **str == '/') { 
        char op = **str; 
        ++*str;
        tree_node_t* right = get_power(tree, str); 
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

// PWR -> P { '^' PWR }
static tree_node_t* get_power(tree_t* tree, char** str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

    skip_spaces(str);

    tree_node_t* left = get_primary(tree, str);
    if (!left) return nullptr;

    skip_spaces(str);
    while (**str == '^') {
        ++*str;
        tree_node_t* right = get_power(tree, str);
        if (!right) {
            LOGGER_ERROR("get_power: right operand is nullptr");
            return left;
        }

        func_type_t func = POW;
        left = init_node(FUNCTION, make_union_func(func), left, right);
        if (!left) {
            LOGGER_ERROR("get_power: init_node failed");
            return nullptr;
        }

        skip_spaces(str);
    }

    return left;
}

// NUM -> [0-9]+
static tree_node_t* get_num(char** str) { 
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

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
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

    skip_spaces(str);

    if (**str == '(') {
        ++*str;

        tree_node_t* node = get_equat(tree, str);
        if (!node) return nullptr;

        skip_spaces(str);
        if (!expect_char(str, ')')) {
            return node;
        }
        return node;
    }

    if (isalpha((unsigned char)**str)) {
        char*  tmp     = *str;
        size_t id_len  = 0;

        if (!try_parse_identifier(&tmp, &id_len)) {
            LOGGER_ERROR("get_primary: failed to parse identifier");
            return nullptr;
        }

        char* after_id = tmp;
        skip_spaces(&after_id);

        if (*after_id == '(') {
            tree_node_t* node = get_func(tree, str);
            if (!node) {
                LOGGER_ERROR("get_primary: failed to parse function call");
            }
            return node;
        } else {
            return get_var(tree, str);
        }
    }

    return get_num(str);
}

// VAR -> [a-zA-Z]+
static tree_node_t* get_var(tree_t* tree, char** str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

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

// FUNC -> IDENT '(' arg_list ')'
static tree_node_t* get_func(tree_t* tree, char** str) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    HARD_ASSERT(str  != nullptr, "str ptr is nullptr");
    HARD_ASSERT(*str != nullptr, "String is nullptr");

    skip_spaces(str);

    char* func_start = *str;

    func_type_t func = (func_type_t)0;
    size_t      argc = 0;

    if (!parse_func_header(str, &func, &argc)) {
        *str = func_start;
        return nullptr;
    }

    skip_spaces(str);
    if (!expect_char(str, '(')) {
        *str = func_start;
        return nullptr;
    }

    tree_node_t* left  = nullptr;
    tree_node_t* right = nullptr;

    if (!parse_func_args(tree, str, argc, &left, &right)) {
        return nullptr;
    }

    skip_spaces(str);
    if (!expect_char(str, ')')) {
        LOGGER_ERROR("get_func: expected ')' after arguments");
        return left;
    }

    tree_node_t* node = nullptr;
    if (argc == 1) {
        node = init_node(FUNCTION, make_union_func(func), left, nullptr);
    } else if (argc == 2) {
        node = init_node(FUNCTION, make_union_func(func), left, right);
    } else {
        LOGGER_ERROR("get_func: unsupported argc=%zu", argc);
        return left;
    }

    if (!node) {
        LOGGER_ERROR("get_func: init_node failed");
        return nullptr;
    }

    return node;
}
