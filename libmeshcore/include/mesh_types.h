/**
 * mesh_types.h - 公共数据类型定义
 *
 * 本文件定义 libmeshcore 中使用的核心数据类型，
 * 是所有模块的基础头文件。
 */
#ifndef MESH_TYPES_H
#define MESH_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ─────────────────── 版本 ─────────────────── */

#define MESHCORE_VERSION_MAJOR  1
#define MESHCORE_VERSION_MINOR  0
#define MESHCORE_VERSION_PATCH  0

/* ─────────────────── 错误码 ─────────────────── */

typedef enum {
    MESH_OK                  = 0,
    MESH_ERROR_UNKNOWN       = 1,
    MESH_ERROR_INVALID_PARAM = 2,
    MESH_ERROR_TIMEOUT       = 3,
    MESH_ERROR_BUFFER_FULL   = 4,
    MESH_ERROR_NOT_CONNECTED = 5,

    /* 串口错误 (100-) */
    MESH_ERROR_SERIAL_OPEN   = 100,
    MESH_ERROR_SERIAL_READ   = 101,
    MESH_ERROR_SERIAL_WRITE  = 102,

    /* 协议错误 (200-) */
    MESH_ERROR_PROTO_DECODE  = 200,
    MESH_ERROR_PROTO_ENCODE  = 201,
    MESH_ERROR_FRAME_SYNC    = 202,
    MESH_ERROR_FRAME_LEN     = 203,

    /* 节点错误 (300-) */
    MESH_ERROR_NODE_NOT_FOUND = 300,
    MESH_ERROR_NODE_DB_FULL   = 301,
} mesh_error_t;

/* ─────────────────── 节点 ─────────────────── */

#define MESH_MAX_NODES      256
#define MESH_NODE_ID_LEN    12      /* "!aabbccdd\0" = 10 + null */
#define MESH_LONG_NAME_LEN  41
#define MESH_SHORT_NAME_LEN 5
#define MESH_PUBKEY_LEN     32

typedef struct {
    uint32_t node_num;
    char     node_id[MESH_NODE_ID_LEN];     /* "!xxxxxxxx" 格式 */
    char     long_name[MESH_LONG_NAME_LEN];
    char     short_name[MESH_SHORT_NAME_LEN];
    uint8_t  hw_model;
    uint8_t  role;
    uint8_t  public_key[MESH_PUBKEY_LEN];
    bool     has_public_key;

    /* 位置 */
    double   latitude;
    double   longitude;
    int32_t  altitude;
    uint32_t position_time;

    /* 设备状态 */
    uint32_t battery_level;     /* 0-100, 101 = 充电中 */
    float    voltage;
    float    snr;
    int32_t  rssi;
    uint32_t uptime_seconds;

    /* 时间戳（毫秒，CLOCK_MONOTONIC） */
    uint64_t last_heard_ms;
    uint64_t first_seen_ms;
    bool     is_valid;
} mesh_node_t;

/* ─────────────────── 帧事件类型 ─────────────────── */

typedef enum {
    MESH_EVENT_UNKNOWN      = 0,
    MESH_EVENT_PACKET       = 1,    /* 收到 MeshPacket */
    MESH_EVENT_MY_INFO      = 2,    /* 自身节点信息 */
    MESH_EVENT_NODE_INFO    = 3,    /* 网络中节点信息 */
    MESH_EVENT_CONFIG       = 4,    /* 设备配置 */
    MESH_EVENT_MODULE_CONFIG = 5,   /* 模块配置 */
    MESH_EVENT_CHANNEL      = 6,    /* 信道配置 */
    MESH_EVENT_QUEUED_MSG   = 7,    /* 发送队列状态 */
    MESH_EVENT_CONFIG_COMPLETE = 8, /* 配置推送完成 */
    MESH_EVENT_LOG          = 9,    /* 设备日志 */
    MESH_EVENT_REBOOTED     = 10,   /* 设备重启 */
} mesh_event_type_t;

/* PortNum 常量（来自 portnums.proto） */
typedef enum {
    PORTNUM_UNKNOWN        = 0,
    PORTNUM_TEXT_MESSAGE   = 1,
    PORTNUM_REMOTE_HW      = 2,
    PORTNUM_POSITION       = 3,
    PORTNUM_NODEINFO       = 4,
    PORTNUM_ROUTING        = 5,
    PORTNUM_ADMIN          = 6,
    PORTNUM_TEXT_MSG_COMPRESSED = 7,
    PORTNUM_WAYPOINT       = 8,
    PORTNUM_ATAK_PLUGIN    = 9,
    PORTNUM_SERIAL_APP     = 64,
    PORTNUM_STORE_FORWARD  = 65,
    PORTNUM_RANGE_TEST     = 66,
    PORTNUM_TELEMETRY      = 67,
    PORTNUM_ZPS_APP        = 68,
    PORTNUM_PRIVATE_APP    = 256,
    PORTNUM_PRIVATE_CONFIG = 287,
    PORTNUM_WAKEUP_COMM    = 288,
    PORTNUM_MAX            = 511,
} mesh_portnum_t;

/* MeshPacket 摘要（解析后的核心字段） */
#define MESH_TEXT_MAX_LEN   241

typedef struct {
    uint32_t packet_id;
    uint32_t from_num;
    uint32_t to_num;
    char     from_id[MESH_NODE_ID_LEN];     /* "!xxxxxxxx" */
    char     to_id[MESH_NODE_ID_LEN];        /* "!xxxxxxxx" or "broadcast" */
    uint8_t  channel;
    uint8_t  hop_limit;
    int8_t   rx_snr_x4;     /* SNR * 4，避免浮点，实际值 = rx_snr_x4 / 4.0 */
    int32_t  rx_rssi;
    bool     want_ack;
    bool     via_mqtt;

    /* Payload */
    mesh_portnum_t portnum;
    bool     decoded;
    uint8_t  payload[512];
    uint16_t payload_len;

    /* 如果是文本消息，填充此字段 */
    char     text[MESH_TEXT_MAX_LEN];
    bool     is_text;
} mesh_packet_t;

/* 顶层帧事件结构体 */
typedef struct {
    mesh_event_type_t type;
    uint64_t          received_ms;   /* 接收时间戳（毫秒） */

    union {
        mesh_packet_t  packet;       /* MESH_EVENT_PACKET */
        mesh_node_t    node;         /* MESH_EVENT_NODE_INFO */
        uint32_t       my_node_num;  /* MESH_EVENT_MY_INFO */
        char           log_text[256]; /* MESH_EVENT_LOG */
    } data;
} mesh_event_t;

/* ─────────────────── 工具宏 ─────────────────── */

#define MESH_ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

/* 节点号 → "!xxxxxxxx" 字符串 */
static inline void mesh_node_num_to_id(uint32_t num, char *buf, size_t len) {
    if (len >= 10) {
        buf[0] = '!';
        /* 输出8位十六进制（小写）*/
        for (int i = 7; i >= 1; i--) {
            buf[i] = "0123456789abcdef"[num & 0xF];
            num >>= 4;
        }
        buf[9] = '\0';
    }
}

/* "!xxxxxxxx" 字符串 → 节点号 */
static inline uint32_t mesh_node_id_to_num(const char *id) {
    if (!id || id[0] != '!') return 0;
    uint32_t num = 0;
    for (int i = 1; i <= 8; i++) {
        char c = id[i];
        if (c == '\0') break;
        num <<= 4;
        if (c >= '0' && c <= '9') num |= (c - '0');
        else if (c >= 'a' && c <= 'f') num |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') num |= (c - 'A' + 10);
    }
    return num;
}

/* 广播地址 */
#define MESH_BROADCAST_NUM  0xFFFFFFFFu
#define MESH_BROADCAST_ID   "broadcast"

/* 获取单调时钟毫秒数（跨平台） */
uint64_t mesh_time_ms(void);

#endif /* MESH_TYPES_H */
