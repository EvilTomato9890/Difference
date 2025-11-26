#include "tree_operations.h"
#include "tree_info.h"
#include "asserts.h"
#include "logger.h"
#include "DSL.h"
#include "differentiator.h"

tree_node_t* get_diff(tree_t* tree, tree_node_t* node) {
    HARD_ASSERT(node != nullptr, "Node is nullptr");

    if (node->type == CONSTANT) {
        return c(0);
    }
    if(node->type == VARIABLE) {
        return c(1);
    }
    tree_node_t* out_node = {};
    tree_node_t* l = node->left  ? clone_node(node->left)  : nullptr;
    tree_node_t* r = node->right ? clone_node(node->right) : nullptr;

    switch(node->value.func) {
        case ADD:  return    ADD_(d(l), d(r));
        case MUL:  return    ADD_(MUL_(d(l), cpy(r)), MUL_(cpy(r), d(r)));
        case SUB:  return    SUB_(d(l), d(r));
        case SIN:  return    MUL_(COS_(cpy(r)), d(l));
        case COS:  return    MUL_(SUB_(c(0), SIN_(l)), d(l));
        default:
            LOGGER_ERROR("Unknown func");
            return nullptr;
    }
    
    return out_node;
}

double calculate_tree(tree_t* tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");
    
}

static double calculate_nodes_recursive(tree_t* tree, tree_node_t* curr_node) {

}