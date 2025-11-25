#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "asserts.h"
#include "tree_operations.h"
#include "tree_verification.h"
#include "tree_info.h"
#include "error_handler.h"
#include "logger.h"
#include "DSL.h"
#include "differentiator.h"
#include "tree_file_io.h"
#include "../libs/StackDead-main/stack.h"
#include "forest_info.h"
#include "forest_operations.h"
#include "file_operations.h"

//================================================================================

static void test_read_empty_tree() {
    LOGGER_INFO("=== Тест: чтение пустого дерева ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");

    forest_open_dump_file(&forest, "empty_tree_test");
    HARD_ASSERT(forest.dump_file != nullptr, "failed to create test file");

    error = read_file_to_buffer(forest.dump_file, &forest.buff);
    HARD_ASSERT(error == ERROR_NO, "Read file error");

    error = tree_parse_from_buffer(test_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_read_from_file failed");
    HARD_ASSERT(test_tree->root == nullptr, "root should be nullptr for empty tree");
    HARD_ASSERT(test_tree->size == 0, "size should be 0 for empty tree");

    error = tree_write_to_file(test_tree, "Empty_tree_test1");
    HARD_ASSERT(error == ERROR_NO, "AA");

    forest_close_dump_file(&forest);
    forest_dest(&forest);

    LOGGER_INFO("Тест пройден: чтение пустого дерева\n");
}

static void test_read_single_constant() {
    LOGGER_INFO("=== Тест: чтение дерева с одной константой ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");

    forest_open_dump_file(&forest, "Single_tree_test");
    HARD_ASSERT(forest.dump_file != nullptr, "failed to create test file");

    error = read_file_to_buffer(forest.dump_file, &forest.buff);
    HARD_ASSERT(error == ERROR_NO, "Read file error");

    error = tree_parse_from_buffer(test_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_read_from_file failed");
    HARD_ASSERT(test_tree->root != nullptr, "root should be nullptr for empty tree");
    HARD_ASSERT(test_tree->size == 1, "size should be 0 for empty tree");

    error = tree_write_to_file(test_tree, "single_tree_test1");
    HARD_ASSERT(error == ERROR_NO, "AA");

    forest_close_dump_file(&forest);
    forest_dest(&forest);

    LOGGER_INFO("Тест пройден: чтение дерева с одной константой\n");
}

static void test_read_single_variable() {
    LOGGER_INFO("=== Тест: чтение дерева с одной переменной ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    const char* filename = "test_variable.tree";
    FILE* file = fopen(filename, "w");
    HARD_ASSERT(file != nullptr, "failed to create test file");
    fprintf(file, "(\"x\" nil nil)");
    fclose(file);
    
    error = tree_read_from_file(&tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_read_from_file failed");
    HARD_ASSERT(tree.root != nullptr, "root should not be nullptr");
    HARD_ASSERT(tree.root->type == VARIABLE, "node type should be VARIABLE");


    HARD_ASSERT(my_ssstrcmp(tree.var_stack->data[tree.root->value.var_idx].str, {"x", 1}) == 0, "variable name should be 'x'");
    HARD_ASSERT(tree.size == 1, "size should be 1");

    ON_DEBUG(
    tree.dump_file = fopen("aaaa.html", "w");
    HARD_ASSERT(tree.dump_file != nullptr, "failed to create dump file");
    )
    
    error = tree_dump(&tree, VER_INIT, true, "Tesas");

    ON_DEBUG(
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");
    if (tree.dump_file != nullptr) {
        fclose(tree.dump_file);
        tree.dump_file = nullptr;
    }
    )
    tree_destroy(&tree);
    remove(filename);
    LOGGER_INFO("Тест пройден: чтение дерева с одной переменной\n");
}

static void test_DSL() {
    tree_t tree_origin = {};
    stack_t stack = {};
    error_code error = tree_init(&tree_origin, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "INIT failed");

    #ifdef VERIFY_DEBUG
    tree_origin.dump_file = fopen("test_dump_DSL.html", "w");
    HARD_ASSERT(tree_origin.dump_file != nullptr, "failed to create file");
    #endif
    tree_t* tree = &tree_origin;
    tree_origin.root = ADD_(SIN_(c(1)), c(2));
    tree_origin.head->left = tree_origin.root;

    tree_dump(tree, VER_INIT, true, "FF");

    #ifdef VERIFY_DEBUG
    if (tree_origin.dump_file != nullptr) {
        fclose(tree_origin.dump_file);
        tree_origin.dump_file = nullptr;
    }
    #endif


    tree_destroy(tree);
}

static void test_read_complex_tree() {
    LOGGER_INFO("=== Тест: чтение сложного дерева ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    const char* filename = "test_complex.tree";

    #ifdef VERIFY_DEBUG
    tree.dump_file = fopen("test_dump_nodes.html", "w");
    HARD_ASSERT(tree.dump_file != nullptr, "failed to create dump file");
    #endif
    error = tree_read_from_file(&tree, filename);
    error = tree_dump(&tree, VER_INIT, true, "Test dump tree with nodes");



    tree_t tree_diff = {};
    stack_t stack_diff = {};
    error = tree_init(&tree_diff, &stack_diff ON_DEBUG(, VER_INIT));
    ON_DEBUG({
    tree_diff.dump_file = fopen("test_dump_nodes.html", "a");
    })

    tree_node_t* new_root = get_diff(&tree, tree.root);
    tree_make_root(&tree_diff, new_root);

    LOGGER_DEBUG("Diff ended");

    error = tree_dump(&tree, VER_INIT, true, "After diff");
    
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");
    
    #ifdef VERIFY_DEBUG
    if (tree.dump_file != nullptr) {
        fclose(tree.dump_file);
        tree.dump_file = nullptr;
    }
    #endif


    tree_destroy(&tree);
    tree_destroy(&tree_diff);
    //remove(filename);
    LOGGER_INFO("Тест пройден: чтение сложного дерева\n");
}

static void test_read_nonexistent_file() {
    LOGGER_INFO("=== Тест: чтение несуществующего файла ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    error = tree_read_from_file(&tree, "nonexistent_file.tree");
    HARD_ASSERT(error & ERROR_OPEN_FILE, "should return ERROR_OPEN_FILE");
    
    tree_destroy(&tree);
    LOGGER_INFO("Тест пройден: чтение несуществующего файла\n");
}

//================================================================================

static void test_write_empty_tree() {
    LOGGER_INFO("=== Тест: запись пустого дерева ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    const char* filename = "test_write_empty.tree";
    error = tree_write_to_file(&tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    
    FILE* file = fopen(filename, "r");
    HARD_ASSERT(file != nullptr, "output file should exist");
    char buffer[10] = {};
    fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    tree_destroy(&tree);
    remove(filename);
    LOGGER_INFO("Тест пройден: запись пустого дерева\n");
}

static void test_write_constant_tree() {
    LOGGER_INFO("=== Тест: запись дерева с константой ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    value_t value = make_union(CONSTANT, (const_val_type)100);
    tree_node_t* root = tree_init_root(&tree, CONSTANT, value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    const char* filename = "test_write_constant.tree";
    error = tree_write_to_file(&tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    
    tree_t read_tree = {};
    stack_t read_stack = {};
    error = tree_init(&read_tree, &read_stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    error = tree_read_from_file(&read_tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_read_from_file failed");
    HARD_ASSERT(read_tree.root != nullptr, "root should not be nullptr");
    HARD_ASSERT(read_tree.root->type == CONSTANT, "type should be CONSTANT");
    HARD_ASSERT(read_tree.root->value.constant == 100, "value should be 100");
    
    tree_destroy(&tree);
    tree_destroy(&read_tree);
    remove(filename);
    LOGGER_INFO("Тест пройден: запись дерева с константой\n");
}

static void test_write_variable_tree() {
    LOGGER_INFO("=== Тест: запись дерева с переменной ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    size_t idx = add_var({"test_var", strlen("test_var")}, 0, &stack, &error);
    value_t value = make_union(VARIABLE, idx);
    tree_node_t* root = tree_init_root(&tree, VARIABLE, value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    const char* filename = "test_write_variable.tree";
    error = tree_write_to_file(&tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    
    tree_t read_tree = {};
    stack_t read_stack = {};
    error = tree_init(&read_tree, &read_stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    error = tree_read_from_file(&read_tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_read_from_file failed");
    HARD_ASSERT(read_tree.root != nullptr, "root should not be nullptr");
    HARD_ASSERT(read_tree.root->type == VARIABLE, "type should be VARIABLE");
    HARD_ASSERT(strcmp(tree.var_stack->data[tree.root->value.var_idx].str.ptr, "test_var") == 0, "variable name should match");
    
    tree_destroy(&tree);
    tree_destroy(&read_tree);
    remove(filename);
    LOGGER_INFO("Тест пройден: запись дерева с переменной\n");
}

static void test_write_complex_tree() {
    LOGGER_INFO("=== Тест: запись сложного дерева ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    value_t root_value = make_union(FUNCTION, ADD);
    tree_node_t* root = tree_init_root(&tree, FUNCTION, root_value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    value_t left_value = make_union(CONSTANT, (const_val_type)10);
    tree_node_t* left = tree_insert_left(&tree, CONSTANT, left_value, root);
    HARD_ASSERT(left != nullptr, "tree_insert_left failed");
    
    size_t idx = add_var({"y", 1}, 0, &stack, nullptr);
    value_t right_value = make_union(VARIABLE, idx);
    tree_node_t* right = tree_insert_right(&tree, VARIABLE, right_value, root);
    HARD_ASSERT(right != nullptr, "tree_insert_right failed");
    
    const char* filename = "test_write_complex.tree";
    error = tree_write_to_file(&tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    tree_dump(&tree, VER_INIT, true, "Aaa");
    tree_t read_tree = {};
    stack_t read_stack = {};
    error = tree_init(&read_tree, &read_stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    error = tree_read_from_file(&read_tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_read_from_file failed");
    HARD_ASSERT(read_tree.root != nullptr, "root should not be nullptr");
    HARD_ASSERT(read_tree.root->type == FUNCTION, "root type should be FUNCTION");
    HARD_ASSERT(read_tree.size == 3, "size should be 3");
    
    tree_destroy(&tree);
    tree_destroy(&read_tree);
    LOGGER_INFO("Тест пройден: запись сложного дерева\n");
}

//================================================================================

static void test_dump_empty_tree() {
    LOGGER_INFO("=== Тест: дамп пустого дерева ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    #ifdef VERIFY_DEBUG
    tree.dump_file = fopen("test_dump_empty.html", "w");
    HARD_ASSERT(tree.dump_file != nullptr, "failed to create dump file");
    #endif
    
    error = tree_dump(&tree, VER_INIT, false, "Test dump empty tree");

    #ifdef VERIFY_DEBUG
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");
    #endif
    
    #ifdef VERIFY_DEBUG
    if (tree.dump_file != nullptr) {
        fclose(tree.dump_file);
        tree.dump_file = nullptr;
    }
    #endif
    
    tree_destroy(&tree);
    LOGGER_INFO("Тест пройден: дамп пустого дерева\n");
}

static void test_dump_tree_with_nodes() {
    LOGGER_INFO("=== Тест: дамп дерева с узлами ===");
    
    tree_t tree = {};
    stack_t stack = {};
    error_code error = tree_init(&tree, &stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");
    
    value_t root_value = make_union(CONSTANT, (const_val_type)42);
    tree_node_t* root = tree_init_root(&tree, CONSTANT, root_value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    #ifdef VERIFY_DEBUG
    tree.dump_file = fopen("test_dump_nodes.html", "w");
    HARD_ASSERT(tree.dump_file != nullptr, "failed to create dump file");
    #endif
    
    error = tree_dump(&tree, VER_INIT, true, "Test dump tree with nodes");

    #ifdef VERIFY_DEBUG
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");
    #endif
    
    #ifdef VERIFY_DEBUG
    if (tree.dump_file != nullptr) {
        fclose(tree.dump_file);
        tree.dump_file = nullptr;
    }
    #endif
    
    tree_destroy(&tree);
    LOGGER_INFO("Тест пройден: дамп дерева с узлами\n");
}

//================================================================================

int run_tests() {
    LOGGER_INFO("========================================\n");
    LOGGER_INFO("Начало тестирования функций чтения/записи и дампа\n");
    LOGGER_INFO("========================================\n\n");
    
    test_read_empty_tree();
    test_read_single_constant();
    test_read_single_variable();
    test_read_complex_tree();
    test_read_nonexistent_file();
    
    test_write_empty_tree();
    test_write_constant_tree();
    test_write_variable_tree();
    test_write_complex_tree();
    
    test_dump_empty_tree();
    test_DSL();
    LOGGER_INFO("========================================\n");
    LOGGER_INFO("Все тесты успешно пройдены!\n");
    LOGGER_INFO("========================================\n");


    


    return 0;
}

