/*!
    \file logger.c
    \brief Logger implementation

    This file contains logger's implementation.

    Logger is the singleton. Therefore init_logger must return
    -1 if logger already exists

    Logger uses wrapper of time()
*/

#include "../../include/logger.h"
#include "../../include/my_time.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// TYPES DECLARATION

/*!
    \struct LoggerData
    \brief struct that describes info needed for logging session
*/
struct LoggerData {
    char *path;
    FILE *session;
    int file_size_limit;
    int is_initialized;
};

// GLOBALS

struct LoggerData logger_state = {NULL, NULL, 0, 0}; // singleton

// FUNCTION DECLARATIONS

int init_logger(const char *path, int file_size_limit);
void write_log(enum OutputStream stream, enum LogLevel level,
               const char *filename, int line_number, const char *format, ...);
void fini_logger(void);


//FUNCTION DEFINITIONS

int init_logger(const char *path, int file_size_limit) {
    //validate input
    if (logger_state.is_initialized)
        return -1;

    if (!path) {
        logger_state.is_initialized = 1;
        logger_state.path = NULL;
        return 0;
    }

    // validate: path, RIGHTS, and so on
    if (access(path, W_OK) && !access(path, F_OK)) {
        fprintf(stderr, "Have no permissions for file %s\n", path);
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
    //choose filestream
    FILE *log_file_stream = stderr;
    switch (stream) {
    case STDERR: {
        log_file_stream = stderr;
        break;
    }
    case STDOUT: {
        log_file_stream = stdout;
    }
    case FILESTREAM: {
        if (!logger_state.session) {
            fprintf(stderr, "Failed to log into NULL file. LOG is\n");
            log_file_stream = stderr;
            return;
        }
        log_file_stream = logger_state.session;
    }
    }

    //write message in loop for case when log message overflows log file
    bool success_write = false;
    for  (int cnt_tries = 0; !success_write && cnt_tries < 1; cnt_tries++) {
        //choose log level
        const char *msg_lvl;
        switch (level) {
        case LOG_DEBUG:
            msg_lvl = "DEBUG";
            break;
        case LOG_INFO:
            msg_lvl = "INFO";
            break;
        case LOG_WARNING:
            msg_lvl = "WARNING";
            break;
        case LOG_ERROR:
            msg_lvl = "ERROR";
            break;
        case LOG_FATAL:
            msg_lvl = "FATAL";
            break;
        default:
            errno = EBADMSG;
            return;
        }
        //write header of log
        time_t mytime = get_time();
        struct tm *now = localtime(&mytime);
        fprintf(log_file_stream,
                "%d-%d-%dT%d:%d:%d(%s) %s %d %d | %s: ", now->tm_year + 1900,
                now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min,
                now->tm_sec, now->tm_zone, filename, line_number, getpid(),
                msg_lvl);
        //write body of log
        va_list ptr;
        va_start(ptr, format);
        vfprintf(log_file_stream, format, ptr);
        va_end(ptr);
        fprintf(log_file_stream, "\n");

        success_write = true;
        if (stream == FILESTREAM) {
            fflush(logger_state.session);
            //check size of file
            if (logger_state.file_size_limit > 0 &&
                ftell(logger_state.session) > logger_state.file_size_limit) {
                success_write = false;
                fclose(logger_state.session);
            }
        }
    }
}

void fini_logger(void) {
    fclose(logger_state.session);
    free(logger_state.path);
    logger_state.is_initialized = 0;
}