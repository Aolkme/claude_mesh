/**
 * web_server.h - 嵌入式 HTTP + WebSocket 服务器接口（基于 Mongoose）
 *
 * HTTP 端点：
 *   GET  /          → 服务 static/ 目录下的静态文件（index.html）
 *   POST /api/cmd   → JSON 命令，与 TCP IPC 命令格式完全一致
 *
 * WebSocket 端点：
 *   WS   /ws        → 建立连接后自动订阅所有事件广播
 *                     发送 JSON 命令 → 返回 JSON 响应
 *                     接收事件推送（与 TCP monitor 格式相同）
 */
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "mongoose.h"
#include <stdbool.h>

typedef struct {
    struct mg_mgr mgr;
    void         *gstate;           /* gateway_state_t * */
    char          static_dir[256];  /* 静态文件根目录 */
    bool          initialized;
} web_server_t;

/**
 * 初始化并启动 Web 服务器
 *
 * @param ws         服务器状态结构体
 * @param bind_addr  监听地址，如 "0.0.0.0:8080"；NULL 或空串 = 禁用
 * @param static_dir 静态文件根目录（如 "static" 或 "/usr/share/meshgateway/static"）
 * @param gstate     gateway_state_t 指针，供命令处理器使用
 * @return 0 成功，-1 失败
 */
int  web_server_init(web_server_t *ws, const char *bind_addr,
                     const char *static_dir, void *gstate);

/**
 * 非阻塞驱动 Mongoose 事件循环（在主 select 循环末尾调用）
 */
void web_server_poll(web_server_t *ws);

/**
 * 向所有已连接的 WebSocket 客户端广播 JSON 事件
 * （与 tcp_server_broadcast_event 配合使用）
 */
void web_server_broadcast(web_server_t *ws, const char *json);

/**
 * 关闭服务器，释放资源
 */
void web_server_close(web_server_t *ws);

#endif /* WEB_SERVER_H */
