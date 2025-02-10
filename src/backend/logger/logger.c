#include "../../include/logger.h"


int is_logger_initialized = 0;

int init_logger(char *path, long file_size_limit) {
    if (is_logger_initialized)
        return -1;
    is_logger_initialized = 1;
    return 0;
}

int fini_logger(void) {
    if (!is_logger_initialized)
        return -1;
    is_logger_initialized = 0;
    return 0;
}