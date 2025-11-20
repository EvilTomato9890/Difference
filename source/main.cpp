#include "logger.h"
#include "tree_operations.h"
#include "tree_verification.h"

int run_tests();

int main() {
    logger_initialize_stream(stderr);
    
    run_tests();
    
    logger_close();
    return 0;
}