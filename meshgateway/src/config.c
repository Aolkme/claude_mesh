/**
 * config.c - INI 配置文件解析实现
 *
 * 格式示例：
 *   [serial]
 *   device = /dev/ttyUSB0
 *   baudrate = 115200
 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* 默认值 */
static void config_defaults(config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->serial_device,  "",            sizeof(cfg->serial_device) - 1);
    cfg->serial_baudrate        = 115200;
    cfg->auto_connect           = false;
    cfg->reconnect_interval_s   = 5;
    strncpy(cfg->tcp_host,       "127.0.0.1",   sizeof(cfg->tcp_host) - 1);
    cfg->tcp_port              = 9999;
    cfg->tcp_max_clients       = 16;
    cfg->heartbeat_interval_s  = 600;
    cfg->heartbeat_timeout_s   = 30;
    cfg->log_level             = LOG_INFO;
    cfg->log_file[0]           = '\0';
    cfg->node_expire_s         = 3600;
    strncpy(cfg->web_bind,       "0.0.0.0:8080", sizeof(cfg->web_bind) - 1);
    strncpy(cfg->web_static_dir, "static",        sizeof(cfg->web_static_dir) - 1);
}

/* 去除首尾空白 */
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)*(e-1))) e--;
    *e = '\0';
    return s;
}

static log_level_t parse_log_level(const char *s) {
    if (strcasecmp(s, "debug") == 0) return LOG_DEBUG;
    if (strcasecmp(s, "warn")  == 0) return LOG_WARN;
    if (strcasecmp(s, "error") == 0) return LOG_ERROR;
    return LOG_INFO;
}

int config_load(const char *path, config_t *cfg) {
    config_defaults(cfg);
    if (!path || !path[0]) return 0;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (!*p || *p == '#' || *p == ';') continue;

        /* [section] */
        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                *end = '\0';
                strncpy(section, p + 1, sizeof(section) - 1);
                trim(section);
            }
            continue;
        }

        /* key = value */
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        /* 去除行内注释 */
        char *comment = strchr(val, '#');
        if (!comment) comment = strchr(val, ';');
        if (comment) { *comment = '\0'; trim(val); }

        if (strcmp(section, "serial") == 0) {
            if      (strcmp(key, "device")             == 0) strncpy(cfg->serial_device, val, sizeof(cfg->serial_device) - 1);
            else if (strcmp(key, "baudrate")           == 0) cfg->serial_baudrate = (uint32_t)atoi(val);
            else if (strcmp(key, "auto_connect")       == 0) cfg->auto_connect = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
            else if (strcmp(key, "reconnect_interval") == 0) cfg->reconnect_interval_s = (uint32_t)atoi(val);
        } else if (strcmp(section, "network") == 0) {
            if      (strcmp(key, "host")        == 0) strncpy(cfg->tcp_host, val, sizeof(cfg->tcp_host) - 1);
            else if (strcmp(key, "port")        == 0) cfg->tcp_port = (uint16_t)atoi(val);
            else if (strcmp(key, "max_clients") == 0) cfg->tcp_max_clients = atoi(val);
        } else if (strcmp(section, "heartbeat") == 0) {
            if      (strcmp(key, "interval") == 0) cfg->heartbeat_interval_s = (uint32_t)atoi(val);
            else if (strcmp(key, "timeout")  == 0) cfg->heartbeat_timeout_s  = (uint32_t)atoi(val);
        } else if (strcmp(section, "log") == 0) {
            if      (strcmp(key, "level") == 0) cfg->log_level = parse_log_level(val);
            else if (strcmp(key, "file")  == 0) strncpy(cfg->log_file, val, sizeof(cfg->log_file) - 1);
        } else if (strcmp(section, "node") == 0) {
            if (strcmp(key, "expire") == 0) cfg->node_expire_s = (uint32_t)atoi(val);
        } else if (strcmp(section, "web") == 0) {
            if      (strcmp(key, "bind")       == 0) strncpy(cfg->web_bind,       val, sizeof(cfg->web_bind) - 1);
            else if (strcmp(key, "static_dir") == 0) strncpy(cfg->web_static_dir, val, sizeof(cfg->web_static_dir) - 1);
        }
    }

    fclose(f);
    return 0;
}

void config_print(const config_t *cfg) {
    LOG_INFO("config", "serial.device    = %s",  cfg->serial_device);
    LOG_INFO("config", "serial.baudrate  = %u",  cfg->serial_baudrate);
    LOG_INFO("config", "network.host     = %s",  cfg->tcp_host);
    LOG_INFO("config", "network.port     = %u",  cfg->tcp_port);
    LOG_INFO("config", "network.max_cli  = %d",  cfg->tcp_max_clients);
    LOG_INFO("config", "heartbeat.interval= %us", cfg->heartbeat_interval_s);
    LOG_INFO("config", "node.expire      = %us", cfg->node_expire_s);
}
