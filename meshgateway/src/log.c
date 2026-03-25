/**
 * log.c - 日志模块实现
 *
 * 格式: [YYYY-MM-DD HH:MM:SS] [LEVEL] [MODULE] Message
 */
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define MUTEX_T           CRITICAL_SECTION
#define MUTEX_INIT(m)     InitializeCriticalSection(&(m))
#define MUTEX_LOCK(m)     EnterCriticalSection(&(m))
#define MUTEX_UNLOCK(m)   LeaveCriticalSection(&(m))
#define MUTEX_DESTROY(m)  DeleteCriticalSection(&(m))
#else
#include <pthread.h>
#define MUTEX_T           pthread_mutex_t
#define MUTEX_INIT(m)     pthread_mutex_init(&(m), NULL)
#define MUTEX_LOCK(m)     pthread_mutex_lock(&(m))
#define MUTEX_UNLOCK(m)   pthread_mutex_unlock(&(m))
#define MUTEX_DESTROY(m)  pthread_mutex_destroy(&(m))
#endif

static struct {
    log_level_t level;
    FILE       *file;
    MUTEX_T     mutex;
} g_log;

static const char *level_str[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };

/* ANSI 颜色（终端输出，Windows 控制台不支持时自动忽略） */
static const char *level_color[] = { "\033[36m", "\033[32m", "\033[33m", "\033[31m" };
#define COLOR_RESET "\033[0m"

void log_init(log_level_t level, const char *filepath) {
    g_log.level = level;
    g_log.file  = NULL;
    MUTEX_INIT(g_log.mutex);

    if (filepath && filepath[0]) {
        g_log.file = fopen(filepath, "a");
        if (!g_log.file)
            fprintf(stderr, "[LOG] Cannot open log file: %s\n", filepath);
    }
}

void log_close(void) {
    if (g_log.file) { fclose(g_log.file); g_log.file = NULL; }
    MUTEX_DESTROY(g_log.mutex);
}

void log_write(log_level_t level, const char *module, const char *fmt, ...) {
    if (level < g_log.level) return;

    /* 时间戳 */
    char timebuf[32];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    va_list ap1, ap2;
    va_start(ap1, fmt);
    va_copy(ap2, ap1);

    MUTEX_LOCK(g_log.mutex);

    /* 控制台输出（带颜色） */
    fprintf(stderr, "%s[%s] [%s] [%-10s] ",
            level_color[level], timebuf, level_str[level], module);
    vfprintf(stderr, fmt, ap1);
    fprintf(stderr, "%s\n", COLOR_RESET);

    /* 文件输出（不带颜色） */
    if (g_log.file) {
        fprintf(g_log.file, "[%s] [%s] [%-10s] ",
                timebuf, level_str[level], module);
        vfprintf(g_log.file, fmt, ap2);
        fprintf(g_log.file, "\n");
        fflush(g_log.file);
    }

    MUTEX_UNLOCK(g_log.mutex);

    va_end(ap1);
    va_end(ap2);
}
