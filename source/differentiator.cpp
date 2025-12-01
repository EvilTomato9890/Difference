#include "debug_meta.h"
#include "tree_operations.h"
#include "tree_info.h"
#include "asserts.h"
#include "logger.h"
#include "DSL.h"
#include "differentiator.h"
#include "my_string.h"
#include "forest_info.h"
#include "forest_operations.h"

#include <math.h>

static const double CMP_PRECISION = 1e-9;

//================================================================================
// TODO - diff on 1 arg Correct?
static bool check_in(size_t target, args_arr_t args_arr) {
    if(args_arr.arr == nullptr || args_arr.size == 0) return false;
    
    for(size_t i = 0; i < args_arr.size; i++) 
        if(target == args_arr.arr[i]) return true;
    return false;
}

tree_node_t* get_diff(tree_node_t* node, args_arr_t args_arr) {
    HARD_ASSERT(args_arr.size == 0 || args_arr.arr != nullptr, "Wrong arg list");
    if(!node) return nullptr;

    if (node->type == CONSTANT) {
        return c(0);
    }
    if(node->type == VARIABLE) {
        if(args_arr.size == 0) 
            return c(1);
        if(check_in(node->value.var_idx, args_arr)) 
            return c(1);
        return c(0);
    }

    tree_node_t* l = node->left;
    tree_node_t* r = node->right;

    switch(node->value.func) {
                                // (u + v)' = u' + v'
        case ADD:     return    ADD_(d(l), d(r));

                                // (u * v)' = u'v + uv'
        case MUL:     return    ADD_(MUL_(d(l), cpy(r)), 
                                     MUL_(cpy(l), d(r)));

                                // (u - v)' = u' - v'     
        case SUB:     return    SUB_(d(l), d(r));

                                // (u / v)' = (u'v - uv') / v^2
        case DIV:     return    DIV_(SUB_(MUL_(d(l), cpy(r)), 
                                          MUL_(cpy(l), d(r))), 
                                     MUL_(cpy(r), cpy(r)));

                                // (sin u)' = cos(u) * u'     
        case SIN:     return    MUL_(COS_(cpy(l)), d(l));

                                // (cos u)' = -sin(u) * u'
        case COS:     return    MUL_(SUB_(c(0), SIN_(cpy(l))), d(l));

                                // (tan u)' = 1 / cos(u)^2 * u'
        case TAN:     return    DIV_(d(l), MUL_(COS_(cpy(l)), COS_(cpy(l))));

                                // (ctan u)' = -1 / sin(u)^2 * u'
        case CTAN:    return    SUB_(c(0), DIV_(d(l), ADD_(c(1), MUL_(cpy(l), cpy(l)))));

                                // (arcsin u)' = 1 / sqrt(1 - u^2) * u'
        case ARCSIN:  return    DIV_(d(l), POW_(SUB_(c(1), MUL_(cpy(l), cpy(l))), c(0.5)));

                                // (arccos u)' = -1 / sqrt(1 - u^2) * u'
        case ARCCOS:  return    SUB_(c(0), DIV_(d(l), POW_(SUB_(c(1), 
                                                                MUL_(cpy(l), cpy(l))), c(0.5))));

                                // (arctan u)' = 1 / (1 + u^2) * u'                                   
        case ARCTAN:  return    DIV_(d(l), ADD_(c(1), MUL_(cpy(l), cpy(l))));

                                // (arcctan u)' = -1 / (1 + u^2) * u'
        case ARCCTAN: return    SUB_(c(0), DIV_(d(l), ADD_(c(1), MUL_(cpy(l), cpy(l)))));

                                // (sh u)' = ch u * u'
        case SH:      return    MUL_(CH_(cpy(l)), d(l));

                                // (ch u)' = sh u * u'
        case CH:      return    MUL_(SH_(cpy(l)), d(l));

                                // (arcsh u)' = 1 / sqrt(1 + u^2) * u'
        case ARCSH:   return    DIV_(d(l), POW_(ADD_(c(1), MUL_(cpy(l), cpy(l))), c(0.5)));

                                // (arcch u)' = 1 / sqrt(u^2 - 1) * u'
        case ARCCH:   return    DIV_(d(l), POW_(SUB_(MUL_(cpy(l), cpy(l)), c(1)), c(0.5)));

                                // (ln u)' = 1 / u * u'
        case LN:      return    DIV_(d(l), cpy(l));

                                // (e^u)' = e^u * u'
        case EXP:     return    MUL_(POW_(c(M_E), cpy(l)), d(l));

                                //(u^v) = u^v * (v' ln u + v u'/u)
        case POW:     return    MUL_(POW_(cpy(l), cpy(r)), ADD_(MUL_(d(r),   LN_(cpy(l))),
                                                                MUL_(cpy(r), DIV_(d(l), cpy(l)))));

                                //log_u(v) = [ (v'/v) ln u - (u'/u) ln v ] / (ln u)^2
        case LOG:     return    DIV_(SUB_(MUL_(DIV_(d(r), cpy(r)), LN_(cpy(l))),
                                          MUL_(DIV_(d(l), cpy(l)), LN_(cpy(r)))), 
                                     MUL_(LN_(cpy(l)), LN_(cpy(l))));
        
        default:
            LOGGER_ERROR("Unknown func");
            return nullptr;
    }
}

//================================================================================

#define BASIC_FUNC_TEMPLATE_ONE_ARG(name, function)                      \
    static var_val_type name##_func(var_val_type a, var_val_type b) {    \
        HARD_ASSERT(!isnan(a), "a is nan");                              \
        (void)b;                                                         \
        return function;                                                 \
    }

#define BASIC_FUNC_TEMPLATE_TWO_ARGS(name, function)                     \
    static var_val_type name##_func(var_val_type a, var_val_type b) {    \
        HARD_ASSERT(!isnan(a), "a is nan");                              \
        HARD_ASSERT(!isnan(b), "b is nan");                              \
        return function;                                                 \
    }

// Add -> eval, differentiate, 
//TODO: вынести в отдельный файл и добавить функции вычисления произодных подобными шаблонами в отдельном файле
BASIC_FUNC_TEMPLATE_TWO_ARGS(ADD, a + b)
BASIC_FUNC_TEMPLATE_TWO_ARGS(MUL, a * b)
BASIC_FUNC_TEMPLATE_TWO_ARGS(SUB, a - b)
BASIC_FUNC_TEMPLATE_TWO_ARGS(DIV, a / b)

BASIC_FUNC_TEMPLATE_TWO_ARGS(POW, pow(a, b))
BASIC_FUNC_TEMPLATE_TWO_ARGS(LOG, log(b) / log(a))
BASIC_FUNC_TEMPLATE_ONE_ARG (LN,  log(a))
BASIC_FUNC_TEMPLATE_ONE_ARG (EXP, exp(a))

BASIC_FUNC_TEMPLATE_ONE_ARG(SIN, sin(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(COS, cos(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(TAN, tan(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(CTAN, 1.0 / tan(a))

BASIC_FUNC_TEMPLATE_ONE_ARG(ARCSIN, asin(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(ARCCOS, acos(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(ARCTAN, atan(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(ARCCTAN, M_PI_2 - atan(a))

BASIC_FUNC_TEMPLATE_ONE_ARG(CH, cosh(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(SH, sinh(a))

BASIC_FUNC_TEMPLATE_ONE_ARG(ARCSH, asinh(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(ARCCH, acosh(a))

#undef BASIC_FUNC_TEMPLATE_ONE_ARG
#undef BASIC_FUNC_TEMPLATE_TWO_ARGS

//================================================================================

static var_val_type calculate_nodes_recursive(tree_t* tree, tree_node_t* curr_node, error_code* error) {
    HARD_ASSERT(tree      != nullptr, "tree is nullptr");
    HARD_ASSERT(error     != nullptr, "Error is nullptr");

    if(curr_node       == nullptr)  return nan("4");
    if(curr_node->type == VARIABLE) return get_var_val(tree, curr_node);
    if(curr_node->type == CONSTANT) return curr_node->value.constant;
    if(curr_node->type == FUNCTION) {

        #define HANDLE_FUNC(func_name, ...)                                                        \
            case func_name:                                                                        \
                return func_name##_func(calculate_nodes_recursive(tree, curr_node->left,  error),  \
                                        calculate_nodes_recursive(tree, curr_node->right, error));

        switch(curr_node->value.func) {
            #include "copy_past_file"
            default:
                LOGGER_ERROR("Unknown func op_code");
                return nan("5");
        }

        #undef HANDLE_FUNC
    }

    LOGGER_ERROR("Unknown node type");
    return nan("3");
    
}

//================================================================================

static void clear_input_buff() {
    int ch = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
}

static var_val_type take_var(c_string_t var) {
    HARD_ASSERT(var.ptr != nullptr, "String is nullptr");

    var_val_type num = 0;

    printf("Enter %.*s: ", (int)var.len, var.ptr);
    int smb_cnt = scanf("%lf", &num);
    if(smb_cnt == 0) {
        clear_input_buff();
        printf("Wrong type. Enter again: ");
        smb_cnt = scanf("%lf", &num);
        if(smb_cnt == 0) {
            LOGGER_ERROR("Wrong type");
            clear_input_buff();
            return nan("1");
        }
    }
    return num;

}

static error_code ask_for_vars(tree_t* tree) {
    HARD_ASSERT(tree            != nullptr, "Tree is nullptr");
    HARD_ASSERT(tree->var_stack != nullptr, "Stack is nullptr");

    stack_t* var_stack = tree->var_stack;
    for(size_t i = 0; i < var_stack->size; i++) {
        if(var_stack->data[i].str.ptr != nullptr) {
            var_val_type var_val = take_var(var_stack->data[i].str);
            if(!isnan(var_val)) var_stack->data[i].val = var_val;
            else {
                LOGGER_ERROR("Failed to read var");
                return ERROR_READ_FILE;
            }
        }   
    }
    return ERROR_NO;
}

//================================================================================

var_val_type calculate_tree(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    
    LOGGER_DEBUG("Calculate_tree: started");

    error_code error = ERROR_NO;

    error |= ask_for_vars(tree);
    if(error != ERROR_NO) {
        LOGGER_ERROR("ask_for_vars failed");
        return nan("1");
    }

    var_val_type ans = calculate_nodes_recursive(tree, tree->root, &error);
    if(error != ERROR_NO) {
        LOGGER_ERROR("calculate_nodes_recursive failed");
        return nan("2");
    }
    
    return ans;
}

//================================================================================

static int double_cmp(double a, double b) {
    if(fabs(a - b) < CMP_PRECISION) return 0;
    if(a - b      > CMP_PRECISION)  return 1;
    return -1;
}

static bool node_is_constant(const tree_node_t* node) {
    return node != nullptr && node->type == CONSTANT;
}

//--------------------------------------------------------------------------------

static error_code handle_fixed_elem(tree_node_t* node, const_val_type constant_value) {
    if (node == nullptr) return ERROR_NO;

    error_code error = ERROR_NO;

    if (node->left) {
        error |= destroy_node_recursive(node->left, nullptr);
        node->left = nullptr;
    }
    if (node->right) {
        error |= destroy_node_recursive(node->right, nullptr);
        node->right = nullptr;
    }

    node->type           = CONSTANT;
    node->value.constant = constant_value;

    if (error != ERROR_NO) {
        LOGGER_ERROR("handle_fixed_elem: destroy_node_recursive failed");
    }
    return error;
}

//--------------------------------------------------------------------------------

static error_code handle_neutral_elem(tree_node_t* node,
                                      tree_node_t* child_keep_ptr,
                                      tree_node_t* child_drop_ptr) {
    HARD_ASSERT(node           != nullptr,        "handle_neutral_elem: node is nullptr");
    HARD_ASSERT(child_keep_ptr != nullptr, "handle_neutral_elem: child_keep_ptr is nullptr");

    error_code error = ERROR_NO;

    if (child_drop_ptr != nullptr) {
        error |= destroy_node_recursive(child_drop_ptr, nullptr);
    }

    tree_node_t child_copy = *child_keep_ptr;
    *node = child_copy;

    free(child_keep_ptr);

    if (error != ERROR_NO) {
        LOGGER_ERROR("handle_neutral_elem: destroy_node_recursive failed");
    }
    return error;
}

//--------------------------------------------------------------------------------

static const_val_type eval_function_constant(func_type_t func_type_value,
                                             const_val_type left_value,
                                             const_val_type right_value) {

    #define HANDLE_FUNC(func_name, ...) \
        case func_name:                 \
            return func_name##_func(left_value, right_value);

    switch (func_type_value) {
        #include "copy_past_file"
        default:
            LOGGER_ERROR("eval_function_constant: Unknown func op_code %d",
                         (int)func_type_value);
            return NAN;
    }

    #undef HANDLE_FUNC
}

//--------------------------------------------------------------------------------

static error_code fold_constants_in_node(tree_node_t* node) {
    HARD_ASSERT(node != nullptr, "fold_constants_in_node: node is nullptr");

    if (node->type != FUNCTION) return ERROR_NO;

    tree_node_t* left_ptr  = node->left;
    tree_node_t* right_ptr = node->right;

    const bool is_left_constant  = node_is_constant(left_ptr);
    const bool is_right_constant = node_is_constant(right_ptr);

    const bool is_unary = (right_ptr == nullptr);

    if (is_unary) {
        if (!is_left_constant) {
            return ERROR_NO;
        }
    } else {
        if (!is_left_constant || !is_right_constant) {
            return ERROR_NO;
        }
    }

    const_val_type  left_value =  left_ptr ?  left_ptr->value.constant : 0.0;
    const_val_type right_value = right_ptr ? right_ptr->value.constant : 0.0;

    const_val_type result_value = eval_function_constant(node->value.func,
                                                         left_value,
                                                         right_value);
    if (isnan(result_value)) {
        LOGGER_ERROR("fold_constants_in_node: eval_function_constant failed");
        return ERROR_UNKNOWN_FUNC;
    }

    error_code error = ERROR_NO;

    if (left_ptr != nullptr) {
        error |= destroy_node_recursive(left_ptr, nullptr);
    }
    if (right_ptr != nullptr) {
        error |= destroy_node_recursive(right_ptr, nullptr);
    }

    node->left           = nullptr;
    node->right          = nullptr;
    node->type           = CONSTANT;
    node->value.constant = result_value;

    if (error != ERROR_NO) {
        LOGGER_ERROR("fold_constants_in_node: destroy_node_recursive failed");
    }

    return error;
}

//--------------------------------------------------------------------------------

struct arg_rule_t {
    const_val_type neutral_value;
    const_val_type fixed_value;
    const_val_type fixed_result;
};

struct op_rule_t {
    func_type_t func_type_value;
    size_t      arg_count;
    arg_rule_t  args[MAX_NEUTRAL_ARGS];
};

#define ARG_RULE(neu, fix, res)  { (neu), (fix), (res) }

static const op_rule_t OP_SIMPLIFY_RULES[] = {
    // u + 0 = u, 0 + v = v
    { ADD, 2, {
        ARG_RULE(0.0, NAN, 0.0),  
        ARG_RULE(0.0, NAN, 0.0)   
    }},

    // u - 0 = u
    { SUB, 2, {
        ARG_RULE(NAN, NAN, 0.0),
        ARG_RULE(0.0, NAN, 0.0)  
    }},

    // 1 * v = v, u * 1 = u
    // 0 * v = 0, u * 0 = 0
    { MUL, 2, {
        ARG_RULE(1.0, 0.0, 0.0),  
        ARG_RULE(1.0, 0.0, 0.0)  
    }},

    // 0 / v = 0   u / 1 = u
    { DIV, 2, {
        ARG_RULE(NAN, 0.0, 0.0), 
        ARG_RULE(1.0, NAN, 0.0)  
    }},

    // 1^v = 1  u^0 = 1  u^1 = u      
    { POW, 2, {
        ARG_RULE(NAN, 1.0, 1.0),  
        ARG_RULE(1.0, 0.0, 1.0)   
    }},

    // log_u(1) = 0 
    { LOG, 2, {
        ARG_RULE(NAN, NAN, 0.0),  
        ARG_RULE(NAN, 1.0, 0.0)   
    }},
};

#undef ARG_RULE

static const op_rule_t* find_op_rule(func_type_t func_type_value) {
    const size_t rules_count = sizeof(OP_SIMPLIFY_RULES) / sizeof(OP_SIMPLIFY_RULES[0]);
    for (size_t i = 0; i < rules_count; ++i) {
        if (OP_SIMPLIFY_RULES[i].func_type_value == func_type_value) {
            return &OP_SIMPLIFY_RULES[i];
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------------
//TODO: поправить для разных фиксированных элементов аргументов
static error_code simplify_fixed_elements(tree_node_t* node,
                                          const op_rule_t* rule) {
    HARD_ASSERT(node != nullptr, "simplify_fixed_elements: node is nullptr");
    HARD_ASSERT(rule != nullptr, "simplify_fixed_elements: rule is nullptr");

    tree_node_t* args[MAX_NEUTRAL_ARGS] = { node->left, node->right };
    error_code error = ERROR_NO;

    for (size_t i = 0; i < rule->arg_count; ++i) {
        const arg_rule_t& arg_rule = rule->args[i];
        tree_node_t*      arg_node = args[i];

        if (isnan(arg_rule.fixed_value))                                     continue;
        if (!node_is_constant(arg_node))                                     continue;
        if (double_cmp(arg_node->value.constant, arg_rule.fixed_value) != 0) continue;

        error |= handle_fixed_elem(node, arg_rule.fixed_result);
        return error;
    }

    return error;
}


static error_code simplify_neutral_elements(tree_node_t* node,
                                            const op_rule_t* rule) {
    HARD_ASSERT(node != nullptr, "simplify_neutral_elements: node is nullptr");
    HARD_ASSERT(rule != nullptr, "simplify_neutral_elements: rule is nullptr");

    tree_node_t* args[MAX_NEUTRAL_ARGS] = { node->left, node->right };
    error_code error = ERROR_NO;

    for (size_t i = 0; i < rule->arg_count; ++i) {
        const arg_rule_t& arg_rule = rule->args[i];
        tree_node_t*      arg_node = args[i];

        if (isnan(arg_rule.neutral_value))                                  continue;
        if (!node_is_constant(arg_node))                                    continue;
        if (double_cmp(arg_node->value.constant,  arg_rule.neutral_value))  continue;

        size_t other_index = (i == 0 ? 1 : 0);
        tree_node_t* keep_ptr = args[other_index];
        tree_node_t* drop_ptr = arg_node;

        if (keep_ptr != nullptr) {
            error |= handle_neutral_elem(node, keep_ptr, drop_ptr);
        }
        return error;
    }

    return error;
}

//--------------------------------------------------------------------------------

static error_code simplify_neutral_and_constant_elements(tree_node_t* node) {
    HARD_ASSERT(node != nullptr,
                "simplify_neutral_and_constant_elements: node is nullptr");

    if (node->type != FUNCTION) return ERROR_NO;

    const op_rule_t* rule = find_op_rule(node->value.func);
    if (rule == nullptr) {
        return ERROR_NO;
    }

    error_code error = ERROR_NO;

    error |= simplify_fixed_elements(node, rule);
    if (error != ERROR_NO) {
        return error;
    }

    if (node->type != FUNCTION) {
        return ERROR_NO;
    }

    error |= simplify_neutral_elements(node, rule);
    return error;
}

//--------------------------------------------------------------------------------
//TODO: 0^0
static tree_node_t* optimize_subtree_recursive(tree_t* tree,
                                               tree_node_t* node,
                                               error_code* error_ptr) {
    HARD_ASSERT(error_ptr != nullptr, "optimize_subtree_recursive: error_ptr is nullptr");

    if (node == nullptr) {
        return nullptr;
    }

    if (*error_ptr != ERROR_NO) {
        return node;
    }

    if (node->type == FUNCTION) {
        node->left  = optimize_subtree_recursive(tree, node->left,  error_ptr); //TODO: delete tree
        node->right = optimize_subtree_recursive(tree, node->right, error_ptr);

        *error_ptr |= fold_constants_in_node(node);
        *error_ptr |= simplify_neutral_and_constant_elements(node);
    }

    return node;
}

//================================================================================

error_code tree_optimize(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree_optimize: tree is nullptr");

    LOGGER_DEBUG("tree_optimize: started");
    if (tree->root == nullptr) {
        return ERROR_NO;
    }

    error_code error_value = ERROR_NO;
    tree->root = optimize_subtree_recursive(tree, tree->root, &error_value);

    if (error_value != ERROR_NO) {
        LOGGER_ERROR("tree_optimize: optimize_subtree_recursive failed");
        return error_value;
    }

    tree->size = count_nodes_recursive(tree->root);
    return ERROR_NO;
}
