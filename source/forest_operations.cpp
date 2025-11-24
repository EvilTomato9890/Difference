#include "error_handler.h"
#include "../libs/StackDead-main/error_handlers.h"
#include "tree_info.h"
#include "tree_verification.h"
#include "asserts.h"
#include "logger.h"
#include "../libs/List/include/list_operations.h"
#include "../libs/List/include/list_info.h"
#include "tree_operations.h"

error_code forest_init(forest_t* forest ON_DEBUG(, ver_info_t ver_info)) {
    HARD_ASSERT(forest     != nullptr, "Forest_ptr is nulltpr");

    LOGGER_DEBUG("forest_init: started");

    error_code error = ERROR_NO;


    stack_t* stack = (stack_t*)calloc(1, sizeof(stack_t));
    if(!stack) {
        LOGGER_ERROR("forest_init: calloc for stack failed");
        return ERROR_MEM_ALLOC;
    }
    error |= stack_init(stack, 1, VER_INIT);
    RETURN_IF_ERROR(error, free(stack););


    list_t*  list  = (list_t*) calloc(1, sizeof(list_t));
    if(!list) {
        LOGGER_ERROR("forest_init: calloc for list failed");
        free(stack);
        return ERROR_MEM_ALLOC;
    }
    error |= list_init(list, 1, VER_INIT);
    RETURN_IF_ERROR(error, free(stack); free(list););


    ON_DEBUG({
    forest->dump_file = nullptr;
    forest->ver_info  = ver_info;
    })

    forest->buff = nullptr;

}

ON_DEBUG(
error_code forest_open_dump_file(forest_t* forest, const char* dump_file_name) {
    HARD_ASSERT(forest     != nullptr, "Forest_ptr is nullptr");
    HARD_ASSERT(dump_file_name != nullptr, "Dump_file_name is nullptr");

    LOGGER_DEBUG("Forest_open_dump_file: started");

    FILE* dump_file = fopen(dump_file_name, "w");
    if(!dump_file) {
        LOGGER_ERROR("File open error");
        return ERROR_OPEN_FILE;
    }
    forest->dump_file = dump_file;
    return ERROR_NO;
})

tree_t* forest_add_tree(forest_t* forest, error_code* error_ptr) {
    HARD_ASSERT(forest != nullptr, "Forest is nullptr");
    HARD_ASSERT(error_ptr != nullptr, "Error is nullptr");
    HARD_ASSERT(forest->tree_list != nullptr, "Tree list is nullptr");

    LOGGER_DEBUG("Forest_add_tree: started");

    error_code error = ERROR_NO;

    tree_t* tree = (tree_t*)calloc(1, sizeof(tree_t));
    error |= tree_init(tree, forest->var_stack, VER_INIT);
    list_push_back(forest->tree_list, )
}

error_code forest_dest(forest_t* forest) {
    LOGGER_DEBUG("forest_dest: started");
    if(!forest) {
        LOGGER_WARNING("forest_dest: forest is nullptr");
        return ERROR_NO;
    }
    
    error_code error = ERROR_NO;

    error |= stack_destroy(forest->var_stack);
    forest->var_stack = nullptr;

    error |= list_dest(forest->tree_list);
    forest->tree_list = nullptr;

    free(forest->buff);

    free(forest);

}