#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "error_handler.h"
#include "tree_operations.h"
#include "tree_verification.h"

static error_code check_str_equal(const char* a, const char* b, const char* what) {
    if (a == nullptr || b == nullptr) {
        LOGGER_ERROR("check_str_equal(%s): one or both strings are nullptr (a=%p, b=%p)", what, (const void*)a, (const void*)b);
        return ERROR_NULL_ARG;
    }
    if (strcmp(a, b) != 0) {
        LOGGER_ERROR("check_str_equal(%s): mismatch: '%s' vs '%s'", what, a, b);
        return ERROR_INCORRECT_ARGS;
    }
    return ERROR_NO;
}

static error_code test_basic_build(tree_t* tree) {
    error_code error = ERROR_NO;

    tree_node_t* root = tree_set_root(tree, "Lobby");
    if (root == nullptr) {
        LOGGER_ERROR("test_basic_build: root is nullptr");
        return ERROR_MEM_ALLOC;
    }

    tree_node_t* left  = tree_insert_left(tree,  root, "Reception");
    tree_node_t* right = tree_insert_right(tree, root, "Restaurant");
    if (left == nullptr || right == nullptr) {
        LOGGER_ERROR("test_basic_build: failed to insert top-level children");
        return ERROR_INSERT_FAIL;
    }

    tree_node_t* l1 = tree_insert_left(tree,  left,  "Check-in desk");
    tree_node_t* l2 = tree_insert_right(tree, left,  "Luggage room");
    tree_node_t* r1 = tree_insert_left(tree,  right, "Buffet");
    tree_node_t* r2 = tree_insert_right(tree, right, "Kitchen");

    if (!l1 || !l2 || !r1 || !r2) {
        LOGGER_ERROR("test_basic_build: failed to insert second level");
        return ERROR_INSERT_FAIL;
    }

    error |= check_str_equal(root->value, "Lobby", "root");
    error |= check_str_equal(left->value, "Reception", "left");
    error |= check_str_equal(right->value, "Restaurant", "right");
    error |= check_str_equal(l1->value, "Check-in desk", "l1");
    error |= check_str_equal(l2->value, "Luggage room", "l2");
    error |= check_str_equal(r1->value, "Buffet", "r1");
    error |= check_str_equal(r2->value, "Kitchen", "r2");

    RETURN_IF_ERROR(error);
    return ERROR_NO;
}

static error_code test_replace_and_find(tree_t* tree) {
    error_code error = ERROR_NO;

    tree_node_t* found = tree_find_preorder(tree->root, "Reception");
    if (found == nullptr) {
        LOGGER_ERROR("test_replace_and_find: 'Reception' not found");
        return ERROR_MISSED_ELEM;
    }

    error |= tree_replace_value(found, "Front desk");
    RETURN_IF_ERROR(error);

    tree_node_t* again = tree_find_preorder(tree->root, "Front desk");
    if (again == nullptr || again != found) {
        LOGGER_ERROR("test_replace_and_find: updated node not found or pointer changed");
        return ERROR_MISSED_ELEM;
    }

    return ERROR_NO;
}

static error_code test_remove_subtree(tree_t* tree) {
    error_code error = ERROR_NO;

    tree_node_t* restaurant = tree_find_preorder(tree->root, "Restaurant");
    if (restaurant == nullptr) {
        LOGGER_ERROR("test_remove_subtree: 'Restaurant' not found");
        return ERROR_MISSED_ELEM;
    }

    error |= tree_remove_subtree(tree, restaurant);
    RETURN_IF_ERROR(error);

    tree_node_t* should_be_null = tree_find_preorder(tree->root, "Restaurant");
    if (should_be_null != nullptr) {
        LOGGER_ERROR("test_remove_subtree: 'Restaurant' still present after removal");
        return ERROR_INVALID_STRUCTURE;
    }

    return ERROR_NO;
}

int main(int, char**) {
    logger_initialize_stream(nullptr);
    LOGGER_INFO("Tree tests started");

    error_code error = ERROR_NO;

    tree_t tree = {};
    error |= tree_init(&tree ON_DEBUG(, VER_INIT));
    RETURN_IF_ERROR(error, /* no-op */);

#ifdef VERIFY_DEBUG
    tree.dump_file = fopen("tree_dump.html", "w");
    if (tree.dump_file == nullptr) {
        LOGGER_ERROR("main: failed to open dump file");
        error |= ERROR_OPEN_FILE;
    }
#endif

    if (error == ERROR_NO) {
        error |= test_basic_build(&tree);
    }

#ifdef VERIFY_DEBUG
    if (error == ERROR_NO) {
        error |= tree_verify(&tree, VER_INIT, TREE_DUMP_IMG, "After basic build");
    }
#endif

    if (error == ERROR_NO) {
        error |= test_replace_and_find(&tree);
    }

#ifdef VERIFY_DEBUG
    if (error == ERROR_NO) {
        error |= tree_verify(&tree, VER_INIT, TREE_DUMP_IMG, "After replace and find");
    }
#endif

    if (error == ERROR_NO) {
        error |= test_remove_subtree(&tree);
    }

#ifdef VERIFY_DEBUG
    if (error == ERROR_NO) {
        error |= tree_verify(&tree, VER_INIT, TREE_DUMP_IMG, "After subtree removal");
    }
#endif

    error |= tree_destroy(&tree);

#ifdef VERIFY_DEBUG
    if (tree.dump_file != nullptr) {
        fclose(tree.dump_file);
        tree.dump_file = nullptr;
    }
#endif

    LOGGER_INFO("Tree tests finished, error=%ld", error);
    return (error == ERROR_NO) ? EXIT_SUCCESS : EXIT_FAILURE;
}


#include <string.h>
#include "logger.h"
#include "error_handler.h"
#include "handle_input.h"
#include "list_operations.h"
#include "list_verification.h"

static error_code choose_input_type_and_process(list_t* list, int argc, char* argv[]);

int main(int argc, char* argv[]) {
    logger_initialize_stream(nullptr);
    LOGGER_DEBUG("Program started");
    error_code error = 0;

    list_t list = {};
    error |= list_init(&list, 5 ON_DEBUG(, VER_INIT));

    error |= choose_input_type_and_process(&list, argc, argv);

    error |= list_dest(&list);
    LOGGER_DEBUG("programm ended");
    LOGGER_DEBUG("ERROR_CODE=%ld", (error));
    return 0;
}

static error_code choose_input_type_and_process(list_t* list, int argc, char* argv[]) {
    error_code error = 0;

    if(argc < 2) {
        LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
        return ERROR_INCORRECT_ARGS;
    }
    ON_DEBUG(
        if(argc < 3) {
            LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
            return ERROR_INCORRECT_ARGS;
        }
    )
    if(strcmp(argv[1], "-ic") == 0) {
        ON_DEBUG(
            list->dump_file = fopen(argv[2], "w");
            if(list->dump_file == nullptr) {
                LOGGER_ERROR("Wrong file name");
                return ERROR_OPEN_FILE;
            }
        )
        error |= handle_interactive_console_input(list);
        ON_DEBUG(
            fclose(list->dump_file);
        )
        return error;

    } else if(strcmp(argv[1], "-if") == 0) {
        ON_DEBUG(
            list->dump_file = fopen(argv[2], "w");
            if(list->dump_file == nullptr) {
                LOGGER_ERROR("Wrong file name");
                return ERROR_OPEN_FILE;
            }
        )
        error |= handle_interactive_file_input(list);
        ON_DEBUG(
            fclose(list->dump_file);
        )
        return error;

    } else if(strcmp(argv[1], "-f")  == 0) {
        if(argc < 4) {
            LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
            return ERROR_INCORRECT_ARGS;
        }
        ON_DEBUG(
            if(argc < 5) {
                LOGGER_ERROR("FEW_ARGUMENTS %d", argc);
                return ERROR_INCORRECT_ARGS;
            }
            list->dump_file = fopen(argv[4], "w");
            if(list->dump_file == nullptr) {
                LOGGER_ERROR("Wrong file name");
                return ERROR_OPEN_FILE;
            }
        )
        error |= handle_file_input(list, argv[2], argv[3]);


        list_dump(list, VER_INIT, false, "Clear dump"); //todo: не вставлять

        int insert_idx1 = 0;
        int insert_idx2 = 1;
        int insert_idx3 = 2;
        list_insert_after(list, insert_idx1, 10.0);
        list_insert_after(list, insert_idx2, 20.0);
        list_insert_after(list, insert_idx3, 30.0);
        list_dump(list, VER_INIT, true, "AFTER inserts at idx1: %d, idx2: %d, idx3: %d", insert_idx1, insert_idx2, insert_idx3);

        
        list_linearize(list);
        list_dump(list, VER_INIT, true, "After linearize");

        int insert_idx = 1;
        list_insert_after(list, insert_idx, 40.0);
        list_dump(list, VER_INIT, true, "AFTER insert index %d", insert_idx);

        list_shrink_to_fit(list, false);
        list_dump(list, VER_INIT, true, "After shrink_to_fit");


        list_remove(list, 5);
        list_remove(list, 6);
        list_remove(list, 1);
        list_dump(list, VER_INIT, true, "After removes");
        list_insert_after(list, 11, 15.0);
        list_insert_after(list, 11, 22.0);
        list->arr[3].prev = 150; // to trigger error in dump
        list_dump(list, VER_INIT, true, "Corrupted dump");

        list->arr[8].next = 5;
        list_insert_auto(list, 1, 42.0);
        
        list->arr[3].next = 150;
        list_dump(list, VER_INIT, true, "Corrupted dump 2");

        list->arr[0].next = 3;
        list_dump(list, VER_INIT, true, "Corrupted dump 3");

        list->arr[list->capacity].val = 0;
        list_remove(list, 1);


//Замер времени изолировать на 1 ядре
//Опциии компилятора
//Замер времени, теммпературы.
//Сравнение отдельных операций
// README

//



/*
        list_insert_after(list, 0, 55.0);
        list_insert_after(list, 0, 65.0);
        list_insert_after(list, 0, 75.0);
        list_remove(list, 2);
        list_remove_auto(list, 4);
        list_remove(list, 3);
        list_dump(list, VER_INIT, true, "After more operations");

        list->arr[11].prev = 150;
        list_dump(list, VER_INIT, true, "Corrupted dump free list");
*/
        ON_DEBUG(
            fclose(list->dump_file);
        )
        return error;

    } else {
        LOGGER_ERROR("Unknown arg: %s", argv[1]);
        return ERROR_INCORRECT_ARGS;
    }
}

