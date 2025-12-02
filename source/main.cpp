#include "logger.h"
#include "tree_operations.h"
#include "tree_verification.h"
#include "void_stack.h"

int run_tests();

int main() {
    logger_initialize_stream(stderr);
    void_stack_t void_stack = {};
    run_tests();
    
    logger_close();
    return 0;
}