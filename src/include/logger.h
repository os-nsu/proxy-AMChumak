/*!
    \file logger.h
    \brief Logger dynamic library interface
    \version 1.0
    \date 15 Jan 2025

    This file contains logger's function signatures.

    Logger is the singleton. Therefore double init_logger must return -1.

    Logger must be able to write messages in some output streams that
    enumerates in OutputStream enumeration:

    1) STDOUT
    2) STDERR - standard stream for logging. Logger must use this stream
    if chosen stream not available.
    3) FILESTREAM - logger is able to use this stream if parameter path in
    init_logger call is not NULL. There are log files in chosen directory.
    Their names obey next rule: "proxyN.log", where N is order number of file.
    If all available log files are filled, then logger starts overwrite them
    from first.

    IDENTIFICATION
        src/include/logger.h
*/

#ifndef  LOGGER_H
#define LOGGER_H


enum LogLevel {
    LOG_DEBUG = 1, ///< low level messages
    LOG_INFO,      ///< traces and meta information (could be substitution for
                   ///< stderr)
    LOG_WARNING,   ///< messages that describe possibly incorrect behavior with
                   ///< saving invariant
    LOG_ERROR,     ///< messages about system errors
    LOG_FATAL      ///< messages about system errors that couldn't be processed
};

enum OutputStream { STDOUT = 1, STDERR, FILESTREAM };

/*!
    It initializes logger's data (path to directory with log files).
    \param [in] path path to file with logs. Could be NULL if file
    stream wouldn't be used.
    \param [in] file_size_limit maximal size in Kb of log file
    \return 0 if success, -1 and sets errno if error
*/
int init_logger(const char *path, int file_size_limit);

/*!
    Function frees logger's data structures
*/
int fini_logger(void);

/*!
    This function write message to .log file with specified format (see file
    description)
    \param [in] stream type of output stream
    \param [in] level level of message
    \param [in] filename name of code file where write_log was called
    \param [in] line_number number of line where write_log was called
    \param [in] format pointer to format string (like in printf)
    \param [in] ... variadic list of parameters for format string (like in
    printf)
    \return void
 */
void write_log(enum OutputStream stream, enum LogLevel level,
               const char *filename, int line_number, const char *format, ...);




#endif  //LOGGER_H