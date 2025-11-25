#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>

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
#include "debug_meta.h"

//================================================================================

static void test_read_empty_tree() {
    LOGGER_INFO("=== Тест: чтение пустого дерева ===");
    
    error_code error = ERROR_NO;

    const char* filename = "empty_tree_test.tree";
    FILE* test_file = fopen(filename, "w");
    HARD_ASSERT(test_file != nullptr, "failed to create test file");
    fclose(test_file);

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    error = forest_read_file(&forest, filename);
    HARD_ASSERT(error == ERROR_NO, "forest_read_file failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");

    error = tree_parse_from_buffer(test_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_parse_from_buffer failed");
    HARD_ASSERT(test_tree->root == nullptr, "root should be nullptr for empty tree");
    HARD_ASSERT(test_tree->size == 0, "size should be 0 for empty tree");

    error = tree_write_to_file(test_tree, "Empty_tree_test1.tree");
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");

    forest_dest(&forest);

    LOGGER_INFO("Тест пройден: чтение пустого дерева\n");
}

static void test_read_single_constant() {
    LOGGER_INFO("=== Тест: чтение дерева с одной константой ===");
    
    error_code error = ERROR_NO;

    const char* filename = "Single_tree_test.tree";
    FILE* test_file = fopen(filename, "w");
    HARD_ASSERT(test_file != nullptr, "failed to create test file");
    fprintf(test_file, "(42 nil nil)");
    fclose(test_file);

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    error = forest_read_file(&forest, filename);
    HARD_ASSERT(error == ERROR_NO, "forest_read_file failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");

    error = tree_parse_from_buffer(test_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_parse_from_buffer failed");
    HARD_ASSERT(test_tree->root != nullptr, "root should not be nullptr for single constant tree");
    HARD_ASSERT(test_tree->root->type == CONSTANT, "type should be CONSTANT");
    HARD_ASSERT(test_tree->root->value.constant == 42, "value should be 42");
    HARD_ASSERT(test_tree->size == 1, "size should be 1 for single constant tree");

    error = tree_write_to_file(test_tree, "single_tree_test1.tree");
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");

    forest_dest(&forest);

    LOGGER_INFO("Тест пройден: чтение дерева с одной константой\n");
}

static void test_read_single_variable() {
    LOGGER_INFO("=== Тест: чтение дерева с одной переменной ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    const char* filename = "test_variable.tree";
    FILE* file = fopen(filename, "w");
    HARD_ASSERT(file != nullptr, "failed to create test file");
    fprintf(file, "(\"x\" nil nil)");
    fclose(file);
    
    error = forest_read_file(&forest, filename);
    HARD_ASSERT(error == ERROR_NO, "forest_read_file failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    
    error = tree_parse_from_buffer(test_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_parse_from_buffer failed");
    HARD_ASSERT(test_tree->root != nullptr, "root should not be nullptr");
    HARD_ASSERT(test_tree->root->type == VARIABLE, "node type should be VARIABLE");

    HARD_ASSERT(my_ssstrcmp(test_tree->var_stack->data[test_tree->root->value.var_idx].str, {"x", strlen("x")}) == 0, "variable name should be 'x'");
    HARD_ASSERT(test_tree->size == 1, "size should be 1");

    error = tree_write_to_file(test_tree, "test_variable_write.tree");
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");

    forest_dest(&forest);
    LOGGER_INFO("Тест пройден: чтение дерева с одной переменной\n");
}

static void test_DSL() {
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t tree_origin = {};
    error = tree_init(&tree_origin, forest.var_stack ON_DEBUG(, VER_INIT));
    HARD_ASSERT(error == ERROR_NO, "tree_init failed");

    tree_t* tree = &tree_origin;
    tree_origin.root = ADD_(SIN_(c(1)), c(2));

    tree = forest_include_tree(&forest, &tree_origin, &error);
    HARD_ASSERT(error == ERROR_NO, "forest_include_tree failed");

    #ifdef VERIFY_DEBUG
    forest_open_dump_file(&forest, "test_dump_DSL.html");
    HARD_ASSERT(forest.dump_file != nullptr, "failed to create file");
    #endif

    error = tree_dump(tree, VER_INIT, true, "FF");
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");

    #ifdef VERIFY_DEBUG
    forest_close_dump_file(&forest);
    #endif

    forest_dest(&forest);
}

static void test_read_complex_tree() {
    LOGGER_INFO("=== Тест: чтение сложного дерева ===");
    
    error_code error = ERROR_NO;

    const char* filename = "test_complex.tree";
    FILE* test_file = fopen(filename, "w");
    HARD_ASSERT(test_file != nullptr, "failed to create test file");
    fprintf(test_file, "(+ (10 nil nil) (\"x\" nil nil))");
    fclose(test_file);

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    ON_DEBUG(
    error_code dump_error = forest_open_dump_file(&forest, "test_dump_nodes.html");
    HARD_ASSERT(dump_error == ERROR_NO, "forest_open_dump_file failed");
    HARD_ASSERT(forest.dump_file != nullptr, "failed to create dump file");
    )
    

    error = forest_read_file(&forest, filename);
    HARD_ASSERT(error == ERROR_NO, "forest_read_file failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");

    error = tree_parse_from_buffer(test_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_parse_from_buffer failed");
    HARD_ASSERT(test_tree->root != nullptr, "root should not be nullptr");
    HARD_ASSERT(test_tree->root->type == FUNCTION, "root type should be FUNCTION");
    HARD_ASSERT(test_tree->root->value.func == ADD, "root function should be ADD");
    HARD_ASSERT(test_tree->root->left != nullptr, "left child should not be nullptr");
    HARD_ASSERT(test_tree->root->right != nullptr, "right child should not be nullptr");
    HARD_ASSERT(test_tree->root->left->type == CONSTANT, "left child should be CONSTANT");
    HARD_ASSERT(test_tree->root->left->value.constant == 10, "left child value should be 10");
    HARD_ASSERT(test_tree->root->right->type == VARIABLE, "right child should be VARIABLE");
    HARD_ASSERT(test_tree->size == 3, "size should be 3");

    error = tree_dump(test_tree, VER_INIT, true, "Test dump tree with nodes");
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");

    tree_t* tree_diff = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree for diff failed");

    LOGGER_DEBUG("Diff started");

    tree_node_t* new_root = get_diff(test_tree, test_tree->root);
    tree_change_root(tree_diff, new_root);

    LOGGER_DEBUG("Diff ended");

    error = tree_dump(tree_diff, VER_INIT, true, "After diff");
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");
    
    #ifdef VERIFY_DEBUG
    forest_close_dump_file(&forest);
    #endif

    forest_dest(&forest);
    LOGGER_INFO("Тест пройден: чтение сложного дерева\n");
}

static void test_read_nonexistent_file() {
    LOGGER_INFO("=== Тест: чтение несуществующего файла ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");
    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    error = forest_read_file(&forest, "nonexistent_file.tree");
    HARD_ASSERT(error & ERROR_OPEN_FILE, "should return ERROR_OPEN_FILE");
    forest_dest(&forest);
    LOGGER_INFO("Тест пройден: чтение несуществующего файла\n");
}

//================================================================================

static void test_write_empty_tree() {
    LOGGER_INFO("=== Тест: запись пустого дерева ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    HARD_ASSERT(test_tree->root == nullptr, "tree should be empty");
    
    const char* filename = "test_write_empty.tree";
    error = tree_write_to_file(test_tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    
    FILE* file = fopen(filename, "r");
    HARD_ASSERT(file != nullptr, "output file should exist");
    char buffer[100] = {};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    char* ptr = buffer;
    while (*ptr != '\0' && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    HARD_ASSERT(*ptr == '\0', "file should be empty or contain only whitespace");
    
    forest_dest(&forest);
    LOGGER_INFO("Тест пройден: запись пустого дерева\n");
}

static void test_write_constant_tree() {
    LOGGER_INFO("=== Тест: запись дерева с константой ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    
    value_t value = make_union(CONSTANT, (const_val_type)100);
    tree_node_t* root = tree_init_root(test_tree, CONSTANT, value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    const char* filename = "test_write_constant.tree";
    error = tree_write_to_file(test_tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    
    forest_t read_forest = {};
    error |= forest_init(&read_forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    error = forest_read_file(&read_forest, filename);
    HARD_ASSERT(error == ERROR_NO, "forest_read_file failed");

    tree_t* read_tree = forest_add_tree(&read_forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    
    error = tree_parse_from_buffer(read_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_parse_from_buffer failed");
    HARD_ASSERT(read_tree->root != nullptr, "root should not be nullptr");
    HARD_ASSERT(read_tree->root->type == CONSTANT, "type should be CONSTANT");
    HARD_ASSERT(read_tree->root->value.constant == 100, "value should be 100");
    
    forest_dest(&forest);
    forest_dest(&read_forest);
    LOGGER_INFO("Тест пройден: запись дерева с константой\n");
}

static void test_write_variable_tree() {
    LOGGER_INFO("=== Тест: запись дерева с переменной ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    
    size_t idx = add_var({"test_var", strlen("test_var")}, 0, forest.var_stack, &error);
    HARD_ASSERT(error == ERROR_NO, "add_var failed");
    
    value_t value = make_union(VARIABLE, idx);
    tree_node_t* root = tree_init_root(test_tree, VARIABLE, value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    const char* filename = "test_write_variable.tree";
    error = tree_write_to_file(test_tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    
    forest_t read_forest = {};
    error |= forest_init(&read_forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    error = forest_read_file(&read_forest, filename);
    HARD_ASSERT(error == ERROR_NO, "forest_read_file failed");

    tree_t* read_tree = forest_add_tree(&read_forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");

    error = tree_parse_from_buffer(read_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_parse_from_buffer failed");
    HARD_ASSERT(read_tree->root != nullptr, "root should not be nullptr");
    HARD_ASSERT(read_tree->root->type == VARIABLE, "type should be VARIABLE");
    HARD_ASSERT(my_scstrcmp(read_tree->var_stack->data[read_tree->root->value.var_idx].str, "test_var") == 0, "variable name should match");
    
    forest_dest(&forest);
    forest_dest(&read_forest);
    LOGGER_INFO("Тест пройден: запись дерева с переменной\n");
}

static void test_write_complex_tree() {
    LOGGER_INFO("=== Тест: запись сложного дерева ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    
    value_t root_value = make_union(FUNCTION, ADD);
    tree_node_t* root = tree_init_root(test_tree, FUNCTION, root_value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    value_t left_value = make_union(CONSTANT, (const_val_type)10);
    tree_node_t* left = tree_insert_left(test_tree, CONSTANT, left_value, root);
    HARD_ASSERT(left != nullptr, "tree_insert_left failed");
    
    size_t idx = add_var({"y", 1}, 0, forest.var_stack, nullptr);
    value_t right_value = make_union(VARIABLE, idx);
    tree_node_t* right = tree_insert_right(test_tree, VARIABLE, right_value, root);
    HARD_ASSERT(right != nullptr, "tree_insert_right failed");
    
    const char* filename = "test_write_complex.tree";
    error = tree_write_to_file(test_tree, filename);
    HARD_ASSERT(error == ERROR_NO, "tree_write_to_file failed");
    tree_dump(test_tree, VER_INIT, true, "Aaa");
    
    forest_t read_forest = {};
    error |= forest_init(&read_forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    error = forest_read_file(&read_forest, filename);
    HARD_ASSERT(error == ERROR_NO, "forest_read_file failed");

    tree_t* read_tree = forest_add_tree(&read_forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");

    error = tree_parse_from_buffer(read_tree);
    HARD_ASSERT(error == ERROR_NO, "tree_parse_from_buffer failed");
    HARD_ASSERT(read_tree->root != nullptr, "root should not be nullptr");
    HARD_ASSERT(read_tree->root->type == FUNCTION, "root type should be FUNCTION");
    HARD_ASSERT(read_tree->size == 3, "size should be 3");
    
    forest_dest(&forest);
    forest_dest(&read_forest);
    LOGGER_INFO("Тест пройден: запись сложного дерева\n");
}

//================================================================================

static void test_dump_empty_tree() {
    LOGGER_INFO("=== Тест: дамп пустого дерева ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    
    #ifdef VERIFY_DEBUG
    forest_open_dump_file(&forest, "test_dump_empty.html");
    HARD_ASSERT(forest.dump_file != nullptr, "failed to create dump file");
    #endif
    
    error = tree_dump(test_tree, VER_INIT, false, "Test dump empty tree");
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");

    #ifdef VERIFY_DEBUG
    forest_close_dump_file(&forest);
    #endif
    
    forest_dest(&forest);
    LOGGER_INFO("Тест пройден: дамп пустого дерева\n");
}

static void test_dump_tree_with_nodes() {
    LOGGER_INFO("=== Тест: дамп дерева с узлами ===");
    
    error_code error = ERROR_NO;

    forest_t forest = {};
    error |= forest_init(&forest, VER_INIT);
    HARD_ASSERT(error == ERROR_NO, "forest_init failed");

    tree_t* test_tree = forest_add_tree(&forest, &error);
    HARD_ASSERT(error == ERROR_NO, "add_tree failed");
    
    value_t root_value = make_union(CONSTANT, (const_val_type)42);
    tree_node_t* root = tree_init_root(test_tree, CONSTANT, root_value);
    HARD_ASSERT(root != nullptr, "tree_init_root failed");
    
    #ifdef VERIFY_DEBUG
    forest_open_dump_file(&forest, "test_dump_nodes.html");
    HARD_ASSERT(forest.dump_file != nullptr, "failed to create dump file");
    #endif
    
    error = tree_dump(test_tree, VER_INIT, true, "Test dump tree with nodes");
    HARD_ASSERT(error == ERROR_NO, "tree_dump failed");

    #ifdef VERIFY_DEBUG
    forest_close_dump_file(&forest);
    #endif
    
    forest_dest(&forest);
    LOGGER_INFO("Тест пройден: дамп дерева с узлами\n");
}

//================================================================================

int run_tests() {
    LOGGER_INFO("========================================\n");
    LOGGER_INFO("Начало тестирования функций чтения/записи и дампа\n");
    LOGGER_INFO("========================================\n");
    
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

