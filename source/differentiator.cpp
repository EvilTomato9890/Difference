#include "debug_meta.h"
#include "tree_operations.h"
#include "tree_info.h"
#include "asserts.h"
#include "logger.h"
#include "DSL.h"
#include "differentiator.h"
#include "math.h"

//================================================================================

tree_node_t* get_diff(const tree_t* tree, tree_node_t* node) {
    if(!node) return nullptr;

    if (node->type == CONSTANT) {
        return c(0);
    }
    if(node->type == VARIABLE) {
        return c(1);
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
        case CTAN:    return    DIV_(d(l), ADD_(c(1), MUL_(cpy(l), cpy(l))));

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
        case ARCSH:   return    DIV_(c(1), POW_(ADD_(d(l), MUL_(cpy(l), cpy(l))), c(0.5)));

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

#define BASIC_OPERATION_TEMPLATE(name, operation)                      \
    static var_val_type name##_func (var_val_type a, var_val_type b) { \
        HARD_ASSERT(!isnan(a), "a is nan");                            \
        HARD_ASSERT(!isnan(b), "b is nan");                            \
        return operation;                                              \
    }                

BASIC_OPERATION_TEMPLATE(ADD, a + b)
BASIC_OPERATION_TEMPLATE(MUL, a * b)
BASIC_OPERATION_TEMPLATE(SUB, a - b)
BASIC_OPERATION_TEMPLATE(DIV, a / b)

BASIC_FUNC_TEMPLATE_TWO_ARGS(POW, pow(a, b))
BASIC_FUNC_TEMPLATE_TWO_ARGS(LOG, log(b) / log(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(LN, log(a))
BASIC_FUNC_TEMPLATE_ONE_ARG(EXP, exp(a))

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

#undef BASIC_OPERATION_TEMPLATE
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

static var_val_type take_var(string_t var) {
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
