#include "../../include/logger.h"
#include "../../include/my_time.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

/*!
    \struct LoggerData
    \brief struct that describes info needed for logging session
*/
struct LoggerData {
    long file_size_limit;
    int files_limit;
    int file_num;
    char *directory;
    FILE *session;
    int is_initialized;
};

struct LoggerData logger_state = {0, 0, 0, NULL, NULL, 0}; // singleton

/*!
    \brief allocated and fill string for parent dir name
    \param [in] dir path to directory
    \return allocated string with parent directory name
*/
char *get_parent_dir(const char *path);

void get_next_word(
    const char *path, int pos, int *left,
    int *right) { // return name without '/' (exception: root "/")
    int right_border = pos - 1;
    int left_border = right_border - 1;

    while (left_border >= 0 && path[left_border] != '/') {
        left_border--;
    }
    left_border++;

    if (left_border != right_border && path[right_border] == '/') {
        right_border--;
    }

    *left = left_border;
    *right = right_border;
}

char *get_parent_dir(const char *path) {
    int path_len = strlen(path);
    int right_border = path_len;
    int left_border = path_len;
    get_next_word(path, path_len, &left_border, &right_border);
    int del_border = left_border;

    while (!strncmp(path + left_border, ".", right_border - left_border + 1) &&
           left_border > 0) {
        get_next_word(path, del_border, &left_border, &right_border);
        del_border = left_border;
    }

    if (left_border == 0) {
        if (path[left_border] == '/') {
            char *result = malloc(sizeof(char) * 2);
            return strcpy(result, "/");
        } else if (!strncmp(path, ".", right_border - left_border + 1)) {
            char *result = malloc(sizeof(char) * 4);
            return strcpy(result, "../");
        }
        char *result = malloc(sizeof(char) * 3);
        return strcpy(result, "./");
    }

    char *result = calloc(del_border + 1, sizeof(char));
    return strncpy(result, path, del_border);
}

int init_logger(const char *path, long file_size_limit, int files_limit) {
    if (logger_state.is_initialized)
        return -1;

    if (!path) {
        logger_state.is_initialized = 1;
        logger_state.directory = NULL;
        return 0;
    }

    // validate: path, RIGHTS, and so on
    if (access(path, W_OK) && !access(path, F_OK)) {
        fprintf(stderr, "Directory %s exists and has no rights.\n", path);
        return -1;
    }

    // try to open file
    logger_state.session = fopen(path, "a");

    if (!logger_state.session) {
        fprintf(stderr, "Failed to open file %s\n", path);
        return -1;
    }

    // create structure
    logger_state.directory = (char *)calloc((strlen(path) + 1), sizeof(char));
    strcpy(logger_state.directory, path);
    logger_state.file_num = 0;
    logger_state.file_size_limit = file_size_limit;
    logger_state.files_limit = files_limit;
    logger_state.is_initialized = 1;

    return 0;
}

void write_log(enum OutputStream stream, enum LogLevel level,
               const char *filename, int line_number, const char *format, ...) {
    const char *msg_lvl;

    switch (level) {
    case LOG_DEBUG:
        msg_lvl = "DEBUG: ";
        break;
    case LOG_INFO:
        msg_lvl = "INFO: ";
        break;
    case LOG_WARNING:
        msg_lvl = "WARNING: ";
        break;
    case LOG_ERROR:
        msg_lvl = "ERROR: ";
        break;
    case LOG_FATAL:
        msg_lvl = "FATAL: ";
        break;
    default:
        errno = EBADMSG;
        return;
    }

    long begin_msg = ftell(logger_state.session);
    time_t mytime = get_time();
    struct tm *now = localtime(&mytime);
    fprintf(logger_state.session, "%d-%d-%dT%d:%d:%d(%s) %s %d %d | %s: ",
            now->tm_year + 1900, now->tm_mon + 1,now->tm_mday, now->tm_hour,
            now->tm_min, now->tm_sec,now->tm_zone, filename, line_number, getpid(), msg_lvl);
    va_list ptr;
    va_start(ptr, format);
    vfprintf(logger_state.session, format, ptr);
    va_end(ptr);
    fprintf(logger_state.session, "\n");
    fflush(logger_state.session);
}