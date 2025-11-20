#ifndef DSL_H_INCLUDED
#define DSL_H_INCLUDED

#define CONST_(val) \
    init_node(CONSTANT, make_union(CONSTANT, val), nullptr, nullptr)

#define VAR_(var_name) \
    init_node(VARIABLE, make_union(VARIABLE, var_name), nullptr, nullptr)

#define FUNC_TEMPLATE(op_code, left, right) \
    init_node(FUNCTION, make_union(FUNCTION, op_code), left, right)

#define ADD_(left, right) FUNC_TEMPLATE(ADD, left, right)
#define SUB_(left, right) FUNC_TEMPLATE(SUB, left, right)
#define MUL_(left, right) FUNC_TEMPLATE(MUL, left, right)
#define SIN_(left, ...)   FUNC_TEMPLATE(SIN, left, nullptr)
#endif