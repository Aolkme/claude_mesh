/**
 * config.h - INI 配置文件接口
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "log.h"

typedef struct {
    /* [serial] */
    char     serial_device[128];   /* e.g. /dev/ttyUSB0 or COM3 */
    uint32_t serial_baudrate;      /* default 115200 */
    bool     auto_connect;         /* default false（延迟连接）*/
    uint32_t reconnect_interval_s; /* 断线重连间隔，default 5 */

    /* [network] */
    char     tcp_host[64];         /* default 127.0.0.1 */
    uint16_t tcp_port;             /* default 9999 */
    int      tcp_max_clients;      /* default 16 */

    /* [heartbeat] */
    uint32_t heartbeat_interval_s; /* default 600 */
    uint32_t heartbeat_timeout_s;  /* default 30 */

    /* [log] */
    log_level_t log_level;         /* default LOG_INFO */
    char        log_file[256];     /* empty = console only */

    /* [node] */
    uint32_t node_expire_s;        /* default 3600 (1 hour) */

    /* [web] */
    char     web_bind[64];         /* bind addr:port, empty = disabled; default "0.0.0.0:8080" */
    char     web_static_dir[256];  /* static files root; default "static" */
} config_t;

/**
 * 加载 INI 配置文件，未指定项使用默认值
 * @param path  配置文件路径（NULL = 全部使用默认值）
 * @param cfg   输出配置结构体
 * @return 0 成功，-1 文件打开失败（仍输出默认值）
 */
int config_load(const char *path, config_t *cfg);

/** 打印当前配置到日志（INFO级别） */
void config_print(const config_t *cfg);

#endif /* CONFIG_H */
