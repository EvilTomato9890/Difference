#include "tree_operations.h"
#include "tree_info.h"
#include "asserts.h"
#include "logger.h"
#include "DSL.h"
#include "differentiator.h"

tree_node_t* get_diff(tree_node_t* node) {
    HARD_ASSERT(node != nullptr, "Node is nullptr");

    error_code error = 0;
    if (node->type == CONSTANT) {
        error |= tree_replace_value(node, CONSTANT, make_union(CONSTANT, 0));
        return node;
    }
    if(node->type == VARIABLE) {
        error |= tree_replace_value(node, CONSTANT, make_union(CONSTANT, 1));
        return node;
    }
    tree_node_t* out_node = {};
    tree_node_t* l = node->left;
    tree_node_t* r = node->right;

    switch(node->value.func) {

        case ADD: 
            out_node = ADD_(d(l), d(r));
            break;
        case MUL:
            out_node = ADD_(MUL_(d(l), r), MUL_(l, d(r)));
            break;
        case SUB:
            out_node = SUB_(d(l), d(r));
            break;
        case SIN:
            out_node = MUL_(COS_(l), d(l));
            break;
        case COS:
            out_node = MUL_(SUB_(c(0), SIN_(l)), d(l));
            break;
        default:
            LOGGER_ERROR("Unknown func");
            return nullptr;
    }
    
    return out_node;
}