#ifndef DSL_H_INCLUDED
#define DSL_H_INCLUDED

#define d(node) get_diff(tree, node)

#ifdef DIFF_DUMP
#define c(val) \
    init_node_with_dump(CONSTANT, make_union(CONSTANT, val), nullptr, nullptr, tree)

#define v(var_name) \
    init_node_with_dump(VARIABLE, make_union(VARIABLE, get_or_add_var_idx({var_name, strlen(var_name)}, 0, tree)), nullptr, nullptr, tree)

#define FUNC_TEMPLATE(op_code, left, right) \
    init_node_with_dump(FUNCTION, make_union(FUNCTION, op_code), left, right, tree)
#else 
#define c(val) \
    init_node(CONSTANT, make_union(CONSTANT, val), nullptr, nullptr)

#define v(var_name) \
    init_node(VARIABLE, make_union(VARIABLE, get_or_add_var_idx({var_name, strlen(var_name)}, 0, tree)), nullptr, nullptr)

#define FUNC_TEMPLATE(op_code, left, right) \
    init_node(FUNCTION, make_union(FUNCTION, op_code), left, right)
#endif

#define ADD_(left, right) FUNC_TEMPLATE(ADD, left, right)
#define SUB_(left, right) FUNC_TEMPLATE(SUB, left, right)
#define MUL_(left, right) FUNC_TEMPLATE(MUL, left, right)
#define SIN_(left, ...)   FUNC_TEMPLATE(SIN, left, nullptr)
#define COS_(left, ...)   FUNC_TEMPLATE(COS, left, nullptr)
#endif