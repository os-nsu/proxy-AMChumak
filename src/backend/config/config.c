#include "../../include/config.h"

int is_config_initialized = 0;

int create_config_table(void) {
    if (is_config_initialized)
        return -1;
    is_config_initialized = 1;
    return 0;
}

int destroy_config_table(void) {
    if (!is_config_initialized)
        return -1;
    is_config_initialized = 0;
    return 0;
}