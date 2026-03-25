/**
 * tcp_server.c - TCP IPC 服务器实现
 */
#include "tcp_server.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCK(s) closesocket(s)
#define sock_errno    WSAGetLastError()
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#define CLOSE_SOCK(s) close(s)
#define sock_errno    errno
#endif

/* 大端 uint32 编解码 */
static void put_be32(uint8_t *b, uint32_t v) {
    b[0] = (v >> 24) & 0xFF; b[1] = (v >> 16) & 0xFF;
    b[2] = (v >>  8) & 0xFF; b[3] = v & 0xFF;
}
static uint32_t get_be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) | (uint32_t)b[3];
}

/* 发送完整帧 */
static void send_frame(sock_t fd, uint8_t type, const char *json) {
    if (fd == INVALID_SOCK || !json) return;
    uint32_t payload_len = (uint32_t)strlen(json);
    uint32_t total       = 5 + payload_len; /* 4 len + 1 type + payload */
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return;

    put_be32(buf, payload_len + 1); /* len 包含 type 字节 */
    buf[4] = type;
    memcpy(buf + 5, json, payload_len);

    size_t sent = 0;
    while (sent < total) {
#ifdef _WIN32
        int n = send(fd, (const char *)(buf + sent), (int)(total - sent), 0);
#else
        ssize_t n = send(fd, buf + sent, total - sent, MSG_NOSIGNAL);
#endif
        if (n <= 0) break;
        sent += (size_t)n;
    }
    free(buf);
}

void tcp_server_send_response(sock_t fd, const char *json) {
    send_frame(fd, MSG_TYPE_RESPONSE, json);
}

/* 接收一帧（阻塞读取固定头，然后读取 payload） */
static char *recv_frame(sock_t fd, uint8_t *type_out) {
    uint8_t hdr[5];
    size_t got = 0;
    while (got < 5) {
#ifdef _WIN32
        int n = recv(fd, (char *)(hdr + got), (int)(5 - got), 0);
#else
        ssize_t n = recv(fd, hdr + got, 5 - got, 0);
#endif
        if (n <= 0) return NULL;
        got += (size_t)n;
    }

    uint32_t frame_len   = get_be32(hdr);   /* type + payload */
    if (frame_len < 1 || frame_len > 65536) return NULL;

    *type_out = hdr[4];
    uint32_t payload_len = frame_len - 1;

    char *buf = (char *)malloc(payload_len + 1);
    if (!buf) return NULL;

    got = 0;
    while (got < payload_len) {
#ifdef _WIN32
        int n = recv(fd, buf + got, (int)(payload_len - got), 0);
#else
        ssize_t n = recv(fd, buf + got, payload_len - got, 0);
#endif
        if (n <= 0) { free(buf); return NULL; }
        got += (size_t)n;
    }
    buf[payload_len] = '\0';
    return buf;
}

/* 移除客户端 */
static void remove_client(tcp_server_t *srv, int idx) {
    LOG_INFO("tcp_server", "Client disconnected fd=%d", (int)srv->clients[idx]);
    CLOSE_SOCK(srv->clients[idx]);
    srv->clients[idx]      = INVALID_SOCK;
    srv->monitor_flags[idx] = false;
    srv->client_count--;
}

/* ─── 公开接口 ─── */

int tcp_server_init(tcp_server_t *srv, const char *host, uint16_t port, int max_clients) {
    memset(srv, 0, sizeof(*srv));
    srv->listen_fd   = INVALID_SOCK;
    srv->max_clients = (max_clients > 16) ? 16 : max_clients;
    for (int i = 0; i < 16; i++) srv->clients[i] = INVALID_SOCK;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LOG_ERROR("tcp_server", "WSAStartup failed");
        return -1;
    }
#endif

    srv->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd == INVALID_SOCK) {
        LOG_ERROR("tcp_server", "socket() failed: %d", sock_errno);
        return -1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("tcp_server", "bind() failed: %d", sock_errno);
        CLOSE_SOCK(srv->listen_fd);
        srv->listen_fd = INVALID_SOCK;
        return -1;
    }
    if (listen(srv->listen_fd, 8) < 0) {
        LOG_ERROR("tcp_server", "listen() failed: %d", sock_errno);
        CLOSE_SOCK(srv->listen_fd);
        srv->listen_fd = INVALID_SOCK;
        return -1;
    }

    LOG_INFO("tcp_server", "Listening on %s:%u", host, port);
    return 0;
}

void tcp_server_close(tcp_server_t *srv) {
    for (int i = 0; i < srv->max_clients; i++)
        if (srv->clients[i] != INVALID_SOCK) CLOSE_SOCK(srv->clients[i]);
    if (srv->listen_fd != INVALID_SOCK) CLOSE_SOCK(srv->listen_fd);
#ifdef _WIN32
    WSACleanup();
#endif
}

void tcp_server_set_command_cb(tcp_server_t *srv, command_cb_t cb, void *userdata) {
    srv->on_command = cb;
    srv->userdata   = userdata;
}

sock_t tcp_server_fill_fdset(tcp_server_t *srv, void *read_fds) {
    fd_set *fds = (fd_set *)read_fds;
    sock_t maxfd = srv->listen_fd;
    FD_SET(srv->listen_fd, fds);
    for (int i = 0; i < srv->max_clients; i++) {
        if (srv->clients[i] != INVALID_SOCK) {
            FD_SET(srv->clients[i], fds);
            if (srv->clients[i] > maxfd) maxfd = srv->clients[i];
        }
    }
    return maxfd;
}

void tcp_server_handle(tcp_server_t *srv, void *read_fds) {
    fd_set *fds = (fd_set *)read_fds;

    /* 新连接 */
    if (FD_ISSET(srv->listen_fd, fds)) {
        struct sockaddr_in ca; socklen_t calen = sizeof(ca);
        sock_t cfd = accept(srv->listen_fd, (struct sockaddr *)&ca, &calen);
        if (cfd != INVALID_SOCK) {
            bool accepted = false;
            for (int i = 0; i < srv->max_clients; i++) {
                if (srv->clients[i] == INVALID_SOCK) {
                    srv->clients[i] = cfd;
                    srv->client_count++;
                    LOG_INFO("tcp_server", "Client connected fd=%d from %s",
                             (int)cfd, inet_ntoa(ca.sin_addr));
                    accepted = true;
                    break;
                }
            }
            if (!accepted) {
                const char *err = "{\"error\":\"server full\"}";
                send_frame(cfd, MSG_TYPE_ERROR, err);
                CLOSE_SOCK(cfd);
            }
        }
    }

    /* 已有客户端消息 */
    for (int i = 0; i < srv->max_clients; i++) {
        sock_t cfd = srv->clients[i];
        if (cfd == INVALID_SOCK || !FD_ISSET(cfd, fds)) continue;

        uint8_t type = 0;
        char *json = recv_frame(cfd, &type);
        if (!json) {
            remove_client(srv, i);
            continue;
        }

        /* 检查 monitor 订阅状态（由 command_handler 在 json 中携带特殊字段） */
        if (strstr(json, "\"monitor_start\"") || strstr(json, "monitor_start")) {
            srv->monitor_flags[i] = true;
        } else if (strstr(json, "\"monitor_stop\"") || strstr(json, "monitor_stop")) {
            srv->monitor_flags[i] = false;
        }

        if (srv->on_command) {
            char *resp = srv->on_command(cfd, json, srv->userdata);
            if (resp) {
                send_frame(cfd, MSG_TYPE_RESPONSE, resp);
                free(resp);
            }
        }
        free(json);
    }
}

void tcp_server_broadcast_event(tcp_server_t *srv, const char *json) {
    for (int i = 0; i < srv->max_clients; i++) {
        if (srv->clients[i] != INVALID_SOCK && srv->monitor_flags[i])
            send_frame(srv->clients[i], MSG_TYPE_EVENT, json);
    }
}
