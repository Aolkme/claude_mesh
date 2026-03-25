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
    int (*send_raw)(const uint8_t *buf, size_t len, void *ctx);
    void       *send_ctx;
} gateway_state_t;

/**
 * 处理一条 JSON 命令请求，返回 JSON 响应字符串（调用者负责 free）
 *
 * 支持的命令（"cmd" 字段）：
 *   get_status    - 返回连接状态、节点数、uptime
 *   get_nodes     - 返回所有节点列表
 *   send_text     - 发送文本消息（需要 to, text, [channel] 字段）
 *   monitor_start - 开始订阅实时事件推送
 *   monitor_stop  - 停止订阅
 */
char *command_handle(sock_t client_fd, const char *json, void *userdata);

#endif /* COMMAND_HANDLER_H */
