/*!
    \file config.h
    \brief Config static library interface
    \version 1.0
    \date 15 Jan 2025

    This file contains config's function signatures and type definitions.

    Config is the singleton. Therefore double create_config_table must return
   -1.

    Configuration file it is set of key-value pair in format: "key = value" (the
   number of spaces and their position of non-significant ones can be any)

    key must consists of this symbols: A-Z, a-z, 0-9, _, - and it length must be
   <= 127

    value must be one of this types:
        INTEGER - signed integer with size 64 bits
        REAL - signed floating point number with double precision
        STRING - c-style const string
    and array of it in format: [value, value, ..., value]. Array can contains
   only values with same type.

    Also config string can constains one-line comment starting with "#".

    Default configuration file name: proxy.conf, it is stored in project's root
   directory.

    TODO error handling and include command for config file
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/*!
    This type describes content of config values.
    It can be interpretred like single value or array of values.
*/
typedef union ConfigData {
    int64_t* integer;
    double* real;
    char** string;
} ConfigData;

/*!
    This enum describes type of config variable
*/
typedef enum ConfigVarType {
    UNDEFINED = 0, // Special type. It will be returned if some error occurred.
    INTEGER = 1,
    REAL,
    STRING
} ConfigVarType;

/*!
    This type describes config variables
*/
typedef struct ConfigVariable {
    char* name;
    char* description;
    ConfigData data; // variable content. It can be single or array. You should
                     // look at count filed to determine it.
    ConfigVarType type; // variable type. If it is UNDEFINED, then error
                        // occurred and another fields are meanless
    int count; // count items in array. If it is one, then variable is single.
} ConfigVariable;

/*!
    Create config table. It should be called once.
    \return -1 if table already exists or 0 if all is OK
*/
int create_config_table(void);

void destroy_variable(ConfigVariable *var);

/*!
    Destroy config table and frees all resources associated with it. It should
   be called once.
   \return -1 if table no exists or 0 if all is OK
*/
int destroy_config_table(void);

/*!
    Parse configuration file located on path.
    \param [in] path path to configuration file. It must not be NULL.
    \return -1 if some error occurred or 0 if all is OK.
*/
int parse_config(const char* path);

/*!
    Define variable from code.
    \param [in] variable ConfigVariable struct with initial values
    \return -1 if variable already exists,
            -2 if some error occurred,
            0 if variable created successfully
*/
int define_variable(const ConfigVariable variable);

/*!
    Get variable from config.
    \param [in] name name of variable
    \return ConfigVariable struct if all is OK and ConfigVariable struct with
   type UNDEFINED if some error occurred
*/
ConfigVariable get_variable(const char* name);

/*!
    Define variable if does not exists or set it if it already exists.
    \param [in] variable ConfigVariable struct with initial values
    \return -1 if some error occurred or 0 if all is OK
*/
int set_variable(const ConfigVariable variable);

/*!
    Determine if variable exists in config
    \param [in] name name of variable
    \return true if variable exists and false if not
*/
bool does_variable_exist(const char* name);


#endif  //CONFIG_H