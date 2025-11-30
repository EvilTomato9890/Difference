#include "forest_info.h"
#include "forest_operations.h"
#include "error_handler.h"
#include "asserts.h"
#include "logger.h"
#include "differentiator.h"
#include "tree_info.h"
#include "tree_operations.h"
#include "DSL.h"

#include <math.h>

static const int    TEYLOR_DEPTH  = 10;

static unsigned long long calc_fact(int n) {
    if(n > 20) LOGGER_WARNING("Fact wil be overloaded");
    unsigned long long ans = 1;
    for(int i = 2; i <= n; i++) {
        ans *= i;
    }
    return ans;
}

static tree_t* add_diff(forest_t* forest, tree_t* target_tree, error_code* error) {
    HARD_ASSERT(forest      != nullptr, "forest is nullptr");
    HARD_ASSERT(target_tree != nullptr, "target_tree is nullptr");
    HARD_ASSERT(error       != nullptr, "error is nullptr");

    tree_t* diff_tree = forest_add_tree(forest, error);
    if(*error != ERROR_NO) {
        LOGGER_ERROR("add_diff: failed add tree");
        return nullptr;
    }
    tree_node_t* diff_root = get_diff(target_tree->root);
    if(diff_root == nullptr) {
        *error |= ERROR_GET_DIFF;
        LOGGER_ERROR("add_diff: failed to take diff");
        return nullptr;
    }
    tree_change_root(diff_tree, diff_root);

    return diff_tree;
}

static const_val_type add_and_calculate_diff(forest_t* forest, tree_t* target_tree, tree_t** diff_out_tree, error_code* error) {
    HARD_ASSERT(forest      != nullptr, "Forest is nullptr");
    HARD_ASSERT(target_tree != nullptr, "Target_tree is nullptr");
    HARD_ASSERT(error       != nullptr, "error si nullptr");

    *diff_out_tree = add_diff(forest, target_tree, error);
    if(*error != ERROR_NO) {
        LOGGER_ERROR("make_teylor: failed make diff");
        return NAN;
    }
    
    const_val_type result = calculate_tree(*diff_out_tree);
    return result;
}

static tree_t* teylor_add_summand(tree_t* teylor_tree, tree_node_t* summand) {
    HARD_ASSERT(teylor_tree != nullptr, "forest is nullptr");
    HARD_ASSERT(summand     != nullptr, "Summand is nullptr");

    tree_node_t* new_root = init_node(FUNCTION, make_union_func(ADD), teylor_tree->root, summand);
    tree_change_root(teylor_tree, new_root);
    return teylor_tree;
}

tree_t* make_teylor(forest_t* forest, tree_t* root_tree) {
    HARD_ASSERT(tree != nullptr, "tree is nullptr");

    LOGGER_DEBUG("make_teylor: started");

    error_code error = ERROR_NO;
    
    tree_t* teylor_tree = forest_add_tree(forest, &error);
    if(error != ERROR_NO) {
        LOGGER_ERROR("add_tree failed");
        return nullptr;
    }

    for(int i = 0; i < TEYLOR_DEPTH; i++) {
        LOGGER_DEBUG("make_teylor: making %d diff", i);
        const_val_type res = add_and_calculate_diff(forest, root_tree, &root_tree, &error);

        tree_node_t* summand = MUL_(DIV_(c(res), c(calc_fact(i))), POW_()
        teylor_add_summand(teylor_tree, )
    }
}
