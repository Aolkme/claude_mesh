/**
 * tcp_server.h - TCP IPC 服务器接口
 *
 * 监听 127.0.0.1:PORT，支持多客户端连接和事件广播。
 * 消息帧格式: [uint32_t len (BE)][uint8_t type][JSON payload]
 */
#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET sock_t;
#define INVALID_SOCK INVALID_SOCKET
#else
typedef int sock_t;
#define INVALID_SOCK (-1)
#endif

/* 消息类型字节 */
#define MSG_TYPE_REQUEST    0x01   /* CLI → daemon */
#define MSG_TYPE_RESPONSE   0x02   /* daemon → CLI (对应请求) */
#define MSG_TYPE_EVENT      0x03   /* daemon → CLI (主动推送) */
#define MSG_TYPE_ERROR      0x04

/* 命令回调：client_fd, json字符串, userdata → 返回JSON响应字符串（调用者free） */
typedef char *(*command_cb_t)(sock_t client_fd, const char *json, void *userdata);

typedef struct {
    sock_t       listen_fd;
    sock_t       clients[16];
    int          client_count;
    int          max_clients;
    command_cb_t on_command;
    void        *userdata;
    bool         monitor_flags[16]; /* 每个客户端是否订阅实时事件 */
} tcp_server_t;

/**
 * 初始化并绑定监听 socket
 * @param srv       服务器状态
 * @param host      绑定地址（"127.0.0.1"）
 * @param port      端口号
 * @param max_clients 最大连接数（≤16）
 * @return 0 成功，-1 失败
 */
int  tcp_server_init(tcp_server_t *srv, const char *host, uint16_t port, int max_clients);
void tcp_server_close(tcp_server_t *srv);

/**
 * 设置命令处理回调
 */
void tcp_server_set_command_cb(tcp_server_t *srv, command_cb_t cb, void *userdata);

/**
 * 填充所有 fd 到 fd_set（供 select() 使用）
 * @return 当前最大 fd 值
 */
sock_t tcp_server_fill_fdset(tcp_server_t *srv, void *read_fds);

/**
 * 处理就绪的 fd（在 select() 返回后调用）
 * read_fds: fd_set*
 */
void tcp_server_handle(tcp_server_t *srv, void *read_fds);

/**
 * 向所有订阅 monitor 的客户端广播事件 JSON
 */
void tcp_server_broadcast_event(tcp_server_t *srv, const char *json);

/**
 * 向单个客户端发送响应
 */
void tcp_server_send_response(sock_t fd, const char *json);

#endif /* TCP_SERVER_H */
