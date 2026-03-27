# Meshtastic 嵌入式网关管理系统

> 本文档集供下一个 AI 参考，用于编写 Linux 服务架构的 Meshtastic 网关管理系统。

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           Linux System                                   │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                         systemd                                    │  │
│  │  ┌─────────────────────────────────────────────────────────────┐  │  │
│  │  │              meshgateway.service                             │  │  │
│  │  │  ┌────────────────────────────────────────────────────────┐ │  │  │
│  │  │  │                  meshgateway daemon                     │ │  │  │
│  │  │  │  ┌─────────────┐    ┌─────────────┐    ┌────────────┐ │ │  │  │
│  │  │  │  │  Serial     │    │   Core      │    │  Socket    │ │ │  │  │
│  │  │  │  │  Manager    │───▶│   Engine    │───▶│  Server    │ │ │  │  │
│  │  │  │  └─────────────┘    └─────────────┘    └─────┬──────┘ │ │  │  │
│  │  │  └──────────────────────────────────────────────┼────────┘ │  │  │
│  │  └─────────────────────────────────────────────────┼──────────┘  │  │
│  │                                                    │             │  │
│  │                        Unix Domain Socket          │             │  │
│  │                    /run/meshgateway.sock          │             │  │
│  │                                                    ▼             │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│                                                                        │
│  ┌─────────────────────────┐      ┌─────────────────────────────────┐  │
│  │      meshgate-cli       │      │       meshgate-ui (Qt)          │  │
│  │      (C CLI工具)         │      │       (Qt 图形界面)              │  │
│  └─────────────────────────┘      └─────────────────────────────────┘  │
│                                                                        │
└─────────────────────────────────────────────────────────────────────────┘
```

## 文档列表

| 序号 | 文档 | 主要内容 | 页数 |
|------|------|----------|------|
| 01 | [系统架构设计](./01_系统架构设计.md) | 整体架构、模块划分、数据流、配置管理 | ~15页 |
| 02 | [后台服务设计](./02_后台服务设计.md) | 守护进程、事件循环、线程模型、串口管理 | ~25页 |
| 03 | [Socket通信协议](./03_Socket通信协议.md) | 消息格式、命令定义、事件推送、错误码 | ~20页 |
| 04 | [CLI工具设计](./04_CLI工具设计.md) | 命令列表、参数解析、输出格式 | ~15页 |
| 05 | [UI客户端接口](./05_UI客户端接口.md) | Qt架构、Socket客户端、数据模型 | ~20页 |
| 06 | [部署与运维](./06_部署与运维.md) | systemd配置、编译打包、日志管理 | ~15页 |

---

## 核心参考资料

### 前置文档

| 文档 | 说明 |
|------|------|
| `spec/01_帧协议规范.md` | 帧格式、状态机解析 |
| `spec/02_解析器内核设计.md` | FromRadio/MeshPacket解析 |
| `spec/03_构造器内核设计.md` | MeshPacket构建 |
| `spec/04_扩展协议规范.md` | 自定义端口协议 |
| `spec/05_心跳与连接管理.md` | 心跳机制 |

### Proto 文件

| 文件 | 说明 |
|------|------|
| `protobufs/meshtastic/mesh.proto` | FromRadio/ToRadio/MeshPacket |
| `protobufs/meshtastic/portnums.proto` | PortNum 枚举 |
| `protobufs/meshtastic/admin.proto` | AdminMessage |
| `protobufs/meshtastic/telemetry.proto` | Telemetry |

### nanopb C 头文件

| 目录 | 说明 |
|------|------|
| `protoHCPP/meshtastic/*.pb.h` | nanopb 生成的 C 头文件 |

---

## 核心功能需求

### 1. 后台服务 (meshgateway)

```
特性:
- C语言编写，Linux守护进程
- systemd 服务管理
- 串口数据读取与解析
- Unix Domain Socket 服务端
- 节点数据库管理
- 心跳机制

核心模块:
- Serial Manager    串口管理
- Socket Server     客户端通信
- Node Manager      节点管理
- Event Loop        事件循环
- Command Handler   命令处理
```

### 2. CLI 工具 (meshgate-cli)

```
特性:
- 轻量级 C 程序
- 连接后台服务
- 多种输出格式

核心命令:
- status        服务状态
- nodes         节点列表
- send          发送消息
- admin         Admin命令
- listen        事件监听
- config        配置管理
```

### 3. Qt UI 客户端 (meshgate-ui)

```
特性:
- Qt5/Qt6 图形界面
- 实时数据展示
- 发送面板
- 事件订阅

核心组件:
- SocketClient       通信客户端
- NodeModel          节点数据模型
- MainWindow         主窗口
- SendPanel          发送面板
```

---

## 通信协议

### 消息格式

```json
// 请求
{"id": 1, "cmd": "get_nodes", "params": {}}

// 响应
{"id": 1, "status": "ok", "data": {"nodes": [...]}}

// 事件
{"event": "node_updated", "data": {...}}
```

### 主要命令

| 命令 | 说明 |
|------|------|
| get_status | 服务状态 |
| get_nodes | 节点列表 |
| get_node | 节点详情 |
| send_text | 发送文本 |
| send_position | 发送位置 |
| send_admin | Admin命令 |
| subscribe | 订阅事件 |

---

## 目录结构

```
/opt/meshgateway/
├── bin/
│   ├── meshgateway         # 守护进程
│   ├── meshgate-cli        # CLI工具
│   └── meshgate-ui         # Qt UI
├── etc/
│   └── meshgateway.conf    # 配置文件
├── var/
│   ├── nodes.db            # 节点数据库
│   └── logs/               # 日志目录
└── run/
    └── meshgateway.sock    # Unix Socket
```

---

## 快速开始

### 编译

```bash
# 编译服务
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 编译 CLI
cd ../cli && mkdir build && cd build
cmake .. && make

# 编译 UI
cd ../../ui && mkdir build && cd build
cmake .. && make
```

### 安装

```bash
sudo make install
sudo systemctl daemon-reload
sudo systemctl enable meshgateway
sudo systemctl start meshgateway
```

### 使用

```bash
# 查看状态
meshgate-cli status

# 列出节点
meshgate-cli nodes

# 发送消息
meshgate-cli send "Hello World"

# 启动 UI
meshgate-ui
```

---

## 技术要点

### 1. 守护进程

```c
// 脱离终端
setsid();
chdir("/");
umask(0);

// 信号处理
signal(SIGTERM, signal_handler);
signal(SIGINT, signal_handler);
signal(SIGHUP, reload_config);

// 通知 systemd
sd_notify(0, "READY=1");
```

### 2. Unix Domain Socket

```c
int fd = socket(AF_UNIX, SOCK_STREAM, 0);

struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
strcpy(addr.sun_path, "/run/meshgateway.sock");

bind(fd, (struct sockaddr*)&addr, sizeof(addr));
listen(fd, 16);
```

### 3. JSON 消息

```c
// 构造请求
char request[1024];
snprintf(request, sizeof(request), 
    "{\"id\":%d,\"cmd\":\"%s\",\"params\":%s}", 
    id, cmd, params);

// 发送
send(fd, request, strlen(request), 0);
send(fd, "\n", 1, 0);
```

---

## 注意事项

1. **服务与客户端解耦:** 服务独立运行，客户端只是遥控器
2. **Socket 权限:** 0666 允许所有用户连接
3. **日志管理:** 日志轮转防止磁盘占满
4. **资源限制:** MemoryMax=50M 适配嵌入式
5. **安全加固:** NoNewPrivileges, ProtectSystem

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2024-01-15 | 初始版本 |