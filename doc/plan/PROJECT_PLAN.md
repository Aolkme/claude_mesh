# claude_mesh — Meshtastic 网关服务系统

> 项目路径：`E:\GiteeRepositories\TEMS\meshtastic-docs\Project\claude_mesh\`
> 创建日期：2026-03-24
> 状态：开发中

---

## 1. 项目概述

### 1.1 背景

本项目基于 `meshdebug` Python调试工具的研究成果，构建一套**可在生产环境运行**的 Meshtastic 网关服务系统。

`meshdebug` 是一个 PyQt6 图形调试工具（~5000行代码），验证了：
- Meshtastic 串口帧协议（0x94 0xC3 魔数）
- 完整的 FromRadio/MeshPacket protobuf 解析
- Admin 操作和 Session Passkey 管理
- PKI 加密（X25519 + AES-256-CCM + Ed25519）

本项目将这些功能提炼为 **C语言服务系统**，实现常驻后台运行和多客户端控制。

### 1.2 系统目标

| 目标 | 说明 |
|------|------|
| 服务常驻 | 后台守护进程持续运行，独立于客户端生命周期 |
| 跨平台 | Linux（树莓派/ARM）、Windows、macOS 均可运行 |
| 多端控制 | CLI工具和其他客户端可同时连接控制同一服务 |
| 资源友好 | 内存 < 10MB，CPU空闲 < 5%，适合嵌入式平台 |
| 稳定可靠 | 串口断线自动重连，心跳保活，异常恢复 |

---

## 2. 系统架构

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           Host System                                    │
│                                                                          │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    meshgateway  (C daemon)                        │   │
│  │                                                                   │   │
│  │  ┌─────────────┐    ┌─────────────┐    ┌───────────────────────┐ │   │
│  │  │  Serial     │    │   Core      │    │   TCP Server          │ │   │
│  │  │  Manager    │──▶ │   Engine    │──▶ │   127.0.0.1:9999      │ │   │
│  │  └─────────────┘    └─────────────┘    └──────────┬────────────┘ │   │
│  │       ▲                    │                      │              │   │
│  │       │                    ▼                      │              │   │
│  │  ┌────┴────────┐    ┌─────────────┐               │              │   │
│  │  │  Serial     │    │    Node     │               │              │   │
│  │  │  Device     │    │   Manager   │               │              │   │
│  │  └─────────────┘    └─────────────┘               │              │   │
│  └──────────────────────────────────────────────────┼──────────────┘   │
│                                                      │                   │
│                         TCP localhost:9999           │                   │
│                                                      ▼                   │
│  ┌─────────────────────────┐      ┌────────────────────────────────┐    │
│  │     meshgate-cli  (C)   │      │   meshgate-ui / 自定义客户端   │    │
│  │  status / nodes /       │      │  (任何支持TCP socket的程序)    │    │
│  │  monitor / send         │      └────────────────────────────────┘    │
│  └─────────────────────────┘                                             │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 组件职责

| 组件 | 语言 | 职责 | 生命周期 |
|------|------|------|----------|
| `meshgateway` | C11 | 后台守护进程，串口通信，协议解析，节点管理 | 系统启动运行 |
| `meshgate-cli` | C11 | 轻量命令行工具，通过TCP连接守护进程 | 用户调用时运行 |
| `libmeshcore` | C11 | 核心协议库（帧解析、protobuf解析、节点管理）| 静态库 |

### 2.3 数据流

**接收流（设备 → 服务）：**
```
Serial Device
  → Serial Thread: 读取字节，帧同步状态机 (0x94 0xC3)
  → Frame Parser: 提取 FromRadio payload
  → Proto Parser: 解码 protobuf → mesh_event_t
  → Core Engine: 更新节点表，写日志
  → TCP Broadcast: 推送事件到所有监控客户端
```

**发送流（CLI → 设备）：**
```
meshgate-cli send --to !xxx "hello"
  → TCP Request: {"cmd":"send_text","params":{"to":"!xxx","text":"hello"}}
  → Command Handler: 构造 MeshPacket
  → Packet Builder: ToRadio protobuf + 帧头 (0x94 0xC3)
  → Serial Thread: 写入串口
```

---

## 3. 目录结构

```
claude_mesh/
├── CMakeLists.txt                    # 顶层构建配置
├── README.md                         # 项目说明
├── meshgateway.conf.example          # 配置文件示例
│
├── doc/
│   └── plan/
│       └── PROJECT_PLAN.md           # 本文件
│
├── protobufs_protobuf-c/             # 已生成的 protobuf-c C文件
│   ├── nanopb.pb-c.h / .c           # protobuf-c 运行时
│   └── meshtastic/
│       ├── mesh.pb-c.h / .c         # MeshPacket, FromRadio, ToRadio
│       ├── admin.pb-c.h / .c        # AdminMessage
│       ├── config.pb-c.h / .c       # Config (LoRa/Device/Security等)
│       ├── telemetry.pb-c.h / .c    # Telemetry
│       ├── portnums.pb-c.h / .c     # PortNum 枚举
│       └── ... (42个消息定义)
│
├── libmeshcore/                      # 核心协议库（静态库）
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── mesh_types.h             # 公共数据类型定义
│   │   ├── frame_parser.h           # 帧解析器接口
│   │   ├── serial_port.h            # 跨平台串口接口
│   │   ├── proto_parser.h           # protobuf解析接口
│   │   ├── node_manager.h           # 节点管理接口
│   │   └── heartbeat.h              # 心跳管理接口
│   └── src/
│       ├── frame_parser.c           # 0x94 0xC3 帧同步状态机
│       ├── serial_port.c            # Win32/POSIX串口实现
│       ├── proto_parser.c           # FromRadio → mesh_event_t
│       ├── node_manager.c           # 节点表（256节点上限）
│       └── heartbeat.c              # want_config_id 心跳
│
├── meshgateway/                      # 守护进程
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.c                   # 入口、信号处理、主循环
│       ├── config.c / config.h      # INI配置文件解析
│       ├── log.c / log.h            # 日志模块（DEBUG/INFO/WARN/ERROR）
│       ├── event_loop.c/h           # select()跨平台事件循环
│       ├── tcp_server.c/h           # TCP localhost:9999 服务器
│       └── command_handler.c/h      # JSON命令路由和处理
│
└── meshgate-cli/                     # CLI 命令行工具
    ├── CMakeLists.txt
    └── src/
        ├── main.c                   # 命令解析入口
        ├── tcp_client.c/h           # TCP连接和消息收发
        └── output_format.c/h        # table/json/csv格式输出
```

---

## 4. 通信协议

### 4.1 IPC 消息帧格式

CLI工具与守护进程之间通过 TCP 通信，消息帧格式：

```
┌────────────────────┬──────────┬──────────────────────────┐
│  Length (4 bytes)  │  Type    │  Payload (JSON UTF-8)    │
│  Big Endian        │  1 byte  │  N bytes                 │
└────────────────────┴──────────┴──────────────────────────┘

Type:
  0x01 = JSON 请求/响应
  0x02 = JSON 事件推送（服务端主动发送）
```

### 4.2 命令列表

| 命令 | 方向 | 参数 | 说明 |
|------|------|------|------|
| `get_status` | CLI→服务 | 无 | 获取服务状态 |
| `get_nodes` | CLI→服务 | `sort`, `filter` | 获取节点列表 |
| `send_text` | CLI→服务 | `to`, `text`, `channel` | 发送文本消息 |
| `monitor_start` | CLI→服务 | 无 | 订阅实时帧事件 |
| `monitor_stop` | CLI→服务 | 无 | 取消订阅 |

### 4.3 事件类型（服务→CLI推送）

| 事件 | 数据 | 说明 |
|------|------|------|
| `frame_received` | mesh_event JSON | 收到新帧 |
| `node_updated` | node JSON | 节点信息更新 |
| `serial_status` | `connected`/`disconnected` | 串口状态变化 |

---

## 5. 配置文件

**路径**：`meshgateway.conf`（运行目录，或 `/etc/meshgateway.conf`）

```ini
[serial]
device = /dev/ttyUSB0      ; Windows 用 COM3
baudrate = 115200
reconnect_interval = 5     ; 秒

[network]
host = 127.0.0.1
port = 9999
max_clients = 16

[heartbeat]
interval = 600             ; 秒 (10分钟)
timeout = 30               ; 秒

[log]
level = info               ; debug/info/warn/error
file = meshgateway.log
max_size = 10              ; MB
max_files = 5

[node]
expire_time = 7200         ; 秒 (2小时)
```

---

## 6. CLI 工具使用

```bash
# 查看服务状态
meshgate-cli status

# 列出所有节点（表格格式）
meshgate-cli nodes

# JSON格式输出
meshgate-cli nodes --json

# 实时监控帧（持续输出直到 Ctrl+C）
meshgate-cli monitor

# 发送文本消息
meshgate-cli send --to !aabbccdd "hello world"

# 广播
meshgate-cli send "broadcast test"

# 指定服务地址（默认 127.0.0.1:9999）
meshgate-cli --host 192.168.1.100 --port 9999 status
```

---

## 7. 技术选型

| 模块 | 技术方案 | 原因 |
|------|----------|------|
| Protobuf 解析 | protobuf-c + 现有 `.pb-c.c` | 已生成42个消息文件，可直接使用 |
| JSON 解析/生成 | cJSON（单文件嵌入）| 轻量，无外部依赖，MIT许可 |
| 串口通信 | POSIX termios（Linux/macOS）+ Win32 API（Windows）| 原生API，零依赖 |
| IPC 通信 | TCP localhost | 最高跨平台兼容性 |
| 事件循环 | `select()` | 可移植到所有平台（Linux/Windows/macOS） |
| 线程 | pthreads（Linux/macOS）+ Win32 threads（Windows）| 通过宏抽象统一接口 |
| 构建系统 | CMake 3.15+ | 跨平台构建标准 |

---

## 8. 构建指南

### 8.1 依赖安装

**Linux (Debian/Ubuntu)：**
```bash
sudo apt-get install build-essential cmake libprotobuf-c-dev
```

**macOS：**
```bash
brew install cmake protobuf-c
```

**Windows（MinGW/MSYS2）：**
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-protobuf-c
```

**Windows（vcpkg）：**
```bash
vcpkg install protobuf-c
```

### 8.2 编译

```bash
cd claude_mesh
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 8.3 运行

```bash
# Linux/macOS
./meshgateway --port /dev/ttyUSB0 --config ../meshgateway.conf

# Windows
.\meshgateway.exe --port COM3 --config ..\meshgateway.conf

# 后台运行（Linux）
nohup ./meshgateway --port /dev/ttyUSB0 &

# 查看状态
./meshgate-cli status
```

---

## 9. 开发计划

### 阶段一：libmeshcore 核心库（当前）

- [x] 项目计划书
- [ ] CMake 骨架
- [ ] `mesh_types.h` - 公共数据类型
- [ ] `frame_parser.c/h` - 帧同步状态机
- [ ] `serial_port.c/h` - 跨平台串口
- [ ] `proto_parser.c/h` - FromRadio protobuf 解析
- [ ] `node_manager.c/h` - 节点表管理
- [ ] `heartbeat.c/h` - 心跳保活

### 阶段二：meshgateway 守护进程

- [ ] `log.c/h` - 日志模块
- [ ] `config.c/h` - INI 配置文件
- [ ] `event_loop.c/h` - select() 事件循环
- [ ] `tcp_server.c/h` - TCP IPC 服务器
- [ ] `command_handler.c/h` - JSON 命令处理
- [ ] `main.c` - 主程序

### 阶段三：meshgate-cli

- [ ] `tcp_client.c/h` - TCP 客户端
- [ ] `output_format.c/h` - 格式化输出
- [ ] `main.c` - 命令行解析（status/nodes/monitor/send）

---

## 10. 参考资料

| 资料 | 路径 | 用途 |
|------|------|------|
| Python参考实现 | `Project/CLI2/meshdebug/` | 行为参考，协议验证 |
| 帧解析参考 | `meshdebug/meshdebug/serial_worker.py` | 帧同步状态机 |
| 协议解析参考 | `meshdebug/meshdebug/proto_parser.py` | FromRadio解析逻辑 |
| 加密参考 | `meshdebug/meshdebug/pki_crypto.py` | PKI加密实现 |
| 架构规范 | `doc/spec/gateway/01~06_*.md` | 设计细节和代码模板 |
| Protobuf消息 | `protobufs_protobuf-c/meshtastic/mesh.pb-c.h` | 核心消息定义 |
