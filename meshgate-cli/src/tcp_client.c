/**
 * tcp_client.c - CLI TCP 客户端实现
 */
#include "tcp_client.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCK(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCK(s) close(s)
#endif

static void put_be32(uint8_t *b, uint32_t v) {
    b[0]=(v>>24)&0xFF; b[1]=(v>>16)&0xFF; b[2]=(v>>8)&0xFF; b[3]=v&0xFF;
}
static uint32_t get_be32(const uint8_t *b) {
    return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|(uint32_t)b[3];
}

int tcp_client_connect(tcp_client_t *cli, const char *host, uint16_t port) {
    cli->fd = INVALID_SOCK;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return -1;
#endif

    cli->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cli->fd == INVALID_SOCK) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (connect(cli->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        CLOSE_SOCK(cli->fd);
        cli->fd = INVALID_SOCK;
        return -1;
    }
    return 0;
}

void tcp_client_close(tcp_client_t *cli) {
    if (cli->fd != INVALID_SOCK) {
        CLOSE_SOCK(cli->fd);
        cli->fd = INVALID_SOCK;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

bool tcp_client_is_connected(const tcp_client_t *cli) {
    return cli->fd != INVALID_SOCK;
}

/* 发送完整帧 */
static int send_frame(sock_t fd, uint8_t type, const char *json) {
    uint32_t payload_len = (uint32_t)strlen(json);
    uint32_t total = 5 + payload_len;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return -1;
    put_be32(buf, payload_len + 1);
    buf[4] = type;
    memcpy(buf + 5, json, payload_len);

    size_t sent = 0;
    while (sent < total) {
#ifdef _WIN32
        int n = send(fd, (const char *)(buf + sent), (int)(total - sent), 0);
#else
        ssize_t n = send(fd, buf + sent, total - sent, 0);
#endif
        if (n <= 0) { free(buf); return -1; }
        sent += (size_t)n;
    }
    free(buf);
    return 0;
}

/* 接收完整帧 */
char *tcp_client_recv_frame(tcp_client_t *cli, uint8_t *type_out) {
    uint8_t hdr[5];
    size_t got = 0;
    while (got < 5) {
#ifdef _WIN32
        int n = recv(cli->fd, (char *)(hdr + got), (int)(5 - got), 0);
#else
        ssize_t n = recv(cli->fd, hdr + got, 5 - got, 0);
#endif
        if (n <= 0) return NULL;
        got += (size_t)n;
    }

    uint32_t frame_len = get_be32(hdr);
    if (frame_len < 1 || frame_len > 65536) return NULL;
    *type_out = hdr[4];
    uint32_t payload_len = frame_len - 1;

    char *buf = (char *)malloc(payload_len + 1);
    if (!buf) return NULL;
    got = 0;
    while (got < payload_len) {
#ifdef _WIN32
        int n = recv(cli->fd, buf + got, (int)(payload_len - got), 0);
#else
        ssize_t n = recv(cli->fd, buf + got, payload_len - got, 0);
#endif
        if (n <= 0) { free(buf); return NULL; }
        got += (size_t)n;
    }
    buf[payload_len] = '\0';
    return buf;
}

char *tcp_client_send_command(tcp_client_t *cli, const char *json_cmd) {
    if (send_frame(cli->fd, MSG_TYPE_REQUEST, json_cmd) < 0) return NULL;
    uint8_t type = 0;
    return tcp_client_recv_frame(cli, &type);
}
