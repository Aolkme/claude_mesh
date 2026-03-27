/**
 * command_handler.h - JSON 命令路由接口
 */
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "tcp_server.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ─── 网关全局状态（供 main.c 定义，command_handler.c/event_loop.c 使用） ─── */
typedef struct gateway_state {
    bool        connected;
    uint32_t    my_node_num;
    time_t      start_time;
    uint64_t    rx_count;

    /* 串口指针（NULL = 未连接；connect_serial 命令设置后非 NULL）*/
    void       *serial;    /* serial_port_t *，避免循环 include */

    /* 配置指针（event_loop 和 command_handler 均需访问）*/
    void       *cfg;       /* config_t * */

    /* 原始串口发送回调（遗留接口，由 command_handler 调用）*/
    int (*send_raw)(const uint8_t *buf, size_t len, void *ctx);
    void       *send_ctx;
} gateway_state_t;

/**
 * 处理一条 JSON 命令请求，返回 JSON 响应字符串（调用者负责 free）
 *
 * 支持的命令（"cmd" 字段）：
 *   get_status        - 连接状态、节点数、uptime
 *   get_gateway_info  - 网关自身节点信息
 *   get_nodes         - 所有节点列表（含私有配置字段）
 *   get_node          - 单节点详情（node_id 参数）
 *   connect_serial    - 触发串口连接（device, [baudrate]）
 *   disconnect_serial - 断开串口
 *   send_text         - 发送文本（to, text, [channel], [want_ack]）
 *   send_position             - 发送位置（to, lat, lon, [alt], [channel]）
 *   admin_get_session_passkey - 请求 Session Passkey（node_id）
 *   admin_get_config          - 读取设备配置（node_id, config_type, [passkey]）
 *   admin_get_channel         - 读取信道配置（node_id, channel_index, [passkey]）
 *   admin_reboot              - 重启远程节点（node_id, [delay_s], [passkey]）
 *   monitor_start             - 订阅实时事件推送
 *   monitor_stop              - 取消订阅
 */
char *command_handle(sock_t client_fd, const char *json, void *userdata);

#endif /* COMMAND_HANDLER_H */
