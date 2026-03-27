# 03_Socket通信协议

> 本文档定义 meshgateway 服务与客户端之间的通信协议。

## 1. 协议概述

### 1.1 传输层

| 配置项 | 值 |
|--------|-----|
| 协议 | Unix Domain Socket |
| 类型 | SOCK_STREAM (流式) |
| 路径 | `/run/meshgateway.sock` |
| 字节序 | 大端 (长度字段) |

### 1.2 帧格式

```
┌────────────┬────────────┬──────────────────────┐
│ Length     │ Type       │ Payload              │
│ (4 bytes)  │ (1 byte)   │ (N bytes)            │
│ Big Endian │            │ JSON UTF-8           │
└────────────┴────────────┴──────────────────────┘

Length: 不包含自身 (4字节)，仅 Payload + Type 的长度
Type:
  0x01 = JSON 请求/响应
  0x02 = JSON 事件推送
```

### 1.3 JSON 消息类型

| 类型 | 方向 | 说明 |
|------|------|------|
| Request | Client → Server | 客户端请求 |
| Response | Server → Client | 服务端响应 |
| Event | Server → Client | 服务端推送事件 |

---

## 2. 请求/响应格式

### 2.1 请求格式

```json
{
    "id": 1,                    // 请求ID (必填, 用于匹配响应)
    "cmd": "get_nodes",         // 命令名称 (必填)
    "params": {                 // 命令参数 (可选)
        ...
    }
}
```

### 2.2 响应格式 (成功)

```json
{
    "id": 1,                    // 对应请求ID
    "status": "ok",             // 状态
    "data": {                   // 响应数据
        ...
    }
}
```

### 2.3 响应格式 (失败)

```json
{
    "id": 1,
    "status": "error",
    "error": {
        "code": 300,            // 错误码
        "message": "Node not found",
        "detail": "..."         // 详细信息 (可选)
    }
}
```

---

## 3. 命令定义

### 3.1 服务状态命令

#### get_status

获取服务运行状态。

**请求:**
```json
{
    "id": 1,
    "cmd": "get_status"
}
```

**响应:**
```json
{
    "id": 1,
    "status": "ok",
    "data": {
        "version": "1.0.0",
        "uptime_seconds": 3600,
        "serial_state": "connected",
        "serial_device": "/dev/ttyUSB0",
        "client_count": 2,
        "node_count": 15,
        "last_packet_time": 1705315845
    }
}
```

### 3.2 节点管理命令

#### get_nodes

获取所有节点列表。

**请求:**
```json
{
    "id": 2,
    "cmd": "get_nodes",
    "params": {
        "include_expired": false   // 可选，是否包含过期节点
    }
}
```

**响应:**
```json
{
    "id": 2,
    "status": "ok",
    "data": {
        "nodes": [
            {
                "node_id": "!aabbccdd",
                "node_num": 2863311530,
                "long_name": "Node Alpha",
                "short_name": "NDA",
                "hw_model": 13,
                "role": 0,
                "battery_level": 85,
                "voltage": 3.7,
                "latitude": 31.5,
                "longitude": 121.5,
                "altitude": 10,
                "snr": 5.5,
                "rssi": -45,
                "last_heard": 1705315845,
                "hops_away": 1
            }
        ],
        "total": 15
    }
}
```

#### get_node

获取单个节点详情。

**请求:**
```json
{
    "id": 3,
    "cmd": "get_node",
    "params": {
        "node_id": "!aabbccdd"
    }
}
```

**响应:**
```json
{
    "id": 3,
    "status": "ok",
    "data": {
        "node_id": "!aabbccdd",
        "node_num": 2863311530,
        "long_name": "Node Alpha",
        "short_name": "NDA",
        "hw_model": 13,
        "hw_model_name": "RAK4631",
        "role": 0,
        "role_name": "CLIENT",
        "public_key": "base64...",
        "battery_level": 85,
        "voltage": 3.7,
        "channel_utilization": 2.5,
        "air_util_tx": 0.1,
        "latitude": 31.5,
        "longitude": 121.5,
        "altitude": 10,
        "position_time": 1705315845,
        "snr": 5.5,
        "rssi": -45,
        "last_heard": 1705315845,
        "hops_away": 1,
        "is_favorite": false,
        "is_ignored": false
    }
}
```

### 3.3 消息发送命令

#### send_text

发送文本消息。

**请求:**
```json
{
    "id": 4,
    "cmd": "send_text",
    "params": {
        "to": "!aabbccdd",        // 或 "broadcast"
        "channel": 0,
        "text": "Hello World",
        "want_ack": false,        // 可选
        "hop_limit": 3            // 可选
    }
}
```

**响应:**
```json
{
    "id": 4,
    "status": "ok",
    "data": {
        "packet_id": 12345678,
        "queued": true
    }
}
```

#### send_position

发送位置信息。

**请求:**
```json
{
    "id": 5,
    "cmd": "send_position",
    "params": {
        "to": "broadcast",
        "channel": 0,
        "latitude": 31.5,
        "longitude": 121.5,
        "altitude": 10,
        "time": 1705315845
    }
}
```

### 3.4 Admin 命令

#### send_admin

发送 Admin 消息。

**请求:**
```json
{
    "id": 6,
    "cmd": "send_admin",
    "params": {
        "to": "!aabbccdd",
        "command": "get_config",
        "params": {
            "config_type": "DEVICE_CONFIG"
        },
        "want_response": true
    }
}
```

**支持的 Admin 命令:**

| command | params | 说明 |
|---------|--------|------|
| get_config | config_type | 获取配置 |
| set_owner | long_name, short_name | 设置所有者 |
| reboot | delay_seconds | 重启设备 |
| shutdown | delay_seconds | 关机 |
| factory_reset | type | 恢复出厂 |
| get_node_info | - | 获取节点信息 |
| remove_node | node_num | 移除节点 |

**config_type 枚举值:**
```
DEVICE_CONFIG = 0
POSITION_CONFIG = 1
POWER_CONFIG = 2
NETWORK_CONFIG = 3
DISPLAY_CONFIG = 4
LORA_CONFIG = 5
BLUETOOTH_CONFIG = 6
SECURITY_CONFIG = 7
```

### 3.5 订阅命令

#### subscribe

订阅服务端事件。

**请求:**
```json
{
    "id": 7,
    "cmd": "subscribe",
    "params": {
        "events": [
            "node_updated",
            "node_expired",
            "packet_received",
            "serial_state_changed"
        ]
    }
}
```

**响应:**
```json
{
    "id": 7,
    "status": "ok",
    "data": {
        "subscribed": ["node_updated", "packet_received"]
    }
}
```

#### unsubscribe

取消订阅。

**请求:**
```json
{
    "id": 8,
    "cmd": "unsubscribe",
    "params": {
        "events": ["packet_received"]
    }
}
```

### 3.6 配置命令

#### get_config

获取服务配置。

**请求:**
```json
{
    "id": 9,
    "cmd": "get_config"
}
```

**响应:**
```json
{
    "id": 9,
    "status": "ok",
    "data": {
        "serial": {
            "device": "/dev/ttyUSB0",
            "baudrate": 115200
        },
        "heartbeat": {
            "interval": 600,
            "timeout": 30
        },
        "node": {
            "expire_time": 7200
        }
    }
}
```

#### set_config

修改服务配置。

**请求:**
```json
{
    "id": 10,
    "cmd": "set_config",
    "params": {
        "serial": {
            "device": "/dev/ttyUSB1"
        }
    }
}
```

---

## 4. 事件推送格式

### 4.1 通用格式

```json
{
    "event": "event_name",
    "timestamp": 1705315845000,
    "data": {
        ...
    }
}
```

### 4.2 事件列表

#### node_updated

节点信息更新。

```json
{
    "event": "node_updated",
    "timestamp": 1705315845000,
    "data": {
        "node_id": "!aabbccdd",
        "changes": ["position", "battery", "snr"],
        "node": {
            "node_id": "!aabbccdd",
            "battery_level": 85,
            "latitude": 31.5,
            "longitude": 121.5,
            "snr": 5.5
        }
    }
}
```

#### node_expired

节点过期。

```json
{
    "event": "node_expired",
    "timestamp": 1705315845000,
    "data": {
        "node_id": "!aabbccdd",
        "reason": "timeout",
        "last_heard": 1705312245
    }
}
```

#### packet_received

收到数据包。

```json
{
    "event": "packet_received",
    "timestamp": 1705315845000,
    "data": {
        "packet_id": 12345678,
        "from_id": "!aabbccdd",
        "to_id": "broadcast",
        "portnum": 1,
        "portnum_name": "TEXT_MESSAGE_APP",
        "channel": 0,
        "hops_traveled": 1,
        "payload_summary": "Hello World"
    }
}
```

#### serial_state_changed

串口状态变化。

```json
{
    "event": "serial_state_changed",
    "timestamp": 1705315845000,
    "data": {
        "state": "connected",
        "device": "/dev/ttyUSB0",
        "baudrate": 115200
    }
}
```

#### telemetry_received

收到遥测数据。

```json
{
    "event": "telemetry_received",
    "timestamp": 1705315845000,
    "data": {
        "node_id": "!aabbccdd",
        "time": 1705315845,
        "device_metrics": {
            "battery_level": 85,
            "voltage": 3.7,
            "channel_utilization": 2.5,
            "air_util_tx": 0.1,
            "uptime_seconds": 86400
        }
    }
}
```

#### position_received

收到位置数据。

```json
{
    "event": "position_received",
    "timestamp": 1705315845000,
    "data": {
        "node_id": "!aabbccdd",
        "latitude": 31.5,
        "longitude": 121.5,
        "altitude": 10,
        "time": 1705315845,
        "location_source": "LOC_INTERNAL"
    }
}
```

---

## 5. 错误码

### 5.1 错误码表

| 范围 | 类别 |
|------|------|
| 1-99 | 通用错误 |
| 100-199 | 串口错误 |
| 200-299 | 协议错误 |
| 300-399 | 节点错误 |
| 400-499 | Socket错误 |
| 500-599 | 配置错误 |

### 5.2 常用错误码

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 1 | UNKNOWN | 未知错误 |
| 2 | INVALID_PARAM | 参数无效 |
| 3 | NOT_CONNECTED | 未连接设备 |
| 4 | TIMEOUT | 操作超时 |
| 100 | SERIAL_OPEN_FAILED | 串口打开失败 |
| 101 | SERIAL_READ_ERROR | 串口读取错误 |
| 200 | PROTO_DECODE_ERROR | 协议解码错误 |
| 202 | UNKNOWN_COMMAND | 未知命令 |
| 203 | INVALID_JSON | JSON格式错误 |
| 300 | NODE_NOT_FOUND | 节点不存在 |
| 301 | NODE_EXPIRED | 节点已过期 |
| 400 | SOCKET_ERROR | Socket错误 |

---

## 6. 客户端实现示例

### 6.1 C 语言客户端

```c
// client.h
#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int fd;
    char recv_buf[65536];
    int recv_len;
} client_t;

/* 连接服务 */
int client_connect(client_t *c, const char *socket_path);

/* 断开连接 */
void client_disconnect(client_t *c);

/* 发送请求 */
int client_send_request(client_t *c, int id, const char *cmd, const char *params);

/* 接收响应 */
int client_recv_response(client_t *c, int timeout_ms, char *buf, size_t len);

/* 同步调用 */
int client_call(client_t *c, const char *cmd, const char *params,
                char *response, size_t len, int timeout_ms);

#endif /* CLIENT_H */
```

```c
// client.c
#include "client.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

int client_connect(client_t *c, const char *socket_path) {
    memset(c, 0, sizeof(client_t));
    c->fd = -1;
    
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    c->fd = fd;
    return 0;
}

void client_disconnect(client_t *c) {
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
}

int client_send_request(client_t *c, int id, const char *cmd, const char *params) {
    char msg[65536];
    int len;
    
    if (params && params[0]) {
        len = snprintf(msg, sizeof(msg), 
            "{\"id\":%d,\"cmd\":\"%s\",\"params\":%s}\n", id, cmd, params);
    } else {
        len = snprintf(msg, sizeof(msg), 
            "{\"id\":%d,\"cmd\":\"%s\"}\n", id, cmd);
    }
    
    if (send(c->fd, msg, len, 0) != len) {
        return -1;
    }
    
    return 0;
}

int client_recv_response(client_t *c, int timeout_ms, char *buf, size_t len) {
    struct pollfd pfd = { .fd = c->fd, .events = POLLIN };
    
    if (poll(&pfd, 1, timeout_ms) <= 0) {
        return -1;  // 超时
    }
    
    ssize_t n = recv(c->fd, buf, len - 1, 0);
    if (n <= 0) {
        return -1;
    }
    
    buf[n] = '\0';
    
    // 移除末尾的换行
    if (n > 0 && buf[n-1] == '\n') {
        buf[n-1] = '\0';
    }
    
    return 0;
}

int client_call(client_t *c, const char *cmd, const char *params,
                char *response, size_t len, int timeout_ms) {
    static int next_id = 1;
    int id = next_id++;
    
    if (client_send_request(c, id, cmd, params) != 0) {
        return -1;
    }
    
    if (client_recv_response(c, timeout_ms, response, len) != 0) {
        return -1;
    }
    
    return 0;
}
```

### 6.2 使用示例

```c
int main() {
    client_t client;
    
    // 连接
    if (client_connect(&client, "/run/meshgateway.sock") != 0) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }
    
    // 获取节点
    char response[65536];
    if (client_call(&client, "get_nodes", NULL, response, sizeof(response), 5000) == 0) {
        printf("Response: %s\n", response);
    }
    
    // 发送消息
    const char *params = "{\"to\":\"broadcast\",\"channel\":0,\"text\":\"Hello\"}";
    if (client_call(&client, "send_text", params, response, sizeof(response), 5000) == 0) {
        printf("Response: %s\n", response);
    }
    
    // 断开
    client_disconnect(&client);
    
    return 0;
}
```

---

## 7. 协议版本

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2024-01-15 | 初始版本 |