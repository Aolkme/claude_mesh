/**
 * log.h - 日志模块接口
 */
#ifndef LOG_H
#define LOG_H

#include <stdio.h>

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

/**
 * 初始化日志模块
 * @param level    最低输出级别
 * @param filepath 日志文件路径（NULL = 只输出控制台）
 */
void log_init(log_level_t level, const char *filepath);
void log_close(void);

/* 格式化日志输出宏 */
#define LOG_DEBUG(module, fmt, ...) log_write(LOG_DEBUG, module, fmt, ##__VA_ARGS__)
#define LOG_INFO(module, fmt, ...)  log_write(LOG_INFO,  module, fmt, ##__VA_ARGS__)
#define LOG_WARN(module, fmt, ...)  log_write(LOG_WARN,  module, fmt, ##__VA_ARGS__)
#define LOG_ERROR(module, fmt, ...) log_write(LOG_ERROR, module, fmt, ##__VA_ARGS__)

void log_write(log_level_t level, const char *module, const char *fmt, ...);

#endif /* LOG_H */
