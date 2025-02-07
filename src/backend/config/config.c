#include "../../include/config.h"
#include "../../include/logger.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// STRUCTURES

/*!
    \struct ConfigNode
    \brief Structure that describes node in table
*/
typedef struct ConfigNode {
    struct ConfigNode *next;
    ConfigVariable variable;
} ConfigNode;

/*!
    \struct GroupsTableData
    \brief table of parmeter groups

    Hash table of configuration parameters.
*/
typedef struct ConfigTable {
    struct ConfigNode **table; // array of hash baskets
    int size;                  // size of table
    int cur_count;             ///< current total count of parameters
    int is_allocated; ///< flag means that buffer table allocated and exists
} ConfigTable;

ConfigTable g_config = {NULL, 0, 0,
                        0}; ///< GLOBAL MAIN CONFIGURATION ACCESS TABLE

// FUNCTION DECLARATIONS

unsigned long hash_string(const char *str);

int create_config_table(void);

void destroy_variable(ConfigVariable *var);

int destroy_config_table(void);

ConfigVariable copy_variable(const ConfigVariable var);

ConfigVariable get_variable(const char *name);

int set_variable(const ConfigVariable variable);

bool does_variable_exist(const char *name);

/*!
    \brief hash string
    Now algorithm is sdbm because this was used
    in real data base and worked fine
    \param[in] str value
    \return hash that is unsigned long
*/
unsigned long hash_string(const char *str) {
    unsigned long hash = 0;
    int cur_sym;

    while ((cur_sym = *str++)) {
        hash = cur_sym + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

int create_config_table(void) {
    if (g_config.is_allocated) {
        return -1;
    }

    int size = 100;
    g_config.table = (ConfigNode **)calloc(size, sizeof(ConfigNode *));
    g_config.size = 100;
    g_config.cur_count = 0;
    g_config.is_allocated = 1;
    return 0;
}

void destroy_variable(ConfigVariable *var) {
    free(var->name);
    var->name = NULL;
    free(var->description);
    var->description = NULL;
    switch (var->type) {
    case UNDEFINED: {
        free((void *)var->data.integer);
        break;
    }
    case STRING: {
        for (int i = 0; i < var->count; i++) {
            free(var->data.string[i]);
        }
        free(var->data.string);
        break;
    }
    case INTEGER: {
        free(var->data.integer);
        break;
    }
    case REAL: {
        free(var->data.real);
        break;
    }
    }
    var->count = 0;
    var->data = (ConfigData)(int64_t *)NULL;
    var->type = UNDEFINED;
}

int destroy_config_table(void) {
    if (!g_config.is_allocated)
        return -1;

    for (int i = 0; i < g_config.size; i++) {
        ConfigNode *cur_node = g_config.table[i], *del_node;
        while (cur_node) {
            del_node = cur_node;
            cur_node = cur_node->next;
            destroy_variable(&del_node->variable);
            free(del_node);
        }
    }
    free(g_config.table);
    g_config.table = NULL;
    g_config.size = 0;
    g_config.cur_count = 0;
    g_config.is_allocated = 0;
    return 0;
}

ConfigVariable copy_variable(const ConfigVariable var) {
    ConfigVariable result;
    if (var.name) {
        result.name = (char *)calloc(strlen(var.name) + 1, sizeof(char));
        strcpy(result.name, var.name);
    } else {
        result.name = NULL;
    }

    if (var.description) {
        result.description =
            (char *)calloc(strlen(var.description) + 1, sizeof(char));
        strcpy(result.description, var.description);
    } else {
        result.description = NULL;
    }

    result.count = var.count;
    result.type = var.type;

    if (!var.data.integer) {
        result.data.integer = NULL;
        return result;
    }

    switch (result.type) {

    case UNDEFINED: {
        result.data = (ConfigData)(int64_t *)NULL;
        break;
    }
    case INTEGER: {
        result.data =
            (ConfigData)(int64_t *)malloc(result.count * sizeof(int64_t));
        memcpy(result.data.integer, var.data.integer,
               result.count * sizeof(int64_t));
        break;
    }
    case REAL: {
        result.data =
            (ConfigData)(double *)malloc(result.count * sizeof(double));
        memcpy(result.data.real, var.data.real, result.count * sizeof(double));
        break;
    }
    case STRING: {
        result.data =
            (ConfigData)(char **)malloc(result.count * sizeof(char *));
        for (int i = 0; i < result.count; i++) {
            result.data.string[i] =
                (char *)calloc(strlen(var.data.string[i]) + 1, sizeof(char));
            strcpy(result.data.string[i], var.data.string[i]);
        }
        break;
    }
    }
    return result;
}

int define_variable(const ConfigVariable variable) {
    write_log(STDERR, LOG_DEBUG, "config.c", __LINE__, "hashed name is %s %d",
              variable.name, g_config.size);
    unsigned long hash = hash_string(variable.name) % g_config.size;

    // check and push
    write_log(STDERR, LOG_DEBUG, "config.c", __LINE__, "start saving variable");
    ConfigNode *prev_node = g_config.table[hash];
    write_log(STDERR, LOG_DEBUG, "config.c", __LINE__, "checked hash table");
    if (!prev_node) {
        g_config.table[hash] = malloc(sizeof(ConfigNode));
        g_config.table[hash]->next = NULL;
        g_config.table[hash]->variable = copy_variable(variable);
        return 0;
    }
    write_log(STDERR, LOG_DEBUG, "config.c", __LINE__, "search place");
    while (prev_node->next) {
        if (!strcmp(prev_node->variable.name, variable.name))
            return -1;
    }
    if (!strcmp(prev_node->variable.name, variable.name))
            return -1;
    write_log(STDERR, LOG_DEBUG, "config.c", __LINE__, "found place");
    ConfigNode *new_node = malloc(sizeof(ConfigNode));
    new_node->next = NULL;
    new_node->variable = copy_variable(variable);
    prev_node->next = new_node;

    return 0;
}

ConfigVariable get_variable(const char *name) {
    unsigned long hash = hash_string(name) % g_config.size;

    ConfigNode *cur_node = g_config.table[hash];
    ConfigVariable result = {NULL, NULL, NULL, UNDEFINED, 0};

    while (cur_node) {
        if (!strcmp(cur_node->variable.name, name)) {
            result = copy_variable(cur_node->variable);
            return result;
        }
    }
    return result;
}

int set_variable(const ConfigVariable variable) {
    unsigned long hash = hash_string(variable.name) % g_config.size;
    ConfigNode *prev_node = g_config.table[hash];
    if (!prev_node) {
        g_config.table[hash] = malloc(sizeof(ConfigNode));
        g_config.table[hash]->next = NULL;
        g_config.table[hash]->variable = copy_variable(variable);
        return 0;
    }

    while (prev_node->next) {
        if (!strcmp(prev_node->variable.name, variable.name))
            destroy_variable(&prev_node->variable);
        prev_node->variable = copy_variable(variable);
        return 0;
    }

    if (!strcmp(prev_node->variable.name, variable.name)) {
        prev_node->variable = copy_variable(variable);
    } else {
        ConfigNode *new_node = malloc(sizeof(ConfigNode));
        new_node->next = NULL;
        new_node->variable = copy_variable(variable);
        prev_node->next = new_node;
    }

    return 0;
}

bool does_variable_exist(const char *name) {
    unsigned long hash = hash_string(name) % g_config.size;

    ConfigNode *cur_node = g_config.table[hash];

    while (cur_node) {
        if (!strcmp(cur_node->variable.name, name))
            return true;
    }
    return false;
}
