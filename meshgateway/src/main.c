/**
 * main.c - meshgateway 守护进程入口
 *
 * 设计原则（来自 doc/设计需求补充.md）：
 *   - 启动时不自动连接串口（延迟连接模式）
 *   - 串口连接由 CLI/UI 通过 connect_serial 命令触发
 *   - auto_connect=true 时兼容旧行为（向后兼容）
 *
 * 用法:
 *   meshgateway [options]
 *   -c <config>    配置文件路径（默认 meshgateway.conf）
 *   -p <device>    串口设备（覆盖配置文件，且自动连接）
 *   -b <baudrate>  波特率（默认 115200）
 *   -l <level>     日志级别 debug|info|warn|error
 *   -h             显示帮助
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "log.h"
#include "config.h"
#include "tcp_server.h"
#include "web_server.h"
#include "event_loop.h"
#include "command_handler.h"
#include "../libmeshcore/include/serial_port.h"
#include "../libmeshcore/include/frame_parser.h"
#include "../libmeshcore/include/frame_builder.h"
#include "../libmeshcore/include/node_manager.h"
#include "../libmeshcore/include/heartbeat.h"
#include "../libmeshcore/include/proto_parser.h"
#include "../libmeshcore/include/mesh_types.h"

/* ─── 全局退出标志 ─── */
static volatile bool g_running = true;

#ifdef _WIN32
#include <windows.h>
static BOOL WINAPI console_handler(DWORD sig) {
    if (sig == CTRL_C_EVENT || sig == CTRL_BREAK_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
static void sig_handler(int s) {
    (void)s;
    g_running = false;
}
#endif

/* ─── 心跳发送回调 ─── */
static gateway_state_t *g_gstate_for_hb = NULL;

static void heartbeat_send_cb(void *userdata) {
    (void)userdata;
    gateway_state_t *gs = g_gstate_for_hb;
    if (!gs || !gs->serial) return;

    uint8_t proto_buf[64];
    size_t  proto_len = 0;
    if (proto_build_heartbeat(proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return;

    uint8_t frame[68];
    size_t  frame_len = 0;
    if (frame_builder_encode(proto_buf, proto_len, frame, sizeof(frame), &frame_len) != MESH_OK)
        return;

    serial_port_write(gs->serial, frame, frame_len);
    LOG_INFO("main", "Heartbeat sent (%zu bytes)", frame_len);
}

/* ─── 心跳失败回调（重试耗尽）─── */
static void heartbeat_fail_cb(void *userdata) {
    (void)userdata;
    LOG_WARN("main", "Heartbeat: all retries failed, device may be disconnected");
    /* TODO: 触发串口重连逻辑（阶段四实现）*/
}

/* ─── want_config 握手 ─── */
void send_want_config(serial_port_t *sp) {
    uint8_t proto_buf[64];
    size_t  proto_len = 0;
    uint32_t config_id = (uint32_t)time(NULL);
    if (proto_build_want_config(config_id, proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return;

    uint8_t frame[68];
    size_t  frame_len = 0;
    if (frame_builder_encode(proto_buf, proto_len, frame, sizeof(frame), &frame_len) != MESH_OK)
        return;

    serial_port_write(sp, frame, frame_len);
    LOG_INFO("main", "want_config sent (id=%u)", config_id);
}

/* ─── 帮助信息 ─── */
static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -c <file>    Config file (default: meshgateway.conf)\n");
    printf("  -p <device>  Serial device (overrides config; implies auto-connect)\n");
    printf("  -b <baud>    Baud rate (default: 115200)\n");
    printf("  -l <level>   Log level: debug|info|warn|error\n");
    printf("  -h           Show this help\n\n");
    printf("Service starts WITHOUT serial connection by default.\n");
    printf("Use 'meshgate-cli connect --device <dev>' to connect at runtime.\n");
    printf("Set auto_connect=true in config or use -p to auto-connect on start.\n\n");
    printf("Example:\n");
    printf("  %s -c /etc/meshgateway.conf\n", prog);
    printf("  %s -p /dev/ttyUSB0          (auto-connect)\n", prog);
}

/* ─── main ─── */
int main(int argc, char *argv[]) {
    const char *config_path   = "meshgateway.conf";
    const char *serial_device = NULL;  /* 命令行指定的串口（同时 implies auto_connect）*/
    uint32_t    baudrate      = 0;
    const char *log_level_str = NULL;

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            serial_device = argv[++i];
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            baudrate = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            log_level_str = argv[++i];
        }
    }

    /* 加载配置 */
    config_t cfg;
    int cfg_ret = config_load(config_path, &cfg);

    /* 命令行参数覆盖配置文件 */
    if (serial_device) {
        strncpy(cfg.serial_device, serial_device, sizeof(cfg.serial_device) - 1);
        cfg.auto_connect = true;  /* 命令行指定设备 → 自动连接 */
    }
    if (baudrate)      cfg.serial_baudrate = baudrate;
    if (log_level_str) {
        if      (strcmp(log_level_str, "debug") == 0) cfg.log_level = LOG_DEBUG;
        else if (strcmp(log_level_str, "warn")  == 0) cfg.log_level = LOG_WARN;
        else if (strcmp(log_level_str, "error") == 0) cfg.log_level = LOG_ERROR;
        else                                           cfg.log_level = LOG_INFO;
    }

    /* 初始化日志 */
    log_init(cfg.log_level, cfg.log_file[0] ? cfg.log_file : NULL);
    LOG_INFO("main", "meshgateway starting (v%d.%d.%d)",
             MESHCORE_VERSION_MAJOR, MESHCORE_VERSION_MINOR, MESHCORE_VERSION_PATCH);

    if (cfg_ret < 0)
        LOG_WARN("main", "Config file '%s' not found, using defaults", config_path);
    else
        LOG_INFO("main", "Config loaded from '%s'", config_path);

    config_print(&cfg);

    /* 信号处理 */
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    /* 初始化节点管理器 */
    node_manager_init();

    /* 初始化心跳（使用新接口，含超时重试）*/
    heartbeat_init(
        (uint64_t)cfg.heartbeat_interval_s * 1000,
        (uint64_t)cfg.heartbeat_timeout_s  * 1000,
        3,
        heartbeat_send_cb,
        heartbeat_fail_cb,
        NULL
    );

    /* 初始化 TCP 服务器 */
    tcp_server_t tcp;
    if (tcp_server_init(&tcp, cfg.tcp_host, cfg.tcp_port, cfg.tcp_max_clients) < 0) {
        LOG_ERROR("main", "Cannot start TCP server on %s:%u", cfg.tcp_host, cfg.tcp_port);
        log_close();
        return 1;
    }
    LOG_INFO("main", "TCP server listening on %s:%u", cfg.tcp_host, cfg.tcp_port);

    /* 初始化 gateway_state */
    gateway_state_t gstate;
    memset(&gstate, 0, sizeof(gstate));
    gstate.start_time = time(NULL);
    gstate.cfg        = &cfg;
    gstate.serial     = NULL;     /* 延迟连接，初始为 NULL */
    g_gstate_for_hb   = &gstate;

    /* 注册命令处理回调 */
    tcp_server_set_command_cb(&tcp, command_handle, &gstate);

    /* 初始化 Web 服务器（HTTP + WebSocket）*/
    web_server_t web;
    if (web_server_init(&web, cfg.web_bind, cfg.web_static_dir, &gstate) < 0) {
        LOG_WARN("main", "Web server failed to start, continuing without it");
    }

    /* 初始化帧解析器 */
    frame_parser_t parser;
    static uint8_t s_payload_buf[4096];
    static uint8_t s_text_buf[256];
    frame_parser_init(&parser, s_payload_buf, sizeof(s_payload_buf),
                      s_text_buf, sizeof(s_text_buf));

    /* ── 串口连接（仅当 auto_connect=true 时）── */
    if (cfg.auto_connect && cfg.serial_device[0]) {
        serial_port_t *sp = serial_port_open(cfg.serial_device, cfg.serial_baudrate);
        if (!sp) {
            LOG_ERROR("main", "Cannot open serial port: %s (continuing without serial)",
                      cfg.serial_device);
        } else {
            gstate.serial = sp;
            node_manager_gateway_set_connected(true, cfg.serial_device, cfg.serial_baudrate);
            LOG_INFO("main", "Serial port %s opened at %u baud",
                     cfg.serial_device, cfg.serial_baudrate);

            /* 发送 want_config 握手，触发设备推送节点列表 */
            send_want_config(sp);
        }
    } else {
        LOG_INFO("main", "Waiting for connect_serial command (auto_connect=false)");
    }

    /* 事件循环 */
    event_loop_t loop = {
        .cfg     = &cfg,
        .serial  = gstate.serial,   /* 可为 NULL */
        .tcp     = &tcp,
        .web     = &web,
        .parser  = &parser,
        .gstate  = &gstate,
        .running = &g_running
    };
    event_loop_run(&loop);

    /* 清理 */
    LOG_INFO("main", "Shutting down...");
    web_server_close(&web);
    tcp_server_close(&tcp);
    if (gstate.serial) {
        serial_port_close(gstate.serial);
        gstate.serial = NULL;
    }
    node_manager_close();
    log_close();
    return 0;
}
