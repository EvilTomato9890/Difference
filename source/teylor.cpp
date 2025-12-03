#include "forest_info.h"
#include "forest_operations.h"
#include "error_handler.h"
#include "asserts.h"
#include "logger.h"
#include "differentiator.h"
#include "tree_info.h"
#include "tree_operations.h"
#include "DSL.h"
#include "teylor.h"
#include "tree_verification.h"
#include "list_verification.h"
#include "tex_io.h"

#include <math.h>

static const int TEYLOR_DEPTH  = 4;

static unsigned long long calc_fact(int n) {
    if(n > 20) LOGGER_WARNING("Fact wil be overloaded");
    unsigned long long ans = 1;
    for(int i = 2; i <= n; i++) {
        ans *= i;
    }
    return ans;
}

static tree_t* add_diff(forest_t* forest, tree_t* target_tree, args_arr_t args_arr, error_code* error) {
    HARD_ASSERT(forest      != nullptr, "forest is nullptr");
    HARD_ASSERT(target_tree != nullptr, "target_tree is nullptr");
    HARD_ASSERT(error       != nullptr, "error is nullptr");

    tree_t* diff_tree = forest_add_tree(forest, error);
    if(*error != ERROR_NO) {
        LOGGER_ERROR("add_diff: failed add tree");
        return nullptr;
    }
    print_tex_expr(target_tree, target_tree->root, "Текущий ход событий: ");
    print_tex_delimeter(forest->tex_file);
    LOGGER_DEBUG("add_diff: get_diff started");
    tree_node_t* diff_root = get_diff(target_tree->root, args_arr ON_TEX_CREATION_DEBUG(, target_tree));
    if(diff_root == nullptr) {
        *error |= ERROR_GET_DIFF;
        LOGGER_ERROR("add_diff: failed to take diff");
        return nullptr;
    }

    LOGGER_DEBUG("add_diff: optimize_subtree started");
    tree_node_t* optimized_root = optimize_subtree_recursive(diff_root, error);
    if(*error != ERROR_NO) {
        LOGGER_ERROR("add_diff: failed to optimize tree");
        return nullptr;
    }

    tree_replace_root(diff_tree, optimized_root);

    return diff_tree;
}


static const_val_type add_and_calculate_diff(forest_t* forest,    tree_t* target_tree, tree_t** diff_out_tree,
                                             args_arr_t args_arr, error_code* error) {
    HARD_ASSERT(forest      != nullptr, "Forest is nullptr");
    HARD_ASSERT(target_tree != nullptr, "Target_tree is nullptr");
    HARD_ASSERT(error       != nullptr, "error si nullptr");

    LOGGER_DEBUG("Add_and_calculate_diff: started");
    *diff_out_tree = add_diff(forest, target_tree, args_arr, error);
    if(*error != ERROR_NO) {
        LOGGER_ERROR("make_teylor: failed make diff");
        return NAN;
    }
    
    const_val_type result = calculate_tree(*diff_out_tree, false);
    return result;
}


static tree_t* teylor_add_summand(tree_t* teylor_tree, tree_node_t* summand) {
    HARD_ASSERT(teylor_tree != nullptr, "forest is nullptr");
    HARD_ASSERT(summand     != nullptr, "Summand is nullptr");

    tree_node_t* new_root = init_node(FUNCTION, make_union_func(ADD), teylor_tree->root, summand);
    tree_change_root(teylor_tree, new_root);
    return teylor_tree;
}


tree_t* make_teylor(forest_t* forest, tree_t* root_tree, size_t var_idx, const_val_type target_val) { //TODO - Обнаруживание функций 2-х переменных
    HARD_ASSERT(forest    != nullptr, "forest is nullptr");
    HARD_ASSERT(root_tree != nullptr, "tree is nullptr");

    LOGGER_DEBUG("make_teylor: started");

    error_code error = ERROR_NO;
    print_tex_H1(forest->tex_file, "Прибывает Тейлор и куча дальних родственников");
    tree_t* teylor_tree = forest_add_tree(forest, &error);
    if(error != ERROR_NO) {
        LOGGER_ERROR("add_tree failed");
        return nullptr;
    }
    print_tex_expr(teylor_tree, root_tree->root, "Текущий ход событий: "); //REVIEW - СТоит ли делать отдельный парсер
    put_var_val(teylor_tree, var_idx, target_val);

    const_val_type first_val = calculate_tree(root_tree, false);

    tree_init_root(teylor_tree, CONSTANT, make_union_const(first_val));

    for(int i = 1; i < TEYLOR_DEPTH; i++) {
        LOGGER_DEBUG("make_teylor: making %d diff", i);
        print_tex_H2(forest->tex_file, "Прибывает %d-ая волна родственников Тейлора-Боблина", i);
        const_val_type res = add_and_calculate_diff(forest, root_tree, &root_tree, {&var_idx, 1}, &error);

        tree_node_t* target_var = init_node(VARIABLE, make_union_var(var_idx), nullptr, nullptr);
        tree_node_t* summand = MUL_(DIV_(c(res), c((double)calc_fact(i))),
                                    POW_(SUB_(target_var, c(target_val)),
                                         c(i)));
        teylor_add_summand(teylor_tree, summand);
    }
    return teylor_tree;
}