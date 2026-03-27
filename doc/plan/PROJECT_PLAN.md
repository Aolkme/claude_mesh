# claude_mesh — Meshtastic 网关服务系统工程规划

> 创建：2026-03-24 | 更新：2026-03-26
> 状态：**阶段二已完成，阶段三开发中（入网流程 + PKI）**

---

## 1. 项目概述

### 1.1 背景

本项目基于 `meshdebug` Python 调试工具的研究成果，构建一套**可在生产环境运行的 Meshtastic 网关服务系统**。目标是将协议研究成果提炼为 C 语言服务，实现常驻后台运行和多端控制。

### 1.2 设计原则（`doc/设计需求补充.md` 为最高优先级）

| # | 原则 | 说明 |
|---|------|------|
| 1 | **延迟连接** | 服务启动时不自动连接串口，需通过 CLI/UI 命令触发连接 |
| 2 | **复用已有 protobuf-c** | 直接使用 `protobufs_protobuf-c/` 下的编译文件，不重复实现协议逻辑 |
| 3 | **内核完整** | 解析与发送对所有字段完整实现；CLI/UI 可以精简，内核不能精简 |
| 4 | **双表存储** | 维护两张节点表：网关节点表（自身）和远程节点表（其他节点），含私有配置字段 |
| 5 | **数据分类** | 串口数据分两类：proto 数据（解析存储）、debug 日志（单独处理）|
| 6 | **私有协议复用** | 入网通过 thingseye `PrivateConfigPacket`（PortNum 287），直接调用 `temeshtastic__*` 函数 |

### 1.3 系统目标

| 目标 | 说明 |
|------|------|
| 服务常驻 | 后台守护进程，独立于客户端生命周期 |
| 跨平台 | Linux（树莓派/ARM）、Windows、macOS |
| 多端控制 | CLI 和 UI 可同时连接同一服务 |
| 资源友好 | 内存 < 10MB，CPU 空闲 < 5% |
| 稳定可靠 | 串口断线自动重连，心跳保活，异常恢复 |

---

## 2. 系统架构

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                        Host System                          │
│                                                             │
│  ┌────────────────────────────────────────────────────┐    │
│  │               meshgateway (C daemon)                │    │
│  │                                                     │    │
│  │  ┌──────────────┐    ┌────────────────────────┐    │    │
│  │  │Serial Manager│───▶│      Core Engine        │    │    │
│  │  │  (延迟连接)  │    │  ┌──────────┬────────┐  │    │    │
│  │  └──────────────┘    │  │ Gateway  │ Remote │  │    │    │
│  │        ▲             │  │ Node Tbl │NodeTbl │  │    │    │
│  │        │             │  └──────────┴────────┘  │    │    │
│  │  ┌─────┴──────┐      └───────────┬─────────────┘    │    │
│  │  │Serial Device│                 │ TCP 127.0.0.1:9999│    │
│  │  └────────────┘     └────────────┘                   │    │
│  └─────────────────────────────────────────────────────┘    │
│                              │                               │
│              ┌───────────────┘                               │
│              ▼                         ▼                     │
│  ┌─────────────────┐     ┌────────────────────────┐         │
│  │  meshgate-cli   │     │  meshgate-ui (Qt)       │         │
│  │  (C命令行工具)  │     │  (任意 TCP socket 客户端)│        │
│  └─────────────────┘     └────────────────────────┘         │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 组件职责

| 组件 | 语言 | 职责 | 生命周期 |
|------|------|------|----------|
| `libmeshcore` | C11 | 核心协议库：帧解析/构造、protobuf 解析/构造、节点管理、心跳 | 静态库 |
| `meshgateway` | C11 | 守护进程：串口管理（延迟连接）、事件循环、TCP IPC、HTTP+WS | 系统启动 |
| `meshgate-cli` | C11 | 命令行工具：连接 IPC 服务，发送命令，格式化输出 | 用户调用 |
| `meshgateway-ui` | Vue 3 | Web 前端：Dashboard / 节点 / 消息 / 监控，Vite 构建 | 浏览器 |

### 2.3 Protobuf 文件使用策略

**全部直接使用，不重新实现任何协议逻辑：**

```
protobufs_protobuf-c/
├── meshtastic/               ← 官方 Meshtastic 消息（42 个文件）
│   ├── mesh.pb-c.{h,c}          FromRadio, ToRadio, MeshPacket
│   ├── admin.pb-c.{h,c}         AdminMessage（GET/SET/REBOOT 等）
│   ├── config.pb-c.{h,c}        Config (LoRa/Device/Security/...)
│   ├── telemetry.pb-c.{h,c}     Telemetry, DeviceMetrics
│   ├── channel.pb-c.{h,c}       Channel
│   ├── position.pb-c.{h,c}      Position
│   ├── user.pb-c.{h,c}          User, NodeInfo
│   ├── routing.pb-c.{h,c}       Routing
│   └── portnums.pb-c.{h,c}      PortNum 枚举
└── thingseye/                ← 私有配置（已编译，直接使用）
    └── privateconfig.pb-c.{h,c}
        ├── Temeshtastic__PrivateConfigPacket     (PortNum 287)
        │   ├── Request: SetCompanyKeyRequest     入网邀请
        │   │            ChangeAdminRequest       更换管理员（含 Ed25519 签名）
        │   │            SetAdminKey / SetDeviceName / SetInfoLabel
        │   │            SetSyncWakeupConfig
        │   │            GetConfig / GetInfoLabels / GetSyncWakeup
        │   └── Response: PrivateConfig / DeviceLabels
        │                 SyncWakeupConfig / CompanyConfig
        ├── Temeshtastic__CompanyConfig
        │   ├── company_public_key (32 字节)
        │   ├── is_enrolled
        │   └── last_change_timestamp
        ├── Temeshtastic__PrivateConfig
        │   ├── privateversion, devicename, isadminkeyset
        │   └── sync_wakeup, company_config
        └── Temeshtastic__SyncWakeupConfig (Fixed / Scheduled 策略)
```

### 2.4 数据流

**接收流（串口 → 内核 → 客户端）：**

```
Serial Device (串口字节流)
  │
  ▼ frame_parser_feed()          帧同步状态机 [0x94][0xC3][len_H][len_L][payload]
  ├─ FRAME_RESULT_TEXT_BYTE  ──▶ debug_log_buffer  (设备调试日志，单独归类)
  └─ FRAME_RESULT_COMPLETE   ──▶ proto_parse_from_radio()
                                   ├─ MY_INFO         ──▶ 更新网关节点表
                                   ├─ NODE_INFO        ──▶ 更新远程节点表
                                   ├─ CONFIG           ──▶ 存储网关设备配置
                                   ├─ CHANNEL          ──▶ 存储网关信道配置
                                   ├─ CONFIG_COMPLETE  ──▶ 握手完成标志
                                   └─ PACKET           ──▶ 按 PortNum 分发
                                        ├─ 1  TEXT_MESSAGE   解析文本
                                        ├─ 3  POSITION       解析坐标（meshtastic__position__unpack）
                                        ├─ 4  NODEINFO       解析用户（meshtastic__user__unpack）
                                        ├─ 5  ROUTING        解析路由状态
                                        ├─ 6  ADMIN          解析 AdminMessage 响应
                                        ├─ 67 TELEMETRY      解析遥测（meshtastic__telemetry__unpack）
                                        └─ 287 PRIVATE_CONFIG 解析私有配置响应
                                                           (temeshtastic__private_config_packet__unpack)
                                   ──▶ node_manager_update()  更新节点表
                                   ──▶ tcp_server_broadcast() 推送事件给订阅客户端
```

**发送流（客户端 → 内核 → 串口）：**

```
CLI / UI → TCP JSON 请求
  │
  ▼ command_handler_dispatch()
  │
  ├─ 文本/位置     ──▶ proto_builder_*(...)
  ├─ Admin 操作    ──▶ admin_builder_*(AdminMessage + session_passkey)
  │                    → meshtastic__admin_message__pack()
  └─ 私有配置 287  ──▶ private_config_handler_build_*(...)
                       → temeshtastic__private_config_packet__pack()
  │
  ▼ frame_builder_encode()      添加 [0x94][0xC3][len_H][len_L]
  │
  ▼ serial_port_write()
```

---

## 3. 目录结构（目标态）

```
claude_mesh/
├── CMakeLists.txt
├── meshgateway.conf.example
│
├── protobufs_protobuf-c/              ← 只读，直接使用
│   ├── meshtastic/                    (官方 42 个 pb-c 文件)
│   └── thingseye/
│       └── privateconfig.pb-c.{h,c}
│
├── protobuf-c-1.5.2/                  ← protobuf-c 运行时（已有）
│
├── libmeshcore/                       ← 核心协议库（静态库）
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── mesh_types.h               ✅ 基础类型（已扩展：双表 + 私有配置字段）
│   │   ├── frame_parser.h             ✅ 帧解析
│   │   ├── frame_builder.h            ✅ 帧构造（已建）
│   │   ├── serial_port.h              ✅ 跨平台串口
│   │   ├── proto_parser.h             ✅ FromRadio 解析（PortNum 分发已完整）
│   │   ├── proto_builder.h            ❌ ToRadio/Payload 构造（新建，完整字段）
│   │   ├── admin_builder.h            ❌ AdminMessage 构造（新建）
│   │   ├── private_config_handler.h   ❌ PortNum 287 收发（新建，基于 privateconfig.pb-c.h）
│   │   ├── pki_crypto.h               ❌ X25519 + Ed25519（新建）
│   │   ├── node_manager.h             ✅ 节点表（双表设计 + 私有配置字段 + 字段级更新接口）
│   │   └── heartbeat.h                ✅ 心跳（超时重试状态机已实现）
│   └── src/
│       ├── frame_parser.c             ✅
│       ├── frame_builder.c            ✅ 已建
│       ├── serial_port.c              ✅
│       ├── proto_parser.c             ✅ 全 PortNum 分发完整
│       ├── proto_builder.c            ❌
│       ├── admin_builder.c            ❌
│       ├── private_config_handler.c   ❌
│       ├── pki_crypto.c               ❌
│       ├── node_manager.c             ✅ 双表 + 字段级更新
│       └── heartbeat.c                ✅ 状态机完整
│
├── meshgateway/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.c                     ✅ 延迟连接模式已实现
│       ├── config.c / config.h        ✅ auto_connect / heartbeat_timeout 已补
│       ├── log.c / log.h              ✅
│       ├── event_loop.c / h           ✅ 完整消息分发 + debug_log 分离
│       ├── tcp_server.c / h           ⚠️ 需实现完整 broadcast_event（monitor 订阅集合）
│       └── command_handler.c / h      ⚠️ 需扩展命令集，引入 cJSON
│
└── meshgate-cli/
    ├── CMakeLists.txt
    └── src/
        ├── main.c                     ⚠️ 需扩展命令
        ├── tcp_client.c / h           ✅
        └── output_format.c / h        ✅
```

---

## 4. 节点数据表设计

### 4.1 网关节点表（自身，单条）

握手期由 `MY_INFO` + `CONFIG` + `CHANNEL` 事件填充：

```c
typedef struct {
    // 基本信息（MY_INFO + NODE_INFO）
    uint32_t node_num;
    char     node_id[MESH_NODE_ID_LEN];
    char     long_name[MESH_LONG_NAME_LEN];
    char     short_name[MESH_SHORT_NAME_LEN];
    uint8_t  hw_model;
    uint8_t  role;

    // PKI 密钥
    uint8_t  public_key[32];
    uint8_t  private_key[32];      // 本地持久化，不上报
    bool     has_keypair;

    // 连接状态
    bool     serial_connected;
    bool     config_complete;      // 握手是否完成
    uint64_t connect_time_ms;
    char     serial_device[64];
} mesh_gateway_node_t;
```

### 4.2 远程节点表（含私有配置字段）

```c
typedef struct {
    // Meshtastic 标准字段
    uint32_t node_num;
    char     node_id[MESH_NODE_ID_LEN];
    char     long_name[MESH_LONG_NAME_LEN];
    char     short_name[MESH_SHORT_NAME_LEN];
    uint8_t  hw_model;
    uint8_t  role;
    uint8_t  public_key[32];
    bool     has_public_key;

    // 位置（POSITION_APP 或 NODE_INFO）
    double   latitude;
    double   longitude;
    int32_t  altitude;
    uint32_t position_time;

    // 遥测（TELEMETRY_APP）
    uint32_t battery_level;
    float    voltage;
    uint32_t uptime_seconds;
    float    channel_utilization;
    float    air_util_tx;

    // 信号（MeshPacket 元数据）
    float    snr;
    int32_t  rssi;
    uint8_t  hops_away;

    // 时间戳
    uint64_t last_heard_ms;
    uint64_t first_seen_ms;

    // thingseye 私有配置字段
    bool     is_enrolled;
    uint8_t  company_public_key[32];
    uint64_t last_admin_change_ts;
    bool     is_admin_key_set;
    uint32_t private_config_version;
    char     device_name[64];
    // SyncWakeupConfig 按需扩展

    bool     is_valid;
} mesh_node_t;
```

### 4.3 数据存储格式

| 层次 | 格式 | 说明 |
|------|------|------|
| 运行时 | C 结构体内存数组 | O(1) 访问，快速更新 |
| 持久化 | JSON 文件（nodes.json）| 使用 cJSON 序列化，启动恢复上次状态 |
| IPC 输出 | JSON（via cJSON）| 推送给 CLI/UI 客户端 |

---

## 5. IPC 通信协议

### 5.1 消息帧格式

```
[uint32_t Length (大端)][uint8_t Type][JSON Payload (UTF-8)]

Type: 0x01=请求  0x02=响应  0x03=事件推送  0x04=错误
```

### 5.2 完整命令集

**连接管理：**

| 命令 | 参数 | 说明 |
|------|------|------|
| `connect_serial` | `device`, `baudrate` | 触发串口连接（实现延迟连接设计） |
| `disconnect_serial` | 无 | 断开串口 |
| `get_status` | 无 | 服务状态：连接状态、节点数、运行时长 |

**节点信息：**

| 命令 | 参数 | 说明 |
|------|------|------|
| `get_gateway_info` | 无 | 网关自身节点信息 |
| `get_nodes` | `sort`, `filter` | 远程节点列表 |
| `get_node` | `node_id` | 单节点详情（含私有配置字段）|

**消息发送（内核完整字段）：**

| 命令 | 参数 | 说明 |
|------|------|------|
| `send_text` | `to`, `text`, `channel`, `want_ack`, `hop_limit`, `priority` | 发送文本 |
| `send_position` | `to`, `lat`, `lon`, `alt`, `channel` | 发送位置 |

**Admin 操作（基于 AdminMessage，需 Session Passkey）：**

| 命令 | 参数 | 说明 |
|------|------|------|
| `admin_get_config` | `node_id`, `config_type` | 读远程设备配置 |
| `admin_set_config` | `node_id`, `config_type`, `config` | 写远程设备配置 |
| `admin_get_channel` | `node_id`, `index` | 读远程信道 |
| `admin_set_channel` | `node_id`, `index`, `channel` | 写远程信道（部署私有/工作信道）|
| `admin_reboot` | `node_id` | 重启远程节点 |
| `admin_factory_reset` | `node_id` | 恢复出厂设置 |
| `admin_get_session_passkey` | `node_id` | 获取 Session Passkey |

**入网与私有配置（thingseye PortNum 287）：**

| 命令 | 参数 | 说明 |
|------|------|------|
| `enroll_device` | `node_id` | 发送 `SetCompanyKeyRequest` 入网邀请 |
| `enroll_all` | 无 | 对所有未入网节点批量发送入网邀请 |
| `change_admin` | `node_id` | 更换管理员（`ChangeAdminRequest` + Ed25519 签名）|
| `private_set_device_name` | `node_id`, `name` | 设置远程设备名称 |
| `private_set_info_label` | `node_id`, `action`, `label` | 设置信息标签 |
| `private_set_wakeup` | `node_id`, `config` | 设置唤醒配置 |
| `private_get_config` | `node_id` | 读取私有配置 |

**监控：**

| 命令 | 参数 | 说明 |
|------|------|------|
| `monitor_start` | 无 | 订阅实时事件推送 |
| `monitor_stop` | 无 | 取消订阅 |

### 5.3 事件推送类型

| 事件 | 触发条件 |
|------|---------|
| `serial_connected` | 串口连接成功 |
| `serial_disconnected` | 串口断开 |
| `config_complete` | 握手完成（收到 config_complete_id）|
| `node_updated` | 节点信息变更 |
| `node_new` | 首次发现新节点 |
| `packet_received` | 收到任意 MeshPacket |
| `text_message` | 收到文本消息 |
| `device_enrolled` | 入网操作响应（成功/失败）|
| `debug_log` | 串口 debug 日志（非 proto 数据）|

---

## 6. 配置文件

```ini
[serial]
device =                        ; 留空 = 不自动连接（延迟连接）
baudrate = 115200
reconnect_interval = 5          ; 断线重连间隔（秒）
auto_connect = false            ; 启动时是否自动连接（默认 false）

[network]
host = 127.0.0.1
port = 9999
max_clients = 16

[heartbeat]
interval = 600                  ; 秒（10分钟）
timeout = 30                    ; 秒

[log]
level = info
file = meshgateway.log
max_size = 10                   ; MB
max_files = 5

[node]
expire_time = 7200              ; 远程节点过期时间（秒）
persist_file = nodes.json       ; 节点表持久化文件

[gateway]
key_file = gateway.key          ; 网关密钥对持久化文件（首次启动自动生成）
company_public_key =            ; 公司公钥（hex，32字节）
```

---

## 7. 技术选型

| 模块 | 技术方案 | 原因 |
|------|----------|------|
| Protobuf | protobuf-c（已有 pb-c 文件直接用）| 直接调用 unpack/pack，不重新实现 |
| JSON | cJSON（单文件嵌入）✅ 已集成 | 替代手写 snprintf，MIT 许可 |
| Web 服务器 | Mongoose（单文件嵌入）✅ 已集成 | HTTP + WebSocket，MIT 许可 |
| Web 前端 | Vue 3 + Vite ✅ 已搭建 | 构建产物直接由 Mongoose 服务 |
| PKI 加密 | libsodium 或 mbedtls（待阶段三）| X25519 密钥交换 + Ed25519 签名 |
| 串口 | POSIX termios / Win32 API | 原生，零依赖 |
| IPC | TCP localhost :9999 | 跨平台最高兼容性 |
| 事件循环 | `select()` | 可移植所有平台 |
| 构建 | CMake 3.15+ | 跨平台标准 |

---

## 8. 分阶段开发计划

### 阶段一：协议内核完整化（✅ 已完成）

**目标：** libmeshcore 能完整处理所有 FromRadio/ToRadio 消息类型，修复延迟连接问题

#### 1.1 修正：延迟连接（`main.c`）
- [x] 启动时不打开串口，仅初始化各模块
- [x] 串口连接由 `connect_serial` 命令触发
- [x] 配置 `auto_connect = true` 时允许自动连接（向后兼容）

#### 1.2 新建：`frame_builder.c/h`
- [x] `frame_builder_encode(payload, len, buf_out, buf_len)` → 添加 `[0x94][0xC3][len_H][len_L]`
- [x] 替代 `command_handler.c` 中所有手写帧头代码

#### 1.3 扩展：`proto_parser.c` — 补全 PACKET PortNum 分发
基于已有的 pb-c 文件，在 `parse_payload_by_portnum()` 中补充：
- [x] `PORTNUM_POSITION(3)` → `meshtastic__position__unpack()` → 写入 mesh_packet_t 位置字段
- [x] `PORTNUM_NODEINFO(4)` → `meshtastic__user__unpack()` → 写入用户信息字段
- [x] `PORTNUM_ROUTING(5)` → `meshtastic__routing__unpack()` → 写入路由状态
- [x] `PORTNUM_TELEMETRY(67)` → `meshtastic__telemetry__unpack()` → 写入遥测字段
- [x] `PORTNUM_ADMIN(6)` → 保存原始 admin payload 供上层解析
- [x] `PORTNUM_PRIVATE_CONFIG(287)` → 保存原始 private payload 供上层解析

#### 1.4 扩展：`mesh_types.h`
- [x] 扩展 `mesh_node_t`：增加 thingseye 私有配置字段
- [x] 新增 `mesh_gateway_node_t`：网关自身信息（含 serial_connected 状态）
- [x] 扩展 `mesh_packet_t`：position/user/telemetry/routing 解析子结构体

#### 1.5 扩展：`node_manager.c/h` — 双表设计
- [x] 拆分为网关节点（单条）和远程节点表（最多 256 条）
- [x] 增加字段级更新接口：`node_manager_set_enrolled()`、`node_manager_set_private_config()` 等
- [x] `node_manager_update_from_packet()` 按 PortNum 更新节点字段

#### 1.6 扩展：`heartbeat.c` — 超时重试状态机
- [x] 实现 5 状态机：`IDLE → SENT → WAITING → RETRYING → FAILED`
- [x] 3 次重试，超时 30 秒，失败后回调通知重连
- [x] 新 API：`heartbeat_init(interval_ms, timeout_ms, retries, send_fn, fail_fn, ud)`

**阶段一验证（已通过编译）：**
```bash
cd build && cmake --build .  # 零错误编译通过
```

---

### 阶段二：完整发送能力 + CLI + Web UI（✅ 已完成）

**目标：** 能发送所有类型消息，monitor 实时推送正常，Web UI 可访问

#### 2.1 引入 cJSON ✅
- [x] 单文件嵌入 `third_party/cJSON/cJSON.c/.h`
- [x] 根 CMakeLists.txt 添加 `cjson` 静态库目标
- [x] `command_handler.c` 全面重写（cJSON 解析/构造，替换手写 snprintf/strstr）

#### 2.2 新建：`proto_builder.c/h` ✅
- [x] `proto_builder_text()` — 文本消息构造（完整字段）
- [x] `proto_builder_position()` — 位置消息构造
- [x] 修复 `send_raw` 空指针 bug（改为直接调用 `serial_port_write`）

#### 2.3 新建：`admin_builder.c/h` ✅
- [x] `admin_build_get_session_passkey(dest, from)`
- [x] `admin_build_get_config(dest, from, config_type, passkey)`
- [x] `admin_build_get_channel(dest, from, channel_index, passkey)`
- [x] `admin_build_reboot(dest, from, delay_s, passkey)`
- [x] `admin_build_factory_reset_config(dest, from, passkey)`

#### 2.4 扩展：`command_handler.c` — 完整命令集 ✅
- [x] `connect_serial` / `disconnect_serial`（延迟连接核心）
- [x] `get_status` / `get_gateway_info` / `get_nodes` / `get_node`
- [x] `send_text` / `send_position`
- [x] `admin_get_session_passkey` / `admin_get_config` / `admin_get_channel` / `admin_reboot`
- [x] `monitor_start` / `monitor_stop`

#### 2.5 完善：`meshgate-cli` ✅
- [x] `connect --device <dev> [--baudrate N]`
- [x] `disconnect`、`gateway-info`、`node --id`
- [x] `send --to <id> [--channel N] [--ack] <text>`
- [x] `send-pos --to <id> --lat N --lon N [--alt N]`
- [x] `admin passkey / get-config / get-channel / reboot`

#### 2.6 集成 Mongoose — HTTP+WebSocket 服务器 ✅
- [x] `third_party/mongoose/mongoose.c/.h`（单文件嵌入）
- [x] `meshgateway/src/web_server.c/h`
  - GET /、POST /api/cmd、GET /api/status、GET /api/nodes
  - WS /ws（自动订阅所有广播事件）
  - 静态文件从 `web_static_dir` 目录服务（默认 `./static`）
  - 内嵌 fallback HTML（`static/` 为空时生效）
- [x] `event_loop.c` 添加 `broadcast_event()` helper（同步推送 TCP + WebSocket 客户端）
- [x] `config.h/c` 新增 `web_bind` + `web_static_dir` 配置字段
- [x] `main.c` 集成 `web_server_init/poll/close`

#### 2.7 Vue 3 前端 ✅
- [x] `meshgateway-ui/`（Vue 3 + Vite，构建产物 → `meshgateway/static/`）
- [x] `useGateway.js` composable（WebSocket 全局单例，自动重连，响应式状态）
- [x] `Dashboard.vue`（状态统计 + 串口连接控制）
- [x] `Nodes.vue`（完整节点表格，12 列）
- [x] `Messages.vue`（文本/位置发送 + Admin 操作面板）
- [x] `Monitor.vue`（实时事件日志，自动滚动）
- [x] `vite.config.js`（开发代理 → localhost:8080，生产构建 → static/）
- [x] `meshgateway/static/index.html`（生产 HTML 入口，含 Vue bundle）

**阶段二验证（已通过编译）：**
```bash
# C 后端构建
cd build && cmake --build .    # 零错误

# 启动守护进程（Web UI 自动可用）
./meshgateway/meshgateway
# 浏览器打开 http://0.0.0.0:8080

# CLI 验证
./meshgate-cli/meshgate-cli connect --device /dev/ttyUSB0
./meshgate-cli/meshgate-cli status
./meshgate-cli/meshgate-cli nodes
./meshgate-cli/meshgate-cli send --to !aabbccdd "hello"
./meshgate-cli/meshgate-cli admin passkey --node !aabbccdd

# Vue 开发模式
cd meshgateway-ui && npm run dev    # localhost:5173，代理到 :8080
cd meshgateway-ui && npm run build  # 构建到 meshgateway/static/
```

---

### 阶段三：入网流程与 PKI（当前阶段）

**目标：** 实现 thingseye 私有协议完整入网流程和管理员管理

#### 3.1 引入 PKI 库（libsodium 或 mbedtls）
- [ ] 集成到构建系统
- [ ] 选型依据：libsodium API 简洁；mbedtls 更适合嵌入式无 OS 场景

#### 3.2 新建：`pki_crypto.c/h`
- [ ] `pki_generate_keypair(pub_out, priv_out)` — 生成 X25519 密钥对
- [ ] `pki_load_or_generate(key_file, pub, priv)` — 首次运行自动生成并持久化
- [ ] `pki_sign_ed25519(msg, msg_len, priv_key, sig_out)` — Ed25519 签名（用于 ChangeAdmin）
- [ ] `pki_verify_ed25519(msg, msg_len, pub_key, sig)` — 签名验证

#### 3.3 新建：`private_config_handler.c/h`
**直接调用 `temeshtastic__*` 函数，不重新实现协议：**

- [ ] `private_config_build_enroll(gateway_node_id, company_pub_key, gateway_pub_key)`
  ```c
  Temeshtastic__PrivateConfigPacket__SetCompanyKeyRequest req = {...};
  // 填充 company_public_key, gateway_public_key, gateway_node_id, timestamp
  // → temeshtastic__private_config_packet__pack() 序列化
  ```
- [ ] `private_config_build_change_admin(new_gateway_node_id, new_pub_key, priv_key)`
  ```c
  // 构造 ChangeAdminRequest，用 pki_sign_ed25519() 签名
  // → temeshtastic__private_config_packet__pack()
  ```
- [ ] `private_config_parse_response(payload, len, node_out)`
  ```c
  // temeshtastic__private_config_packet__unpack()
  // → 更新 mesh_node_t 的 is_enrolled, company_public_key 等字段
  ```
- [ ] `private_config_build_set_device_name()`
- [ ] `private_config_build_set_info_label()`
- [ ] `private_config_build_set_wakeup()`
- [ ] `private_config_build_get_*()`

#### 3.4 实现：Session Passkey 管理（嵌入 admin_builder）
- [ ] `session_passkey_request(dest_node_id)` — 发送 get_session_passkey
- [ ] `session_passkey_store(dest, passkey, expire_ms)` — 缓存（~270秒有效）
- [ ] `session_passkey_get(dest)` — 获取有效 passkey，过期自动请求更新
- [ ] admin_builder 所有发送函数自动调用 `session_passkey_get()`

**阶段三验证：**
```bash
./meshgate-cli enroll-device --node !aabbccdd     # 入网
./meshgate-cli get-node !aabbccdd                 # is_enrolled=true
./meshgate-cli admin set-channel --node !aabbccdd --index 0 --channel '{...}'
./meshgate-cli change-admin --node !aabbccdd      # 更换管理员
```

---

### 阶段四：生产就绪

#### 4.1 错误恢复
- [ ] 串口断线：指数退避重连（1→2→4→...→max 60s）
- [ ] 帧解析错误：状态机自动恢复，统计丢帧计数
- [ ] proto 解析失败：记录 hex dump，跳过该帧继续

#### 4.2 节点表持久化
- [ ] 正常关闭时 / 定期写入 `nodes.json`（cJSON 序列化）
- [ ] 启动时加载，上次节点立即可用

#### 4.3 systemd 集成
- [ ] 提供 `meshgateway.service` 单元文件
- [ ] `SIGHUP` 热重载配置（无需重启）
- [ ] Graceful shutdown（SIGTERM → 等待当前帧处理完成）

#### 4.4 CLI 命令完善
- [ ] 扩展 `meshgate-cli` 支持阶段二/三全部新命令
- [ ] `--json` / `--csv` 输出统一使用 cJSON

---

## 9. 当前实现状态

| 模块 | 完成度 | 状态 |
|------|--------|------|
| `frame_parser` | 85% | ✅ 基本完整 |
| `frame_builder` | 100% | ✅ 统一帧头编码，已在全流程使用 |
| `serial_port` | 80% | ✅ 跨平台实现完整 |
| `proto_parser` | 85% | ✅ 全 PortNum 分发（TEXT/POS/NODEINFO/ROUTING/TELEMETRY/ADMIN/287）|
| `proto_builder` | 80% | ✅ 文本包 + 位置包构造；nodeinfo 包待补 |
| `admin_builder` | 90% | ✅ passkey/config/channel/reboot/factory_reset；set_config/set_channel 待补 |
| `node_manager` | 75% | ✅ 双表 + 私有字段 + 字段级更新；持久化（nodes.json）待阶段四 |
| `heartbeat` | 90% | ✅ 超时重试状态机完整 |
| `mesh_types.h` | 90% | ✅ 双表类型 + packet 解析子结构体 + 私有配置字段 |
| `main.c` | 85% | ✅ 延迟连接 + web_server 集成 |
| `config.c/h` | 90% | ✅ 全字段已有（serial/network/heartbeat/log/node/web）|
| `event_loop.c` | 85% | ✅ 全消息分发 + TCP+WS 双路广播 |
| `tcp_server.c` | 80% | ✅ 连接管理 + broadcast_event 正常工作 |
| `command_handler.c` | 85% | ✅ 14 命令全部实现，cJSON 重写完毕 |
| `web_server.c/h` | 90% | ✅ HTTP + WebSocket + 静态文件服务 |
| `meshgateway-ui` | 70% | ✅ 4 视图可用（Dashboard/Nodes/Messages/Monitor）；地图视图待后续 |
| `meshgate-cli` | 85% | ✅ 全命令集实现（connect/send/send-pos/admin） |
| `private_config_handler` | 0% | ❌ 未建（pb-c 文件已就位），待阶段三 |
| `pki_crypto` | 0% | ❌ 未建，待阶段三 |

**阶段一完成度：100% | 阶段二完成度：100% | 总体完成度：约 68%**

---

## 10. 参考资料

| 资料 | 路径 | 用途 |
|------|------|------|
| **设计需求补充** | `doc/设计需求补充.md` | **最高优先级：框架级设计决策** |
| thingseye 私有协议 | `protobufs_protobuf-c/thingseye/privateconfig.pb-c.h` | 入网/管理员/私有配置结构体和函数 |
| Meshtastic 消息 | `protobufs_protobuf-c/meshtastic/mesh.pb-c.h` | FromRadio/ToRadio/MeshPacket |
| Admin 消息 | `protobufs_protobuf-c/meshtastic/admin.pb-c.h` | AdminMessage 完整字段 |
| 帧协议规范 | `doc/spec/01_帧协议规范.md` | 帧同步状态机、握手流程 |
| 解析器设计 | `doc/spec/02_解析器内核设计.md` | FromRadio 解析、字段映射 |
| 构造器设计 | `doc/spec/03_构造器内核设计.md` | ToRadio 构造、Admin 操作 |
| 扩展协议 | `doc/spec/04_扩展协议规范.md` | PortNum 287 协议细节 |
| 心跳管理 | `doc/spec/05_心跳与连接管理.md` | 心跳状态机、重连策略 |
| 网关架构 | `doc/spec/gateway/01~06_*.md` | 守护进程、IPC、CLI、部署设计 |
| Python 参考实现 | `meshdebug/` | 行为参考（协议已验证）|
