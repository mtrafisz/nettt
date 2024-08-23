#include "logger.h"

#include <assert.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#pragma execution_character_set( "utf-8" )
#endif

struct __logger_ctx {
    LogLevel min_log_level;
    FILE* output;
    bool color;
    bool append_nl;
    bool print_datetime;
    bool print_loglevel;
};

static struct __logger_ctx logger_ctx =  {
    .min_log_level = LOG_DEBUG,
    .output = NULL,
    .color = true,
    .append_nl = true,
    .print_datetime = true,
    .print_loglevel = true,
};

void logger_init(FILE* output) {
    assert(output);
    logger_ctx.output = output;
    
    const char* term = getenv("TERM");
    if (term == NULL ||
        (strstr(term, "xterm") == NULL && strstr(term, "screen") == NULL && strstr(term, "ansi") == NULL && strstr(term, "color") == NULL)) {
        log_message(LOG_TRACE, "Color likely not supported in terminal");
        logger_ctx.color = false;
    }

#ifdef _WIN32
    // wine hates me, so I can't really test if this works without creating windows VM
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

void set_log_level(LogLevel level) {
    if (level > LOG_NONE || level < LOG_ALL) return;

    logger_ctx.min_log_level = level;
}

void set_log_color(bool on) {
    logger_ctx.color = on;
}

void set_log_append_nl(bool on) {
    logger_ctx.append_nl = on;
}

void set_log_append_dt(bool on)  {
    logger_ctx.print_datetime = on;
}

void set_log_append_lvl(bool on) {
    logger_ctx.print_loglevel = on;
}

const char* log_level_to_string(LogLevel level) {
    const char* strings[] = {
        "TRACE",
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "FATAL",
    };
    const char* strings_color[] = {
        "\033[1;35mTRACE\033[0m",
        "\033[1;36mDEBUG\033[0m",
        "\033[1;37mINFO\033[0m",
        "\033[1;33mWARNING\033[0m",
        "\033[1;31mERROR\033[0m",
        "\033[1;30m\033[1;41mFATAL\033[0m",
    };
    if (level <= LOG_ALL || level >= LOG_NONE) return NULL;
    return logger_ctx.color ? strings_color[level-1] : strings[level-1];
}

void log_message(LogLevel level, const char* fmt, ...) {
    if (level < logger_ctx.min_log_level || !logger_ctx.output) return;

    va_list args;
    va_start(args, fmt);

    const char* datetime_fmt_plain = "[%d-%02d-%02d %02d:%02d:%02d] ";
    const char* datetime_fmt_color = "[\033[1;34m%d-%02d-%02d %02d:%02d:%02d\033[0m] ";

    if (logger_ctx.print_datetime) {
        time_t now = time(NULL);
        struct tm* now_tm = localtime(&now);

        const char* datetime_fmt = logger_ctx.color ? datetime_fmt_color : datetime_fmt_plain;
        fprintf(logger_ctx.output, datetime_fmt, now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday,
                now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);
    }

    if (logger_ctx.print_loglevel) {
        printf("[%s] ", log_level_to_string(level));
    }

    vprintf(fmt, args);

    if (logger_ctx.append_nl) puts("");
    return;
}
