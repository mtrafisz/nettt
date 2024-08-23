/**
 * Simple logger for C programs.
 * 
 * Usage:
 * 
```c
#include "logger.h"
#include <stdlib.h>

int main(void) {
    logger_init(stdout);
    set_log_level(LEVEL_ALL);
    set_log_append_dt(false);

    const char* filename = "logger.c";

    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        log_errno("Couldn't open file %s", filename);
        exit(1);
    }

    log_message(LOG_INFO, "Opened %s", filename);

    char buffer[1024];
    size_t read = fread(buffer, 1, 1024, fp);
    if (read == 0) {
        log_errno("Failed to read from %s", filename);
        fclose(fp);
        exit(1);
    }

    fclose(fp);

    log_message(LOG_INFO, "%.*s\n", read, buffer);
    return 0;
}
```
 */

#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef enum {
    LEVEL_ALL = 0,
    LEVEL_TRACE,
    LEVEL_DEBUG,
    LEVEL_INFO,
    LEVEL_WARNING,
    LEVEL_ERROR,
    LEVEL_FATAL,
    LEVEL_NONE
} LogLevel;

/**
 * Initialize logger - must be run before anything gets printed
 *
 * You can also call this function to change output of the logger.
 */
void logger_init(FILE* output);

/**
 * Set minimal level of logs, that will be displayed
 *
 * Ex. if You call `set_log_level(LEVEL_FATAL)` only log messages containing fatal errors will be written to FILE
 */
void set_log_level(LogLevel level);
/**
 * Turn on/off writing colors using ANSI escape codes
 */
void set_log_color(bool on);
/**
 * Turn on/off '\n' appending to the end of log message
 */
void set_log_append_nl(bool on);
/**
 * Turn on/off appending datetime to beggining of log message
 */
void set_log_append_dt(bool on);
/**
 * Turn on/off appending log level in format '[<LOG-LEVEL>]` to beggining of log message (after datetime, if active)
 */
void set_log_append_lvl(bool on);

/**
 * Write log message to specified FILE
 */
void log_message(LogLevel level, const char* fmt, ...);
#define log_errno_msg(msg) log_message(LEVEL_FATAL, msg": %s", strerror(errno));
#define log_errno_fmt(fmt, ...) log_message(LEVEL_FATAL, fmt": %s", __VA_ARGS__, strerror(errno));

#endif
