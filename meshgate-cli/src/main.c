/**
 * main.c - meshgate-cli 命令行工具入口
 *
 * 用法:
 *   meshgate-cli [global-opts] <command> [command-opts]
 *
 * 全局选项:
 *   -s HOST:PORT    守护进程地址（默认 127.0.0.1:9999）
 *   -f table|json|csv  输出格式（默认 table）
 *   -v              详细输出
 *
 * 命令:
 *   status              查看守护进程状态
 *   nodes               列出所有节点
 *   monitor             实时监控数据帧（Ctrl+C 退出）
 *   send --to <id> <text>  发送文本消息
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "tcp_client.h"
#include "output_format.h"

static volatile int g_monitor_running = 1;

#ifdef _WIN32
#include <windows.h>
static BOOL WINAPI ctrl_handler(DWORD sig) {
    if (sig == CTRL_C_EVENT) { g_monitor_running = 0; return TRUE; }
    return FALSE;
}
#else
static void sig_handler(int s) { (void)s; g_monitor_running = 0; }
#endif

static void print_usage(const char *prog) {
    printf("Usage: %s [options] <command>\n\n", prog);
    printf("Options:\n");
    printf("  -s HOST:PORT    Daemon address (default: 127.0.0.1:9999)\n");
    printf("  -f table|json|csv  Output format (default: table)\n");
    printf("  -v              Verbose\n\n");
    printf("Commands:\n");
    printf("  status              Show daemon status\n");
    printf("  nodes               List all nodes\n");
    printf("  monitor             Live frame monitor (Ctrl+C to exit)\n");
    printf("  send --to <id> <text>  Send text message\n\n");
    printf("Examples:\n");
    printf("  %s status\n", prog);
    printf("  %s nodes --json\n", prog);
    printf("  %s -s 192.168.1.10:9999 nodes\n", prog);
    printf("  %s send --to !aabbccdd \"Hello mesh\"\n", prog);
    printf("  %s monitor\n", prog);
}

/* 解析 HOST:PORT，返回0成功 */
static int parse_host_port(const char *s, char *host, size_t host_len, uint16_t *port) {
    const char *colon = strrchr(s, ':');
    if (!colon) return -1;
    size_t hlen = (size_t)(colon - s);
    if (hlen >= host_len) return -1;
    memcpy(host, s, hlen);
    host[hlen] = '\0';
    *port = (uint16_t)atoi(colon + 1);
    return 0;
}

int main(int argc, char *argv[]) {
    char     host[64]   = "127.0.0.1";
    uint16_t port       = 9999;
    output_format_t fmt = FMT_TABLE;
    int  verbose        = 0;
    int  cmd_argc       = 0;
    char *cmd_argv[16]  = {0};

    /* 解析全局选项 */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--socket") == 0) && i+1 < argc) {
            if (parse_host_port(argv[++i], host, sizeof(host), &port) < 0) {
                fprintf(stderr, "Invalid address: %s\n", argv[i]);
                return 1;
            }
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) && i+1 < argc) {
            fmt = output_format_parse(argv[++i]);
        } else if (strcmp(argv[i], "--json") == 0) {
            fmt = FMT_JSON;
        } else if (strcmp(argv[i], "--csv") == 0) {
            fmt = FMT_CSV;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            /* 剩余参数作为子命令及其参数 */
            while (i < argc && cmd_argc < 15)
                cmd_argv[cmd_argc++] = argv[i++];
            break;
        }
    }

    if (cmd_argc == 0) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = cmd_argv[0];

    if (verbose)
        fprintf(stderr, "Connecting to %s:%u ...\n", host, port);

    /* 连接守护进程 */
    tcp_client_t cli;
    if (tcp_client_connect(&cli, host, port) < 0) {
        fprintf(stderr, "Cannot connect to meshgateway at %s:%u\n"
                        "Is the daemon running? (meshgateway -p <device>)\n",
                host, port);
        return 1;
    }

    int exit_code = 0;

    /* ─── status ─── */
    if (strcmp(cmd, "status") == 0) {
        char *resp = tcp_client_send_command(&cli, "{\"cmd\":\"get_status\"}");
        if (!resp) { fprintf(stderr, "No response\n"); exit_code = 1; }
        else { output_status(resp, fmt); free(resp); }
    }

    /* ─── nodes ─── */
    else if (strcmp(cmd, "nodes") == 0) {
        char *resp = tcp_client_send_command(&cli, "{\"cmd\":\"get_nodes\"}");
        if (!resp) { fprintf(stderr, "No response\n"); exit_code = 1; }
        else { output_nodes(resp, fmt); free(resp); }
    }

    /* ─── send ─── */
    else if (strcmp(cmd, "send") == 0) {
        const char *to   = NULL;
        const char *text = NULL;
        int channel = 0;

        for (int i = 1; i < cmd_argc; i++) {
            if ((strcmp(cmd_argv[i], "--to") == 0 || strcmp(cmd_argv[i], "-t") == 0) && i+1 < cmd_argc)
                to = cmd_argv[++i];
            else if ((strcmp(cmd_argv[i], "--channel") == 0 || strcmp(cmd_argv[i], "-c") == 0) && i+1 < cmd_argc)
                channel = atoi(cmd_argv[++i]);
            else if (!text)
                text = cmd_argv[i];
        }

        if (!to || !text) {
            fprintf(stderr, "Usage: send --to <node_id> [--channel N] <text>\n");
            exit_code = 1;
        } else {
            char json[512];
            snprintf(json, sizeof(json),
                "{\"cmd\":\"send_text\",\"to\":\"%s\",\"text\":\"%s\",\"channel\":%d}",
                to, text, channel);
            char *resp = tcp_client_send_command(&cli, json);
            if (!resp) { fprintf(stderr, "No response\n"); exit_code = 1; }
            else {
                if (fmt == FMT_JSON) puts(resp);
                else {
                    if (strstr(resp, "\"ok\""))
                        printf("Sent to %s: %s\n", to, text);
                    else
                        printf("Error: %s\n", resp);
                }
                free(resp);
            }
        }
    }

    /* ─── monitor ─── */
    else if (strcmp(cmd, "monitor") == 0) {
        /* 先订阅 */
        char *resp = tcp_client_send_command(&cli, "{\"cmd\":\"monitor_start\"}");
        if (resp) free(resp);

        printf("Monitoring... (Ctrl+C to exit)\n");
        fflush(stdout);

#ifdef _WIN32
        SetConsoleCtrlHandler(ctrl_handler, TRUE);
#else
        signal(SIGINT, sig_handler);
        signal(SIGTERM, sig_handler);
#endif
        while (g_monitor_running) {
            uint8_t type = 0;
            char *frame = tcp_client_recv_frame(&cli, &type);
            if (!frame) break; /* 连接断开 */
            if (type == MSG_TYPE_EVENT || type == MSG_TYPE_RESPONSE)
                output_event(frame);
            free(frame);
        }

        /* 退订（忽略失败） */
        if (tcp_client_is_connected(&cli)) {
            resp = tcp_client_send_command(&cli, "{\"cmd\":\"monitor_stop\"}");
            if (resp) free(resp);
        }
        printf("\nMonitor stopped.\n");
    }

    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        exit_code = 1;
    }

    tcp_client_close(&cli);
    return exit_code;
}
