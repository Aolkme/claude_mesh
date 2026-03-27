/**
 * main.c - meshgate-cli 命令行工具入口
 *
 * 用法:
 *   meshgate-cli [global-opts] <command> [command-opts]
 *
 * 全局选项:
 *   -s HOST:PORT       守护进程地址（默认 127.0.0.1:9999）
 *   -f table|json|csv  输出格式（默认 table）
 *   --json / --csv     格式快捷方式
 *   -v                 详细输出
 *
 * 命令:
 *   status                          守护进程状态
 *   gateway-info                    网关节点详细信息
 *   nodes                           所有节点列表
 *   node --id <node_id>             单节点详情
 *   connect --device <dev> [--baudrate N]  连接串口
 *   disconnect                      断开串口
 *   send --to <id> [--channel N] [--ack] <text>      发送文本
 *   send-pos --to <id> --lat N --lon N [--alt N] [--channel N]  发送位置
 *   monitor                         实时事件监控（Ctrl+C 退出）
 *   admin passkey --node <id>       获取 Session Passkey
 *   admin get-config --node <id> [--type N] [--passkey N]   读设备配置
 *   admin get-channel --node <id> [--index N] [--passkey N] 读信道配置
 *   admin reboot --node <id> [--delay N] [--passkey N]      重启节点
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "tcp_client.h"
#include "output_format.h"
#include "cJSON.h"

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
    printf("  -s HOST:PORT           Daemon address (default: 127.0.0.1:9999)\n");
    printf("  -f table|json|csv      Output format (default: table)\n");
    printf("  --json / --csv         Format shortcuts\n");
    printf("  -v                     Verbose\n\n");
    printf("Commands:\n");
    printf("  status                                    Daemon status\n");
    printf("  gateway-info                              Gateway node info\n");
    printf("  nodes                                     List all nodes\n");
    printf("  node --id <node_id>                       Single node detail\n");
    printf("  connect --device <dev> [--baudrate N]     Connect serial port\n");
    printf("  disconnect                                 Disconnect serial\n");
    printf("  send --to <id> [--channel N] [--ack] <text>  Send text message\n");
    printf("  send-pos --to <id> --lat N --lon N [--alt N] [--channel N]\n");
    printf("  monitor                                   Live event monitor\n");
    printf("  admin passkey --node <id>\n");
    printf("  admin get-config --node <id> [--type N] [--passkey N]\n");
    printf("  admin get-channel --node <id> [--index N] [--passkey N]\n");
    printf("  admin reboot --node <id> [--delay N] [--passkey N]\n\n");
    printf("Examples:\n");
    printf("  %s status\n", prog);
    printf("  %s connect --device /dev/ttyUSB0\n", prog);
    printf("  %s nodes --json\n", prog);
    printf("  %s node --id !aabbccdd\n", prog);
    printf("  %s send --to !aabbccdd \"Hello\"\n", prog);
    printf("  %s send-pos --to broadcast --lat 39.9 --lon 116.4\n", prog);
    printf("  %s monitor\n", prog);
    printf("  %s admin reboot --node !aabbccdd --delay 5\n", prog);
}

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

/* 发送命令并打印响应，返回 0 = ok，1 = error */
static int do_cmd(tcp_client_t *cli, const char *json, output_format_t fmt,
                  void (*render)(const char *, output_format_t))
{
    char *resp = tcp_client_send_command(cli, json);
    if (!resp) { fprintf(stderr, "No response from daemon\n"); return 1; }
    if (render)
        render(resp, fmt);
    else if (fmt == FMT_JSON)
        puts(resp);
    else {
        /* 通用：检查 status 字段 */
        if (strstr(resp, "\"error\""))
            fprintf(stderr, "Error: %s\n", resp);
        else
            puts(resp);
    }
    free(resp);
    return 0;
}

/* ─── arg helpers ─── */
static const char *arg_get(char **av, int ac, const char *flag) {
    for (int i = 0; i < ac - 1; i++)
        if (strcmp(av[i], flag) == 0) return av[i+1];
    return NULL;
}
static int arg_has(char **av, int ac, const char *flag) {
    for (int i = 0; i < ac; i++)
        if (strcmp(av[i], flag) == 0) return 1;
    return 0;
}

int main(int argc, char *argv[]) {
    char     host[64]   = "127.0.0.1";
    uint16_t port       = 9999;
    output_format_t fmt = FMT_TABLE;
    int  verbose        = 0;
    int  cmd_argc       = 0;
    char *cmd_argv[32]  = {0};

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0) && i+1 < argc) {
            if (parse_host_port(argv[++i], host, sizeof(host), &port) < 0) {
                fprintf(stderr, "Invalid address: %s\n", argv[i]);
                return 1;
            }
        } else if ((strcmp(argv[i], "-f") == 0) && i+1 < argc) {
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
            while (i < argc && cmd_argc < 31)
                cmd_argv[cmd_argc++] = argv[i++];
            break;
        }
    }

    if (cmd_argc == 0) { print_usage(argv[0]); return 1; }

    const char *cmd = cmd_argv[0];

    if (verbose)
        fprintf(stderr, "Connecting to %s:%u ...\n", host, port);

    tcp_client_t cli;
    if (tcp_client_connect(&cli, host, port) < 0) {
        fprintf(stderr, "Cannot connect to meshgateway at %s:%u\n"
                        "Is the daemon running?\n", host, port);
        return 1;
    }

    int exit_code = 0;
    char json[1024];

    /* ─── status ─── */
    if (strcmp(cmd, "status") == 0) {
        exit_code = do_cmd(&cli, "{\"cmd\":\"get_status\"}", fmt, output_status);
    }

    /* ─── gateway-info ─── */
    else if (strcmp(cmd, "gateway-info") == 0) {
        exit_code = do_cmd(&cli, "{\"cmd\":\"get_gateway_info\"}", fmt, NULL);
    }

    /* ─── nodes ─── */
    else if (strcmp(cmd, "nodes") == 0) {
        exit_code = do_cmd(&cli, "{\"cmd\":\"get_nodes\"}", fmt, output_nodes);
    }

    /* ─── node --id <id> ─── */
    else if (strcmp(cmd, "node") == 0) {
        const char *id = arg_get(cmd_argv, cmd_argc, "--id");
        if (!id) { fprintf(stderr, "Usage: node --id <node_id>\n"); exit_code = 1; }
        else {
            snprintf(json, sizeof(json), "{\"cmd\":\"get_node\",\"node_id\":\"%s\"}", id);
            exit_code = do_cmd(&cli, json, fmt, NULL);
        }
    }

    /* ─── connect --device <dev> [--baudrate N] ─── */
    else if (strcmp(cmd, "connect") == 0) {
        const char *dev  = arg_get(cmd_argv, cmd_argc, "--device");
        const char *baud = arg_get(cmd_argv, cmd_argc, "--baudrate");
        if (!dev) { fprintf(stderr, "Usage: connect --device <dev>\n"); exit_code = 1; }
        else {
            if (baud)
                snprintf(json, sizeof(json),
                    "{\"cmd\":\"connect_serial\",\"device\":\"%s\",\"baudrate\":%s}",
                    dev, baud);
            else
                snprintf(json, sizeof(json),
                    "{\"cmd\":\"connect_serial\",\"device\":\"%s\"}", dev);
            exit_code = do_cmd(&cli, json, fmt, NULL);
        }
    }

    /* ─── disconnect ─── */
    else if (strcmp(cmd, "disconnect") == 0) {
        exit_code = do_cmd(&cli, "{\"cmd\":\"disconnect_serial\"}", fmt, NULL);
    }

    /* ─── send ─── */
    else if (strcmp(cmd, "send") == 0) {
        const char *to      = arg_get(cmd_argv, cmd_argc, "--to");
        const char *ch_str  = arg_get(cmd_argv, cmd_argc, "--channel");
        int         want_ack = arg_has(cmd_argv, cmd_argc, "--ack");
        /* 文本是最后一个非 flag 参数 */
        const char *text = NULL;
        for (int i = 1; i < cmd_argc; i++) {
            if (cmd_argv[i][0] != '-') { text = cmd_argv[i]; }
            else if ((strcmp(cmd_argv[i], "--to") == 0 ||
                      strcmp(cmd_argv[i], "--channel") == 0) && i+1 < cmd_argc)
                i++;  /* skip value */
        }
        if (!to || !text) { fprintf(stderr, "Usage: send --to <id> <text>\n"); exit_code = 1; }
        else {
            snprintf(json, sizeof(json),
                "{\"cmd\":\"send_text\",\"to\":\"%s\",\"text\":\"%s\","
                "\"channel\":%s,\"want_ack\":%s}",
                to, text, ch_str ? ch_str : "0", want_ack ? "true" : "false");
            exit_code = do_cmd(&cli, json, fmt, NULL);
        }
    }

    /* ─── send-pos ─── */
    else if (strcmp(cmd, "send-pos") == 0) {
        const char *to  = arg_get(cmd_argv, cmd_argc, "--to");
        const char *lat = arg_get(cmd_argv, cmd_argc, "--lat");
        const char *lon = arg_get(cmd_argv, cmd_argc, "--lon");
        const char *alt = arg_get(cmd_argv, cmd_argc, "--alt");
        const char *ch  = arg_get(cmd_argv, cmd_argc, "--channel");
        if (!to || !lat || !lon) {
            fprintf(stderr, "Usage: send-pos --to <id> --lat N --lon N [--alt N]\n");
            exit_code = 1;
        } else {
            snprintf(json, sizeof(json),
                "{\"cmd\":\"send_position\",\"to\":\"%s\","
                "\"lat\":%s,\"lon\":%s,\"alt\":%s,\"channel\":%s}",
                to, lat, lon, alt ? alt : "0", ch ? ch : "0");
            exit_code = do_cmd(&cli, json, fmt, NULL);
        }
    }

    /* ─── admin <subcommand> ─── */
    else if (strcmp(cmd, "admin") == 0) {
        const char *sub = (cmd_argc > 1) ? cmd_argv[1] : "";

        if (strcmp(sub, "passkey") == 0) {
            const char *node = arg_get(cmd_argv, cmd_argc, "--node");
            if (!node) { fprintf(stderr, "Usage: admin passkey --node <id>\n"); exit_code = 1; }
            else {
                snprintf(json, sizeof(json),
                    "{\"cmd\":\"admin_get_session_passkey\",\"node_id\":\"%s\"}", node);
                exit_code = do_cmd(&cli, json, fmt, NULL);
            }
        }
        else if (strcmp(sub, "get-config") == 0) {
            const char *node = arg_get(cmd_argv, cmd_argc, "--node");
            const char *type = arg_get(cmd_argv, cmd_argc, "--type");
            const char *pk   = arg_get(cmd_argv, cmd_argc, "--passkey");
            if (!node) { fprintf(stderr, "Usage: admin get-config --node <id> [--type N] [--passkey N]\n"); exit_code = 1; }
            else {
                snprintf(json, sizeof(json),
                    "{\"cmd\":\"admin_get_config\",\"node_id\":\"%s\","
                    "\"config_type\":%s,\"passkey\":%s}",
                    node, type ? type : "0", pk ? pk : "0");
                exit_code = do_cmd(&cli, json, fmt, NULL);
            }
        }
        else if (strcmp(sub, "get-channel") == 0) {
            const char *node  = arg_get(cmd_argv, cmd_argc, "--node");
            const char *index = arg_get(cmd_argv, cmd_argc, "--index");
            const char *pk    = arg_get(cmd_argv, cmd_argc, "--passkey");
            if (!node) { fprintf(stderr, "Usage: admin get-channel --node <id> [--index N] [--passkey N]\n"); exit_code = 1; }
            else {
                snprintf(json, sizeof(json),
                    "{\"cmd\":\"admin_get_channel\",\"node_id\":\"%s\","
                    "\"channel_index\":%s,\"passkey\":%s}",
                    node, index ? index : "0", pk ? pk : "0");
                exit_code = do_cmd(&cli, json, fmt, NULL);
            }
        }
        else if (strcmp(sub, "reboot") == 0) {
            const char *node  = arg_get(cmd_argv, cmd_argc, "--node");
            const char *delay = arg_get(cmd_argv, cmd_argc, "--delay");
            const char *pk    = arg_get(cmd_argv, cmd_argc, "--passkey");
            if (!node) { fprintf(stderr, "Usage: admin reboot --node <id> [--delay N] [--passkey N]\n"); exit_code = 1; }
            else {
                snprintf(json, sizeof(json),
                    "{\"cmd\":\"admin_reboot\",\"node_id\":\"%s\","
                    "\"delay_s\":%s,\"passkey\":%s}",
                    node, delay ? delay : "5", pk ? pk : "0");
                exit_code = do_cmd(&cli, json, fmt, NULL);
            }
        }
        else {
            fprintf(stderr, "Unknown admin subcommand: %s\n", sub);
            fprintf(stderr, "Available: passkey, get-config, get-channel, reboot\n");
            exit_code = 1;
        }
    }

    /* ─── monitor ─── */
    else if (strcmp(cmd, "monitor") == 0) {
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
            if (!frame) break;
            if (type == MSG_TYPE_EVENT || type == MSG_TYPE_RESPONSE)
                output_event(frame);
            free(frame);
        }

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
