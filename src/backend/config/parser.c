/*!
    \file parser.c
    \brief This file contains config parser implementation
    \version 1.0
    \date 15 Jul 2024

    Whole kernel configuration parameters belong to "kernel" group

    some information about config.conf:

    There are only string in format "key=value"

    key - string that consist only of english letters and symbols '_'.
    There are some command-keys (command-keys ended with '.'):
    group. - change group of parameters

    value could be single or array (array denoted by '{', '}', values in array
   separated by commas). Value could have one of three types: long long, double
   or string.

    There are

    sample.conf:

    max_size=42
    min_level=0.054 #comment-part
    group. = "system" #change group to "system"
    the_best_subjects=["math", "programming"]
    #that part also commented

    IDENTIFICATION
        src/backend/config/parser.c
*/

#include "../../include/config.h"
#include <complex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*PARSER IMPLEMENTATION*/

/*FUNCTION ADVERTISEMENTS*/
int parse_line(char *line, char **rkey, ConfigData *rvalues, int *rsize,
               ConfigVarType *rtype);

int parse_config(const char *path);

/*!
    \brief Parses 1 line from config

    Takes one line from config and parse it. line could be empty or commented.
    Parsing implemented with finite state machine. Rules for line syntax
    described in config.h description.

    \param[in] line Line of config that will be parsed
    \param[out] rkey Pointer to result key
    \param[out] rvalues Pointer to result array of values
    \param[out] rsize Pointer to result size of array
    \param[out] rtype Pointer to result type
    \return 0 if took key and value, -1 in error case, 2 in comment case, 3 if
   line is empty
*/
int parse_line(char *line, char **rkey, ConfigData *rvalues, int *rsize,
               ConfigVarType *rtype) {
    char *cur_sym_ptr = line;

    typedef enum ParsingStates {
        COMMENT = -3,
        ERROR = -2,
        FINISH = -1,
        START = 0,
        KEY,
        SPACES_AFTER_KEY,
        SPACES_BEFORE_VALUES,
        VALUE_DIGIT,
        VALUE_DOUBLE,
        VALUE_STRING,
        VALUE_STRING_SLASH,
        VALUE_ARRAY,
        NEXT_STRING,
        NEXT_STRING_SLASH,
        SPACES_AFTER_ARRAYS_STRING,
        SPACES_BEFORE_ARRAYS_STRING,
        NEXT_DIGIT,
        SPACES_AFTER_ARRAYS_LONG,
        SPACES_BEFORE_ARRAYS_LONG,
        NEXT_EXPECTED_LONG,
        NEXT_DOUBLE,
        SPACES_AFTER_ARRAYS_DOUBLE,
        SPACES_BEFORE_ARRAYS_DOUBLE,
        NEXT_EXPECTED_DOUBLE,
        NEXT_EXACTLY_DOUBLE
    } States;

    States cur_state = START;

    int begin_key_pos, end_key_pos;
    char *key_name = NULL;
    int begin_cur_val_pos, end_cur_val_pos;
    ConfigVarType type = 0;
    int cnt_slash = 0;
    ConfigData values;
    int size = 1;
    int count_values = 0;
    int cur_pos = 0;
    int break_loop = 0;

    /*USES FINITE STATE MACHINE TO PARSE LINE*/
    while (*cur_sym_ptr) {
        char sym = *cur_sym_ptr;
        switch (cur_state) {
        case COMMENT: {
            break_loop = 3;
            break;
        }
        case ERROR: {
            break_loop = 2;
            break;
        }
        case FINISH: {
            break_loop = 1;
            break;
        }
        case START: {
            if (sym == '\n' || sym == ' ' || sym == '\t') {
                cur_state = START;
            } else if (sym == '#') {
                cur_state = COMMENT;
            } else if ((sym >= 'A' && sym <= 'Z') || sym == '_' ||
                       (sym >= 'a' && sym <= 'z')) {
                begin_key_pos = cur_pos;
                cur_state = KEY;
            } else {
                cur_state = ERROR;
            }
            break;
        }
        case KEY: {
            if (sym == '.') {
                end_key_pos = cur_pos + 1;
                key_name = (char *)calloc((end_key_pos - begin_key_pos + 1),
                                          sizeof(char));
                for (int i = begin_key_pos; i < end_key_pos; ++i) {
                    key_name[i - begin_key_pos] = line[i];
                }
            } else if (sym == ' ') {
                end_key_pos = cur_pos;
                key_name = (char *)calloc((end_key_pos - begin_key_pos + 1),
                                          sizeof(char));
                for (int i = begin_key_pos; i < end_key_pos; ++i) {
                    key_name[i - begin_key_pos] = line[i];
                }
                cur_state = SPACES_AFTER_KEY;
            } else if (sym == '=') {
                end_key_pos = cur_pos;
                key_name = (char *)calloc((end_key_pos - begin_key_pos + 1),
                                          sizeof(char));
                for (int i = begin_key_pos; i < end_key_pos; ++i) {
                    key_name[i - begin_key_pos] = line[i];
                }
                cur_state = SPACES_BEFORE_VALUES;
            } else if ((sym < 'A' && sym != ' ' && sym != '.' && sym != '=') ||
                       (sym > 'Z' && sym < 'a' && sym != '_') || sym > 'z') {
                cur_state = ERROR;
            }

            break;
        }
        case SPACES_AFTER_KEY: {
            if (sym != ' ' && sym != '=') {
                cur_state = ERROR;
            } else if (sym == '=') {
                cur_state = SPACES_BEFORE_VALUES;
            }
            break;
        }
        case SPACES_BEFORE_VALUES: {
            if (!((sym >= '0' && sym <= '9') || sym == '\"' || sym == '[' ||
                  sym == ' ')) {
                cur_state = ERROR;
            } else if (sym != ' ') {
                if (sym >= '0' && sym <= '9') {
                    begin_cur_val_pos = cur_pos;
                    cur_state = VALUE_DIGIT;
                } else if (sym == '"') {
                    begin_cur_val_pos = cur_pos + 1;
                    cnt_slash = 0;
                    cur_state = VALUE_STRING;
                } else {
                    cur_state = VALUE_ARRAY;
                }
            }
            break;
        }
        case VALUE_DIGIT: {
            if (!((sym >= '0' && sym <= '9') || sym == '.' || sym == ' ')) {
                cur_state = ERROR;
            } else if (sym == '.') {
                if (*(cur_sym_ptr + 1) < '0' || *(cur_sym_ptr + 1) > '9') {
                    cur_state = ERROR;
                } else {
                    cur_state = VALUE_DOUBLE;
                }
            } else {
                end_cur_val_pos = cur_pos;
                type = INTEGER;
                /*DELETE LEFT NULLS*/
                while (line[begin_cur_val_pos] == '0' &&
                       begin_cur_val_pos < end_cur_val_pos - 1) {
                    begin_cur_val_pos++;
                }
                count_values++;
                values.integer =
                    realloc(values.integer, sizeof(long long) * count_values);
                values.integer[count_values - 1] =
                    strtoll(line + begin_cur_val_pos, NULL, 10);
                cur_state = FINISH;
            }
            break;
        }
        case VALUE_DOUBLE: {
            if (!((sym >= '0' && sym <= '9') || sym == ' ' || sym == '\t' ||
                  sym == '\n')) {
                cur_state = ERROR;
            } else if (sym == ' ' || sym == ' ' || sym == '\t' || sym == '\n') {
                type = REAL;
                end_cur_val_pos = cur_pos;
                count_values++;
                values.real =
                    realloc(values.integer, sizeof(double) * count_values);
                values.real[count_values - 1] =
                    strtod(line + begin_cur_val_pos, NULL);
                cur_state = FINISH;
            }
            break;
        }
        case VALUE_STRING: {
            if (sym == '"') {
                type = STRING;
                end_cur_val_pos = cur_pos;
                char *string_value = (char *)calloc(
                    (end_cur_val_pos - begin_cur_val_pos + 1 - cnt_slash),
                    sizeof(char));
                /*COPY STRING WITHOUT SLASHES*/
                int cnt_sym = 0;
                for (int i = 0; i < end_cur_val_pos - begin_cur_val_pos; ++i) {
                    if (line[begin_cur_val_pos + i] != '\\') {
                        string_value[cnt_sym] = line[begin_cur_val_pos + i];
                    } else {
                        string_value[cnt_sym] = line[begin_cur_val_pos + ++i];
                        // get next symbol after '\'
                    }
                    cnt_sym++;
                }
                count_values++;
                values.string = realloc(values.string, count_values);
                values.string[count_values] = string_value;

                cur_state = FINISH;
            } else if (sym == '\\') {
                cur_state = VALUE_STRING_SLASH;
            } else if (sym == '\n') {
                cur_state = ERROR;
            }
            break;
        }
        case VALUE_STRING_SLASH: {
            cnt_slash++;
            cur_state = VALUE_STRING;
            break;
        }
        case VALUE_ARRAY: {
            if (sym != ' ' && sym != '\"' && !(sym >= '0' && sym <= '9')) {
                cur_state = ERROR;
            } else if (sym >= '0' && sym <= '9') {
                begin_cur_val_pos = cur_pos;
                cur_state = NEXT_DIGIT;
            } else if (sym == '\"') {
                begin_cur_val_pos = cur_pos + 1;
                cnt_slash = 0;
                cur_state = NEXT_STRING;
            }
            break;
        }
        case NEXT_STRING: {
            if (sym == '"') {
                type = STRING;
                end_cur_val_pos = cur_pos;
                char *string_value = (char *)calloc(
                    (end_cur_val_pos - begin_cur_val_pos + 1 - cnt_slash),
                    sizeof(char));
                /*COPY STRING WITHOUT SLASHES*/
                int cnt_sym = 0;
                for (int i = 0; i < end_cur_val_pos - begin_cur_val_pos; ++i) {
                    if (line[begin_cur_val_pos + i] != '\\') {
                        string_value[cnt_sym] = line[begin_cur_val_pos + i];
                    } else {
                        string_value[cnt_sym] = line[begin_cur_val_pos + ++i];
                        // get next symbol after '\'
                    }
                    cnt_sym++;
                }
                count_values++;
                values.string =
                    realloc(values.string, sizeof(char *) * (count_values));
                values.string[count_values - 1] = string_value;

                cur_state = SPACES_AFTER_ARRAYS_STRING;
            } else if (sym == '\\') {
                cur_state = NEXT_STRING_SLASH;
            } else if (sym == '\n') {
                cur_state = ERROR;
            }
            break;
        }
        case NEXT_STRING_SLASH: {
            cnt_slash++;
            cur_state = NEXT_STRING;
            break;
        }
        case SPACES_AFTER_ARRAYS_STRING: {
            if (sym != ' ' && sym != ',' && sym != ']') {
                cur_state = ERROR;
            } else if (sym == ',') {
                cur_state = SPACES_BEFORE_ARRAYS_STRING;
            } else if (sym == ']') {
                cur_state = FINISH;
            }
            break;
        }
        case SPACES_BEFORE_ARRAYS_STRING: {
            if (sym != ' ' && sym != '\"') {
                cur_state = ERROR;
            } else if (sym == '\"') {
                begin_cur_val_pos = cur_pos + 1;
                cnt_slash = 0;
                cur_state = NEXT_STRING;
            }
            break;
        }
        case NEXT_DIGIT: {
            if (!(sym >= '0' && sym <= '9') && sym != '.' && sym != ' ' &&
                sym != ']' && sym != ',') {
                cur_state = ERROR;
            } else if (sym == '.') {
                if (*(cur_sym_ptr + 1) < '0' || *(cur_sym_ptr + 1) > '9') {
                    cur_state = ERROR;
                } else {
                    cur_state = NEXT_DOUBLE;
                }
            } else if (sym == ' ' || sym == ']' || sym == ',') {
                type = INTEGER;
                end_cur_val_pos = cur_pos;
                /*DELETE LEFT NULLS*/
                while (line[begin_cur_val_pos] == '0' &&
                       begin_cur_val_pos < end_cur_val_pos - 1) {
                    begin_cur_val_pos++;
                }
                /*CHECK CAPACITY OF VALUES ARRAY*/
                count_values++;
                values.integer = realloc(values.integer,
                                         sizeof(long long *) * (count_values));
                values.integer[count_values - 1] =
                    strtoll(line + begin_cur_val_pos, NULL, 10);

                if (sym == ' ') {
                    cur_state = SPACES_AFTER_ARRAYS_LONG;
                } else if (sym == ']') {
                    cur_state = FINISH;
                } else if (sym == ',') {
                    cur_state = SPACES_BEFORE_ARRAYS_LONG;
                }
            }
            break;
        }
        case SPACES_AFTER_ARRAYS_LONG: {
            if (sym != ' ' && sym != ',' && sym != ']') {
                cur_state = ERROR;
            } else if (sym == ',') {
                cur_state = SPACES_BEFORE_ARRAYS_LONG;
            } else if (sym == ']') {
                cur_state = FINISH;
            }
            break;
        }
        case SPACES_BEFORE_ARRAYS_LONG: {
            if (sym != ' ' && !(sym >= '0' && sym <= '9')) {
                cur_state = ERROR;
            } else if (sym >= '0' && sym <= '9') {
                begin_cur_val_pos = cur_pos;
                cur_state = NEXT_EXPECTED_LONG;
            }
            break;
        }
        case NEXT_EXPECTED_LONG: {
            if (!(sym >= '0' && sym <= '9') && sym != ' ' && sym != ']' &&
                sym != ',') {
                cur_state = ERROR;
            } else if (sym == ' ' || sym == ']' || sym == ',') {
                end_cur_val_pos = cur_pos;
                /*DELETE LEFT NULLS*/
                while (line[begin_cur_val_pos] == '0' &&
                       begin_cur_val_pos < end_cur_val_pos - 1) {
                    begin_cur_val_pos++;
                }
                /*CHECK CAPACITY OF VALUES ARRAY*/
                count_values++;
                values.integer =
                    realloc(values.integer, sizeof(long long *) * count_values);
                values.integer[count_values - 1] =
                    strtoll(line + begin_cur_val_pos, NULL, 10);

                if (sym == ' ') {
                    cur_state = SPACES_AFTER_ARRAYS_LONG;
                } else if (sym == ',') {
                    cur_state = SPACES_BEFORE_ARRAYS_LONG;
                } else {
                    cur_state = FINISH;
                }
            }
            break;
        }
        case NEXT_DOUBLE: {
            if (!((sym >= '0' && sym <= '9') || sym == ' ' || sym == ']' ||
                  sym == ',')) {
                cur_state = ERROR;
            } else if (sym == ' ' || sym == ']' || sym == ',') {
                type = REAL;
                end_cur_val_pos = cur_pos;
                /*CHECK CAPACITY OF VALUES ARRAY*/
                count_values++;
                values.real =
                    realloc(values.real, sizeof(double *) * count_values);
                values.real[count_values - 1] =
                    strtod(line + begin_cur_val_pos, NULL);

                if (sym == ' ') {
                    cur_state = SPACES_AFTER_ARRAYS_DOUBLE;
                } else if (sym == ',') {
                    cur_state = SPACES_BEFORE_ARRAYS_DOUBLE;
                } else if (sym == ']') {
                    cur_state = FINISH;
                }
            }
            break;
        }
        case SPACES_AFTER_ARRAYS_DOUBLE: {
            if (sym != ' ' && sym != ',' && sym != ']') {
                cur_state = ERROR;
            } else if (sym == ',') {
                cur_state = SPACES_BEFORE_ARRAYS_DOUBLE;
            } else if (sym == ']') {
                cur_state = FINISH;
            }
            break;
        }
        case SPACES_BEFORE_ARRAYS_DOUBLE: {
            if (sym != ' ' && !(sym >= '0' && sym <= '9')) {
                cur_state = ERROR;
            } else if (sym >= '0' && sym <= '9') {
                begin_cur_val_pos = cur_pos;
                cur_state = NEXT_EXPECTED_DOUBLE;
            }
            break;
        }
        case NEXT_EXPECTED_DOUBLE: {
            if (!(sym >= '0' && sym <= '9') && sym != '.') {
                cur_state = ERROR;
            } else if (sym == '.') {
                if (*(cur_sym_ptr + 1) < '0' || *(cur_sym_ptr + 1) > '9') {
                    cur_state = ERROR;
                } else {
                    cur_state = NEXT_EXACTLY_DOUBLE;
                }
            }
            break;
        }
        case NEXT_EXACTLY_DOUBLE: {
            if (!((sym >= '0' && sym <= '9') || sym == ' ' || sym == ']' ||
                  sym == ',')) {
                cur_state = ERROR;
            } else if (sym == ' ' || sym == ']' || sym == ',') {
                end_cur_val_pos = cur_pos;
                /*CHECK CAPACITY OF VALUES ARRAY*/
                count_values++;
                values.real = realloc(values.real, sizeof(double *) * count_values);
                values.real[count_values - 1] = strtod(line + begin_cur_val_pos, NULL);

                if (sym == ' ') {
                    cur_state = SPACES_AFTER_ARRAYS_DOUBLE;
                } else if (sym == ',') {
                    cur_state = SPACES_BEFORE_ARRAYS_DOUBLE;
                } else if (sym == ']') {
                    cur_state = FINISH;
                }
            }
            break;
        }
        default:
            cur_state = ERROR;
            break;
        }

        if (break_loop) {
            break;
        }
        cur_pos++;
        cur_sym_ptr++;
    }
    // CHECK IF BROKE AND DIDN'T WROTE VALUE
    if (cur_state == VALUE_DIGIT) {
        end_cur_val_pos = cur_pos;
        /*DELETE LEFT NULLS*/
        while (line[begin_cur_val_pos] == '0' &&
               begin_cur_val_pos < end_cur_val_pos - 1) {
            begin_cur_val_pos++;
        }
        count_values++;
        values.integer = realloc(values.integer, sizeof(long long *) * count_values);
        values.integer[count_values - 1] = strtoll(line + begin_cur_val_pos, NULL, 10);
        cur_state = FINISH;
        break_loop = 1;
    } else if (cur_state == VALUE_DOUBLE) {
        end_cur_val_pos = cur_pos;
        count_values++;
        values.real = realloc(values.real, sizeof(double *) * count_values);
        values.real[count_values] = strtod(line + begin_cur_val_pos, NULL);
        cur_state = FINISH;
        break_loop = 1;
    } else if (cur_state == START) {
        // CHECK THAT LINE IS EMPTY
        break_loop = -1;
    } else if (cur_state == FINISH) {
        break_loop = 1;
    } else if (cur_state == ERROR) {
        break_loop = 2;
    } else if (cur_state == COMMENT) {
        break_loop = 3;
    } else {
        break_loop = 2;
    }

    // RETURN RESULTS

    if (break_loop == 1) {
        *rkey = key_name;
        *rvalues = values;
        *rsize = count_values;
        *rtype = type;
        return 0;
    } else if (break_loop == 3) {
        *rkey = NULL;
        return 2;
    } else if (break_loop == -1) {
        *rkey = NULL;
        return 3;
    }

    /*PROCESS ERROR CASE*/

    if (key_name) {
        free(key_name);
    }
    if (type == STRING) {
        for (int i = 0; i < count_values; ++i) {
            free(values.string[i]);
        }
    }
    free((void *)values.integer);
    *rkey = NULL;
    return -1;
}

/*!
    \brief Parses config file

    Rules for config file described in config.h description. If logger is
   active, parser will write in log. \param[in] path Path to configuration file
    \return 0 if success, 1 if empty file, 2 if no config -1 and sets errno else
*/
int parse_config(const char *path) {
    if (!path) {
        return -1;
    }

    char *line = NULL;
    size_t len = 0;

    if (access(path, F_OK)) {
        fprintf(stderr, "File %s not found", path);
        return -1;
    }


    if (access(path, R_OK)) {
        fprintf(stderr, "Have no permissions for file %s\n", path);
        return -1;
    }


    FILE *config = fopen(path, "r");

    if (!config) {
        return -1;
    }

    int check = 0;              // check result of getline
    int cnt_line = 0;
    int is_empty_file = 1;
    while (!feof(config) && (check = getline(&line, &len, config)) &&
           check != -1) {
        if (line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = 0;
        }
        cnt_line++;
        int size = 0;
        ConfigData values;
        char *key = NULL;
        ConfigVarType type = 0;
        int parse_result = 0;
        if ((parse_result = parse_line(line, &key, &values, &size, &type))) {
            if (parse_result == -1) {
                fprintf(stderr,
                        "Config file error\n\tIn line %d: \"%s\"\n\tRead "
                        "syntax in config.h\n",
                        cnt_line, line);
                free(line);
                fclose(config);
                return -1;
            }
            /*ELSE WE SKIP COMMENT OR EMPTY STRING*/
            free(line);
            continue;
        }
        is_empty_file = 0;
        free(line);

        /*TRY TO ADD NEW PARAMETER*/

        ConfigVariable variable = {key,NULL,values, type, size};
        define_variable(variable);
        destroy_variable(&variable);
    }
    fclose(config);
    if (is_empty_file) {
        return 1;
    }
    return 0;
}