/**
 * tcp_client.h - CLI TCP 客户端接口
 */
#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

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

/* 消息类型（与 tcp_server.h 保持一致） */
#define MSG_TYPE_REQUEST    0x01
#define MSG_TYPE_RESPONSE   0x02
#define MSG_TYPE_EVENT      0x03
#define MSG_TYPE_ERROR      0x04

typedef struct {
    sock_t fd;
} tcp_client_t;

/**
 * 连接到 meshgateway daemon
 * @return 0 成功，-1 失败
 */
int  tcp_client_connect(tcp_client_t *cli, const char *host, uint16_t port);
void tcp_client_close(tcp_client_t *cli);
bool tcp_client_is_connected(const tcp_client_t *cli);

/**
 * 发送 JSON 命令并等待响应
 * @param json_cmd  命令 JSON 字符串
 * @return 响应 JSON 字符串（调用者负责 free），失败返回 NULL
 */
char *tcp_client_send_command(tcp_client_t *cli, const char *json_cmd);

/**
 * 接收一帧（用于 monitor 模式）
 * @param type_out  输出消息类型
 * @return JSON 字符串（调用者负责 free），连接断开返回 NULL
 */
char *tcp_client_recv_frame(tcp_client_t *cli, uint8_t *type_out);

#endif /* TCP_CLIENT_H */
