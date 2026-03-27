# 04_CLI工具设计

> 本文档定义 meshgate-cli 命令行工具的设计与实现。

## 1. 概述

### 1.1 设计目标

- 轻量级，快速启动
- 与服务端解耦，可独立运行
- 支持多种输出格式
- 友好的交互体验

### 1.2 命令格式

```bash
meshgate-cli [global options] <command> [command options] [arguments]
```

---

## 2. 全局选项

| 选项 | 说明 |
|------|------|
| `-s, --socket PATH` | 指定 Socket 路径 (默认: `/run/meshgateway.sock`) |
| `-t, --timeout MS` | 超时时间 (默认: 5000) |
| `-f, --format FORMAT` | 输出格式: table, json, csv (默认: table) |
| `-v, --verbose` | 详细输出 |
| `-q, --quiet` | 静默模式 |
| `-h, --help` | 显示帮助 |
| `--version` | 显示版本 |

---

## 3. 命令列表

### 3.1 服务管理

#### status

显示服务状态。

```bash
meshgate-cli status
meshgate-cli status --json
```

**输出示例:**
```
Service Status:
  Version:     1.0.0
  Uptime:      1h 23m 45s
  Serial:      /dev/ttyUSB0 (connected)
  Nodes:       15
  Clients:     2
```

### 3.2 节点管理

#### nodes

列出所有节点。

```bash
meshgate-cli nodes
meshgate-cli nodes --json
meshgate-cli nodes --format csv
meshgate-cli nodes --sort snr        # 按 SNR 排序
meshgate-cli nodes --filter "battery>50"
```

**输出示例:**
```
ID            Long Name        Short  HW Model   Battery  SNR     Last Heard
!aabbccdd     Node Alpha       NDA    RAK4631    85%      5.5 dB  2 min ago
!ccddeeff     Node Beta        NDB    TBEAM      72%      3.2 dB  5 min ago
!11223344     Node Gamma       NDG    HELTEC     91%      4.1 dB  1 min ago
```

#### node

显示单个节点详情。

```bash
meshgate-cli node !aabbccdd
meshgate-cli node !aabbccdd --json
```

**输出示例:**
```
Node: !aabbccdd
  Long Name:     Node Alpha
  Short Name:    NDA
  Hardware:      RAK4631
  Role:          CLIENT
  
Position:
  Latitude:      31.50000°
  Longitude:     121.50000°
  Altitude:      10 m
  Updated:       2 min ago
  
Device Metrics:
  Battery:       85%
  Voltage:       3.7 V
  Uptime:        1d 2h 30m
  
Signal:
  SNR:           5.5 dB
  RSSI:          -45 dBm
  Hops Away:     1
```

#### node-remove

移除节点。

```bash
meshgate-cli node-remove !aabbccdd
meshgate-cli node-remove !aabbccdd --force
```

### 3.3 消息发送

#### send

发送文本消息。

```bash
meshgate-cli send "Hello World"                           # 广播
meshgate-cli send --to !aabbccdd "Hello"                  # 定向
meshgate-cli send --to !aabbccdd --channel 1 "Hello"      # 指定信道
meshgate-cli send --to broadcast --hop-limit 5 "Hello"    # 指定跳数
meshgate-cli send --want-ack "Important message"          # 要求确认
```

**选项:**
| 选项 | 说明 |
|------|------|
| `--to ID` | 目标节点 (默认: broadcast) |
| `--channel N` | 信道索引 (默认: 0) |
| `--hop-limit N` | 跳数限制 (默认: 3) |
| `--want-ack` | 要求确认 |
| `--reply-id N` | 回复的消息ID |

**输出示例:**
```
Message sent successfully
  Packet ID:  12345678
  To:         broadcast
  Channel:    0
```

#### position

发送位置。

```bash
meshgate-cli position --lat 31.5 --lon 121.5
meshgate-cli position --lat 31.5 --lon 121.5 --alt 10
meshgate-cli position --to !aabbccdd --lat 31.5 --lon 121.5
```

### 3.4 Admin 命令

#### admin

发送 Admin 命令。

```bash
# 获取配置
meshgate-cli admin --to !aabbccdd get-config DEVICE
meshgate-cli admin --to !aabbccdd get-config LORA

# 设置所有者
meshgate-cli admin --to !aabbccdd set-owner --long "My Node" --short "MYN"

# 重启
meshgate-cli admin --to !aabbccdd reboot
meshgate-cli admin --to !aabbccdd reboot --delay 10

# 关机
meshgate-cli admin --to !aabbccdd shutdown --delay 5

# 恢复出厂
meshgate-cli admin --to !aabbccdd factory-reset

# 获取元数据
meshgate-cli admin --to !aabbccdd get-metadata
```

**支持的子命令:**

| 子命令 | 说明 |
|--------|------|
| get-config TYPE | 获取配置 |
| set-owner | 设置所有者名称 |
| reboot | 重启设备 |
| shutdown | 关机 |
| factory-reset | 恢复出厂 |
| get-metadata | 获取固件元数据 |
| get-node-info | 获取节点信息 |
| remove-node NUM | 移除节点 |

### 3.5 设备入网与管理

#### enroll-device

将空设备入网注册到网络。

```bash
# 基本入网
meshgate-cli enroll-device --device !aabbccdd

# 指定公司公钥和网关信息
meshgate-cli enroll-device --device !aabbccdd \
    --company-pubkey <hex_string> \
    --gateway-node !12345678

# JSON 输出
meshgate-cli enroll-device --device !aabbccdd --json
```

**选项:**
| 选项 | 说明 |
|------|------|
| `--device ID` | 目标设备节点ID (必填) |
| `--company-pubkey HEX` | 公司公钥 (32字节hex，默认使用配置) |
| `--gateway-node ID` | 网关节点ID (默认使用本机) |
| `--json` | JSON 输出 |

**输出示例:**
```
Enrolling device !aabbccdd...
Company pubkey: aabbccdd...
Gateway node: !12345678
Status: SUCCESS
Device is now enrolled.
```

#### change-admin

更换设备管理员（需要公司私钥签名）。

```bash
# 更换管理员
meshgate-cli change-admin --device !aabbccdd \
    --new-gateway !87654321 \
    --company-privkey <hex_string>

# 从文件读取公司私钥
meshgate-cli change-admin --device !aabbccdd \
    --new-gateway !87654321 \
    --company-privkey-file /path/to/privkey.bin
```

**选项:**
| 选项 | 说明 |
|------|------|
| `--device ID` | 目标设备节点ID (必填) |
| `--new-gateway ID` | 新网关节点ID (必填) |
| `--company-privkey HEX` | 公司私钥 (32字节hex) |
| `--company-privkey-file PATH` | 公司私钥文件路径 |
| `--json` | JSON 输出 |

**输出示例:**
```
Changing admin for device !aabbccdd...
New gateway: !87654321
Signature: 64-byte Ed25519 signature
Status: SUCCESS
Admin changed successfully.
```

#### get-company-config

获取设备的公司配置状态。

```bash
# 查看设备入网状态
meshgate-cli get-company-config --device !aabbccdd

# JSON 输出
meshgate-cli get-company-config --device !aabbccdd --json
```

**输出示例:**
```
Company Config for !aabbccdd:
  Enrolled:    YES
  Company Key: aabbccdd...
  Last Change: 2024-01-20 10:30:45
```

### 3.6 订阅与监听

#### listen

监听事件并实时输出。

```bash
meshgate-cli listen                            # 监听所有事件
meshgate-cli listen --events node_updated      # 监听特定事件
meshgate-cli listen --events packet_received,telemetry_received
meshgate-cli listen --format json              # JSON 输出
```

**输出示例:**
```
[10:30:45] [node_updated] !aabbccdd position, battery
[10:30:47] [packet_received] TEXT_MESSAGE !aabbccdd -> broadcast "Hello"
[10:30:50] [telemetry_received] !aabbccdd battery=85% voltage=3.7V
[10:31:00] [serial_state_changed] disconnected
[10:31:05] [serial_state_changed] connected
```

**选项:**
| 选项 | 说明 |
|------|------|
| `--events LIST` | 事件列表 (逗号分隔) |
| `--duration SEC` | 监听时长 (默认: 无限) |
| `--count N` | 最多接收 N 条事件 |

### 3.6 配置管理

#### config

查看/修改服务配置。

```bash
# 查看配置
meshgate-cli config
meshgate-cli config --json

# 修改配置
meshgate-cli config --set serial.device=/dev/ttyUSB1
meshgate-cli config --set heartbeat.interval=300

# 重载配置
meshgate-cli config --reload
```

---

## 4. 输出格式

### 4.1 表格格式 (默认)

```
$ meshgate-cli nodes
ID            Long Name        Short  Battery  SNR     Last Heard
!aabbccdd     Node Alpha       NDA    85%      5.5 dB  2 min ago
!ccddeeff     Node Beta        NDB    72%      3.2 dB  5 min ago
```

### 4.2 JSON 格式

```bash
$ meshgate-cli nodes --json
{
  "status": "ok",
  "data": {
    "nodes": [
      {
        "node_id": "!aabbccdd",
        "long_name": "Node Alpha",
        "battery_level": 85,
        "snr": 5.5
      }
    ]
  }
}
```

### 4.3 CSV 格式

```bash
$ meshgate-cli nodes --format csv
node_id,long_name,short_name,battery_level,snr,last_heard
!aabbccdd,Node Alpha,NDA,85,5.5,1705315845
!ccddeeff,Node Beta,NDB,72,3.2,1705315645
```

---

## 5. 程序实现

### 5.1 主程序框架

```c
// cli_main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "client.h"
#include "commands.h"

/* 全局配置 */
static struct {
    char socket_path[256];
    int timeout_ms;
    int format;         /* 0=table, 1=json, 2=csv */
    int verbose;
    int quiet;
} g_config = {
    .socket_path = "/run/meshgateway.sock",
    .timeout_ms = 5000,
    .format = 0,
    .verbose = 0,
    .quiet = 0,
};

/* 命令结构 */
typedef struct {
    const char *name;
    const char *help;
    int (*handler)(client_t *c, int argc, char **argv);
} command_t;

/* 命令列表 */
static command_t commands[] = {
    { "status",              "Show service status",              cmd_status              },
    { "nodes",               "List all nodes",                   cmd_nodes               },
    { "node",                "Show node details",                cmd_node                },
    { "send",                "Send text message",                cmd_send                },
    { "position",            "Send position",                    cmd_position            },
    { "admin",               "Send admin command",               cmd_admin               },
    { "enroll-device",       "Enroll empty device to network",   cmd_enroll_device       },
    { "change-admin",        "Change device administrator",      cmd_change_admin        },
    { "get-company-config",  "Get device company config",        cmd_get_company_config  },
    { "listen",              "Listen to events",                 cmd_listen              },
    { "config",              "View/modify config",               cmd_config              },
    { NULL, NULL, NULL }
};

/* 显示帮助 */
static void show_help(const char *prog) {
    printf("Usage: %s [options] <command> [command-options] [args]\n\n", prog);
    printf("Options:\n");
    printf("  -s, --socket PATH   Socket path (default: /run/meshgateway.sock)\n");
    printf("  -t, --timeout MS    Timeout in ms (default: 5000)\n");
    printf("  -f, --format FMT    Output format: table, json, csv (default: table)\n");
    printf("  -v, --verbose       Verbose output\n");
    printf("  -q, --quiet         Quiet mode\n");
    printf("  -h, --help          Show this help\n");
    printf("      --version       Show version\n");
    printf("\nCommands:\n");
    
    for (int i = 0; commands[i].name; i++) {
        printf("  %-12s  %s\n", commands[i].name, commands[i].help);
    }
    
    printf("\nRun '%s <command> --help' for command details.\n", prog);
}

/* 解析全局选项 */
static int parse_global_options(int *argc, char ***argv) {
    static struct option long_options[] = {
        {"socket",  required_argument, 0, 's'},
        {"timeout", required_argument, 0, 't'},
        {"format",  required_argument, 0, 'f'},
        {"verbose", no_argument,       0, 'v'},
        {"quiet",   no_argument,       0, 'q'},
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(*argc, *argv, "+s:t:f:vqh", long_options, NULL)) != -1) {
        switch (opt) {
            case 's':
                strncpy(g_config.socket_path, optarg, sizeof(g_config.socket_path) - 1);
                break;
            case 't':
                g_config.timeout_ms = atoi(optarg);
                break;
            case 'f':
                if (strcmp(optarg, "json") == 0) g_config.format = 1;
                else if (strcmp(optarg, "csv") == 0) g_config.format = 2;
                break;
            case 'v':
                g_config.verbose = 1;
                break;
            case 'q':
                g_config.quiet = 1;
                break;
            case 'h':
                show_help((*argv)[0]);
                exit(0);
            case 'V':
                printf("meshgate-cli version 1.0.0\n");
                exit(0);
            default:
                return -1;
        }
    }
    
    *argc -= optind;
    *argv += optind;
    
    return 0;
}

/* 主函数 */
int main(int argc, char **argv) {
    /* 解析全局选项 */
    if (parse_global_options(&argc, &argv) != 0) {
        return 1;
    }
    
    if (argc < 1) {
        fprintf(stderr, "Error: No command specified\n");
        show_help(argv[-optind]);
        return 1;
    }
    
    /* 查找命令 */
    command_t *cmd = NULL;
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            cmd = &commands[i];
            break;
        }
    }
    
    if (!cmd) {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[0]);
        return 1;
    }
    
    /* 连接服务 */
    client_t client;
    if (client_connect(&client, g_config.socket_path) != 0) {
        fprintf(stderr, "Error: Failed to connect to service at %s\n", 
                g_config.socket_path);
        return 1;
    }
    
    /* 执行命令 */
    int result = cmd->handler(&client, argc, argv);
    
    /* 断开连接 */
    client_disconnect(&client);
    
    return result;
}
```

### 5.2 命令实现示例

```c
// commands.c
#include "commands.h"
#include "output.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>

extern struct {
    char socket_path[256];
    int timeout_ms;
    int format;
    int verbose;
    int quiet;
} g_config;

/* status 命令 */
int cmd_status(client_t *c, int argc, char **argv) {
    char response[65536];
    
    if (client_call(c, "get_status", NULL, response, sizeof(response), 
                    g_config.timeout_ms) != 0) {
        fprintf(stderr, "Error: Failed to get status\n");
        return 1;
    }
    
    if (g_config.format == 1) {
        printf("%s\n", response);
    } else {
        /* 解析并格式化输出 */
        output_status_table(response);
    }
    
    return 0;
}

/* nodes 命令 */
int cmd_nodes(client_t *c, int argc, char **argv) {
    static struct option long_options[] = {
        {"sort",   required_argument, 0, 's'},
        {"filter", required_argument, 0, 'f'},
        {"json",   no_argument,       0, 'j'},
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    char *sort_field = NULL;
    char *filter = NULL;
    int json_output = 0;
    
    int opt;
    while ((opt = getopt_long(argc, argv, "s:f:jh", long_options, NULL)) != -1) {
        switch (opt) {
            case 's': sort_field = optarg; break;
            case 'f': filter = optarg; break;
            case 'j': json_output = 1; break;
            case 'h':
                printf("Usage: meshgate-cli nodes [options]\n");
                printf("  --sort FIELD    Sort by field\n");
                printf("  --filter EXPR   Filter nodes\n");
                printf("  --json          JSON output\n");
                return 0;
        }
    }
    
    char response[65536];
    char params[256] = "{}";
    
    if (client_call(c, "get_nodes", params, response, sizeof(response),
                    g_config.timeout_ms) != 0) {
        fprintf(stderr, "Error: Failed to get nodes\n");
        return 1;
    }
    
    if (json_output || g_config.format == 1) {
        printf("%s\n", response);
    } else {
        output_nodes_table(response, sort_field, filter);
    }
    
    return 0;
}

/* send 命令 */
int cmd_send(client_t *c, int argc, char **argv) {
    static struct option long_options[] = {
        {"to",        required_argument, 0, 't'},
        {"channel",   required_argument, 0, 'c'},
        {"hop-limit", required_argument, 0, 'l'},
        {"want-ack",  no_argument,       0, 'a'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    char *to = "broadcast";
    int channel = 0;
    int hop_limit = 3;
    int want_ack = 0;
    
    int opt;
    while ((opt = getopt_long(argc, argv, "t:c:l:ah", long_options, NULL)) != -1) {
        switch (opt) {
            case 't': to = optarg; break;
            case 'c': channel = atoi(optarg); break;
            case 'l': hop_limit = atoi(optarg); break;
            case 'a': want_ack = 1; break;
            case 'h':
                printf("Usage: meshgate-cli send [options] <message>\n");
                printf("  --to ID         Target node (default: broadcast)\n");
                printf("  --channel N     Channel index (default: 0)\n");
                printf("  --hop-limit N   Hop limit (default: 3)\n");
                printf("  --want-ack      Request acknowledgment\n");
                return 0;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No message specified\n");
        return 1;
    }
    
    char params[4096];
    snprintf(params, sizeof(params),
        "{\"to\":\"%s\",\"channel\":%d,\"hop_limit\":%d,\"want_ack\":%s,\"text\":\"%s\"}",
        to, channel, hop_limit, want_ack ? "true" : "false", argv[optind]);
    
    char response[65536];
    if (client_call(c, "send_text", params, response, sizeof(response),
                    g_config.timeout_ms) != 0) {
        fprintf(stderr, "Error: Failed to send message\n");
        return 1;
    }
    
    if (!g_config.quiet) {
        printf("Message sent successfully\n");
        if (g_config.verbose) {
            printf("  To:       %s\n", to);
            printf("  Channel:  %d\n", channel);
        }
    }
    
    return 0;
}

/* listen 命令 */
int cmd_listen(client_t *c, int argc, char **argv) {
    static struct option long_options[] = {
        {"events",   required_argument, 0, 'e'},
        {"duration", required_argument, 0, 'd'},
        {"count",    required_argument, 0, 'c'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    char *events = NULL;
    int duration = 0;
    int count = 0;
    
    int opt;
    while ((opt = getopt_long(argc, argv, "e:d:c:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'e': events = optarg; break;
            case 'd': duration = atoi(optarg); break;
            case 'c': count = atoi(optarg); break;
            case 'h':
                printf("Usage: meshgate-cli listen [options]\n");
                printf("  --events LIST   Event types to listen\n");
                printf("  --duration SEC  Listen duration\n");
                printf("  --count N       Max events to receive\n");
                return 0;
        }
    }
    
    /* 订阅事件 */
    char params[1024];
    if (events) {
        snprintf(params, sizeof(params), "{\"events\":[\"%s\"]}", events);
    } else {
        strcpy(params, "{\"events\":[\"node_updated\",\"packet_received\","
                       "\"telemetry_received\",\"serial_state_changed\"]}");
    }
    
    char response[65536];
    if (client_call(c, "subscribe", params, response, sizeof(response),
                    g_config.timeout_ms) != 0) {
        fprintf(stderr, "Error: Failed to subscribe\n");
        return 1;
    }
    
    printf("Listening for events (Ctrl+C to stop)...\n");
    
    /* 接收事件循环 */
    int received = 0;
    time_t start_time = time(NULL);
    
    while (1) {
        if (count > 0 && received >= count) break;
        if (duration > 0 && (time(NULL) - start_time) >= duration) break;
        
        char buf[65536];
        if (client_recv_response(c, 1000, buf, sizeof(buf)) == 0) {
            output_event(buf);
            received++;
        }
    }
    
    return 0;
}
```

---

## 6. 安装

### 6.1 手动安装

```bash
# 编译
make cli

# 安装
sudo install -m 755 meshgate-cli /usr/local/bin/

# 补全脚本
sudo install -m 644 completions/meshgate-cli.bash /etc/bash_completion.d/
```

### 6.2 使用示例

```bash
# 查看状态
meshgate-cli status

# 列出节点
meshgate-cli nodes

# 发送消息
meshgate-cli send --to !aabbccdd "Hello"

# 监听事件
meshgate-cli listen --events packet_received

# 重启远程节点
meshgate-cli admin --to !aabbccdd reboot
```