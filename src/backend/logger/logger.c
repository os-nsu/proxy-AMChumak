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
    char *path;
    FILE *session;
    int is_initialized;
};

struct LoggerData logger_state = {NULL, NULL, 0}; // singleton


int init_logger(const char *path) {
    if (logger_state.is_initialized)
        return -1;

    if (!path) {
        logger_state.is_initialized = 1;
        logger_state.path = NULL;
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
    logger_state.path = (char *)calloc((strlen(path) + 1), sizeof(char));
    strcpy(logger_state.path, path);
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

void fini_logger(void) {
    fclose(logger_state.session);
    free(logger_state.path);
    logger_state.is_initialized = 0;
}