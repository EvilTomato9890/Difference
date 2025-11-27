#ifndef DSL_H_INCLUDED
#define DSL_H_INCLUDED

#include "debug_meta.h"

#define d(node) get_diff(tree, node)

#define cpy(node) subtree_deep_copy(node, nullptr ON_CREATION_DEBUG(, tree))

#ifdef CREATION_DEBUG
#define c(val) \
    init_node_with_dump(CONSTANT, make_union_const(val), nullptr, nullptr, tree)

#define v(var_name) \
    init_node_with_dump(VARIABLE, make_union_var(get_or_add_var_idx({var_name, strlen(var_name)}, 0, tree->var_stack, nullptr)), nullptr, nullptr, tree)

#define FUNC_TEMPLATE(op_code, left, right) \
    init_node_with_dump(FUNCTION, make_union_func(op_code), left, right, tree)
#else 

#define c(val) \
    init_node(CONSTANT, make_union_const(val), nullptr, nullptr)

#define v(var_name) \
    init_node(VARIABLE, make_union_var(get_or_add_var_idx({var_name, strlen(var_name)}, 0, tree->var_stack, nullptr)), nullptr, nullptr)

#define FUNC_TEMPLATE(op_code, left, right) \
    init_node(FUNCTION, make_union_func(op_code), left, right)
#endif

//================================================================================

#define ADD_(left, right) FUNC_TEMPLATE(ADD,     left, right)
#define SUB_(left, right) FUNC_TEMPLATE(SUB,     left, right)
#define MUL_(left, right) FUNC_TEMPLATE(MUL,     left, right)
#define DIV_(left, right) FUNC_TEMPLATE(DIV,     left, right)

#define POW_(left, right) FUNC_TEMPLATE(POW,     left, right)
#define LOG_(left, right) FUNC_TEMPLATE(LOG,     left, right)
#define LN_(left)         FUNC_TEMPLATE(LN,      left, nullptr)
#define EXP_(left)        FUNC_TEMPLATE(EXP,     left, nullptr)

#define SIN_(left)        FUNC_TEMPLATE(SIN,     left, nullptr)
#define COS_(left)        FUNC_TEMPLATE(COS,     left, nullptr)
#define TAN_(left)        FUNC_TEMPLATE(TAN,     left, nullptr)
#define CTAN_(left)       FUNC_TEMPLATE(CTAN,    left, nullptr)

#define ARCSIN_(left)     FUNC_TEMPLATE(ARCSIN,  left, nullptr)
#define ARCCOS_(left)     FUNC_TEMPLATE(ARCCOS,  left, nullptr)
#define ARCTAN_(left)     FUNC_TEMPLATE(ARCTAN,  left, nullptr)
#define ARCCTAN_(left)    FUNC_TEMPLATE(ARCCTAN, left, nullptr)

#define CH_(left)         FUNC_TEMPLATE(CH,      left, nullptr)
#define SH_(left)         FUNC_TEMPLATE(SH,      left, nullptr)

#define ARCSH_(left)      FUNC_TEMPLATE(ARCSH,   left, nullptr)
#define ARCCH_(left)      FUNC_TEMPLATE(ARCCH,   left, nullptr)

//================================================================================


#endif