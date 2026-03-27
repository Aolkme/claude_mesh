/**
 * mesh_types.h - 公共数据类型定义
 *
 * 本文件定义 libmeshcore 中使用的核心数据类型。
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

/* ─────────────────── 常量 ─────────────────── */

#define MESH_MAX_NODES       256
#define MESH_NODE_ID_LEN     12      /* "!aabbccdd\0" */
#define MESH_LONG_NAME_LEN   41
#define MESH_SHORT_NAME_LEN  5
#define MESH_PUBKEY_LEN      32
#define MESH_TEXT_MAX_LEN    241
#define MESH_DEVICE_NAME_LEN 64

/* ─────────────────── PortNum ─────────────────── */

typedef enum {
    PORTNUM_UNKNOWN              = 0,
    PORTNUM_TEXT_MESSAGE         = 1,
    PORTNUM_REMOTE_HW            = 2,
    PORTNUM_POSITION             = 3,
    PORTNUM_NODEINFO             = 4,
    PORTNUM_ROUTING              = 5,
    PORTNUM_ADMIN                = 6,
    PORTNUM_TEXT_MSG_COMPRESSED  = 7,
    PORTNUM_WAYPOINT             = 8,
    PORTNUM_ATAK_PLUGIN          = 9,
    PORTNUM_SERIAL_APP           = 64,
    PORTNUM_STORE_FORWARD        = 65,
    PORTNUM_RANGE_TEST           = 66,
    PORTNUM_TELEMETRY            = 67,
    PORTNUM_ZPS_APP              = 68,
    PORTNUM_PRIVATE_APP          = 256,
    PORTNUM_PRIVATE_CONFIG       = 287,  /* thingseye PrivateConfigPacket */
    PORTNUM_WAKEUP_COMM          = 288,  /* thingseye WakeupComm */
    PORTNUM_MAX                  = 511,
} mesh_portnum_t;

/* ─────────────────── 远程节点表（含私有配置字段）─────────────────── */

typedef struct {
    /* --- Meshtastic 标准字段 --- */
    uint32_t node_num;
    char     node_id[MESH_NODE_ID_LEN];      /* "!xxxxxxxx" 格式 */
    char     long_name[MESH_LONG_NAME_LEN];
    char     short_name[MESH_SHORT_NAME_LEN];
    uint8_t  hw_model;
    uint8_t  role;
    uint8_t  public_key[MESH_PUBKEY_LEN];
    bool     has_public_key;

    /* 位置（来自 POSITION_APP 或 NODE_INFO）*/
    double   latitude;
    double   longitude;
    int32_t  altitude;
    uint32_t position_time;

    /* 遥测（来自 TELEMETRY_APP）*/
    uint32_t battery_level;        /* 0-100，101=充电中 */
    float    voltage;
    uint32_t uptime_seconds;
    float    channel_utilization;
    float    air_util_tx;

    /* 信号（来自 MeshPacket 元数据）*/
    float    snr;
    int32_t  rssi;
    uint8_t  hops_away;

    /* 时间戳（毫秒，CLOCK_MONOTONIC）*/
    uint64_t last_heard_ms;
    uint64_t first_seen_ms;

    /* --- thingseye 私有配置字段 --- */
    bool     is_enrolled;                       /* CompanyConfig.is_enrolled */
    uint8_t  company_public_key[MESH_PUBKEY_LEN]; /* CompanyConfig.company_public_key */
    uint64_t last_admin_change_ts;              /* CompanyConfig.last_change_timestamp */
    bool     is_admin_key_set;                  /* PrivateConfig.isadminkeyset */
    uint32_t private_config_version;            /* PrivateConfig.privateversion */
    char     device_name[MESH_DEVICE_NAME_LEN]; /* PrivateConfig.devicename */

    bool     is_valid;
} mesh_node_t;

/* ─────────────────── 网关节点表（自身，单条）─────────────────── */

typedef struct {
    /* 基本信息（来自 MY_INFO + NODE_INFO 握手）*/
    uint32_t node_num;
    char     node_id[MESH_NODE_ID_LEN];
    char     long_name[MESH_LONG_NAME_LEN];
    char     short_name[MESH_SHORT_NAME_LEN];
    uint8_t  hw_model;
    uint8_t  role;

    /* PKI 密钥对（本地持久化）*/
    uint8_t  public_key[MESH_PUBKEY_LEN];
    uint8_t  private_key[MESH_PUBKEY_LEN];
    bool     has_keypair;

    /* 连接状态 */
    bool     serial_connected;
    bool     config_complete;          /* 握手是否已完成 */
    uint64_t connect_time_ms;
    char     serial_device[64];
    uint32_t serial_baudrate;
} mesh_gateway_node_t;

/* ─────────────────── 帧事件类型 ─────────────────── */

typedef enum {
    MESH_EVENT_UNKNOWN         = 0,
    MESH_EVENT_PACKET          = 1,   /* 收到 MeshPacket（含解析后 payload）*/
    MESH_EVENT_MY_INFO         = 2,   /* 网关自身节点信息 */
    MESH_EVENT_NODE_INFO       = 3,   /* 网络中其他节点信息 */
    MESH_EVENT_CONFIG          = 4,   /* 设备配置（握手期）*/
    MESH_EVENT_MODULE_CONFIG   = 5,   /* 模块配置（握手期）*/
    MESH_EVENT_CHANNEL         = 6,   /* 信道配置（握手期）*/
    MESH_EVENT_QUEUED_MSG      = 7,   /* 发送队列状态 */
    MESH_EVENT_CONFIG_COMPLETE = 8,   /* 握手推送完成 */
    MESH_EVENT_LOG             = 9,   /* 设备 proto 日志 */
    MESH_EVENT_REBOOTED        = 10,  /* 设备重启 */
} mesh_event_type_t;

/* ─────────────────── MeshPacket（含解析后 payload）─────────────────── */

typedef struct {
    /* 包头字段 */
    uint32_t       packet_id;
    uint32_t       from_num;
    uint32_t       to_num;
    char           from_id[MESH_NODE_ID_LEN];
    char           to_id[MESH_NODE_ID_LEN];    /* "!xxxxxxxx" 或 "broadcast" */
    uint8_t        channel;
    uint8_t        hop_limit;
    uint8_t        hop_start;
    int8_t         rx_snr_x4;     /* SNR * 4，实际值 = rx_snr_x4 / 4.0 */
    int32_t        rx_rssi;
    bool           want_ack;
    bool           via_mqtt;
    bool           pki_encrypted;

    /* Payload 标识 */
    mesh_portnum_t portnum;
    bool           decoded;

    /* 原始 payload（未解析的二进制）*/
    uint8_t        payload[512];
    uint16_t       payload_len;

    /* ── 按 PortNum 解析后的结构化数据 ── */

    /* TEXT_MESSAGE(1)：文本内容 */
    char           text[MESH_TEXT_MAX_LEN];
    bool           is_text;

    /* POSITION(3)：位置信息 */
    struct {
        double   latitude;
        double   longitude;
        int32_t  altitude;
        uint32_t timestamp;
        uint32_t pdop;           /* Position dilution of precision * 100 */
        bool     valid;
    } position;

    /* NODEINFO(4)：用户信息 */
    struct {
        char     long_name[MESH_LONG_NAME_LEN];
        char     short_name[MESH_SHORT_NAME_LEN];
        uint8_t  hw_model;
        uint8_t  role;
        uint8_t  public_key[MESH_PUBKEY_LEN];
        bool     has_public_key;
        bool     valid;
    } user;

    /* TELEMETRY(67)：遥测数据 */
    struct {
        uint32_t battery_level;
        float    voltage;
        float    channel_utilization;
        float    air_util_tx;
        uint32_t uptime_seconds;
        float    temperature;
        float    relative_humidity;
        float    barometric_pressure;
        bool     has_device_metrics;
        bool     has_environment_metrics;
        bool     valid;
    } telemetry;

    /* ROUTING(5)：路由状态 */
    struct {
        uint8_t  error_reason;   /* Routing.Error 枚举值 */
        bool     valid;
    } routing;

    /* ADMIN(6)：AdminMessage 响应（原始序列化，留给 admin_builder 解析）*/
    uint8_t        admin_payload[512];
    uint16_t       admin_payload_len;

    /* PRIVATE_CONFIG(287)：私有配置响应（原始序列化，留给 private_config_handler 解析）*/
    uint8_t        private_payload[512];
    uint16_t       private_payload_len;

} mesh_packet_t;

/* ─────────────────── 顶层帧事件 ─────────────────── */

typedef struct {
    mesh_event_type_t type;
    uint64_t          received_ms;

    union {
        mesh_packet_t  packet;          /* MESH_EVENT_PACKET */
        mesh_node_t    node;            /* MESH_EVENT_NODE_INFO */
        uint32_t       my_node_num;     /* MESH_EVENT_MY_INFO */
        char           log_text[256];   /* MESH_EVENT_LOG */
    } data;
} mesh_event_t;

/* ─────────────────── 工具宏与内联函数 ─────────────────── */

#define MESH_ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

/* 广播地址 */
#define MESH_BROADCAST_NUM  0xFFFFFFFFu
#define MESH_BROADCAST_ID   "broadcast"

/* 节点号 → "!xxxxxxxx" 字符串 */
static inline void mesh_node_num_to_id(uint32_t num, char *buf, size_t len) {
    if (len >= 10) {
        buf[0] = '!';
        for (int i = 8; i >= 1; i--) {
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
        if      (c >= '0' && c <= '9') num |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') num |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') num |= (uint32_t)(c - 'A' + 10);
    }
    return num;
}

/* 获取单调时钟毫秒数（跨平台，在 mesh_types.c / serial_port.c 中实现）*/
uint64_t mesh_time_ms(void);

#endif /* MESH_TYPES_H */
