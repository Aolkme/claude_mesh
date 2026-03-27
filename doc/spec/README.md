# Meshtastic C/C++ 内核开发规范文档

> 本文档集供下一个 AI 参考，用于编写 C/C++ 版本的 Meshtastic 协议解析与构造内核。

## 文档列表

| 序号 | 文档 | 主要内容 | 关键技术点 |
|------|------|----------|------------|
| 01 | [帧协议规范](./01_帧协议规范.md) | 串口帧格式、状态机解析、握手协议 | 帧同步、状态机、want_config |
| 02 | [解析器内核设计](./02_解析器内核设计.md) | FromRadio/MeshPacket解析、PortNum处理、JSON输出 | nanopb、protobuf解码、字段映射 |
| 03 | [构造器内核设计](./03_构造器内核设计.md) | MeshPacket/ToRadio构造、各类型payload构建 | protobuf编码、AdminMessage、PKI加密 |
| 04 | [扩展协议规范](./04_扩展协议规范.md) | PRIVATE_CONFIG(287)/WAKEUP_COMM(288)私有协议 | 自定义协议、同步唤醒配置 |
| 05 | [心跳与连接管理](./05_心跳与连接管理.md) | 心跳机制、连接状态管理、断线重连 | 状态机、指数退避、want_config心跳 |

---

## 核心参考资料

### Proto 文件
| 文件 | 说明 |
|------|------|
| `protobufs/meshtastic/mesh.proto` | FromRadio/ToRadio/MeshPacket/NodeInfo/Position/User 定义 |
| `protobufs/meshtastic/portnums.proto` | PortNum 枚举定义 |
| `protobufs/meshtastic/admin.proto` | AdminMessage 消息定义 |
| `protobufs/meshtastic/telemetry.proto` | Telemetry/DeviceMetrics/EnvironmentMetrics 定义 |
| `protobufs/meshtastic/config.proto` | Config/DeviceConfig/PositionConfig/PowerConfig 定义 |
| `protobufs/meshtastic/module_config.proto` | ModuleConfig 定义 |

### C/C++ 头文件 (nanopb 生成)
| 目录 | 说明 |
|------|------|
| `protoHCPP/meshtastic/*.pb.h` | nanopb 生成的 C 头文件，可直接使用 |

### Python 实现参考
| 文件 | 说明 |
|------|------|
| `meshtastic-python/meshtastic/stream_interface.py` | 帧解析实现 |
| `meshtastic-python/meshtastic/mesh_interface.py` | 发送/接收/心跳实现 |
| `meshdebug/serial_worker.py` | 串口工作线程 |
| `meshdebug/proto_parser.py` | 协议解析器 |
| `meshdebug/widgets/send_panel.py` | 发送面板实现 |
| `meshdebug/pki_crypto.py` | PKI 加密实现 |

---

## 核心功能需求

### 1. 解析内核

```
串口数据 → 帧解析 → FromRadio解析 → MeshPacket解析 → Payload解析 → JSON输出
```

**关键点：**
- 帧同步状态机 (0x94 0xC3)
- FromRadio variant 类型判断
- MeshPacket 字段映射 (from → from_id, to → to_id)
- PortNum payload 类型路由
- JSON 格式规范 (snake_case)

### 2. 构造内核

```
用户参数 → Payload构建 → MeshPacket构建 → (可选)PKI加密 → ToRadio构建 → 帧发送
```

**关键点：**
- 默认参数设置
- Payload 类型构造函数
- AdminMessage 完整操作支持
- Session Passkey 自动注入
- PKI 加密 (X25519 + AES-256-CCM)

### 3. 心跳机制

- 间隔: 10-14 分钟 (推荐 10 分钟)
- 类型: ToRadio.want_config_id
- 超时: 30 秒
- 重试: 3 次

### 4. 扩展协议

- **PortNum 287 (PRIVATE_CONFIG_APP):** 私有配置协议
- **PortNum 288 (WAKEUP_COMM_APP):** 唤醒命令协议

---

## 建议的项目结构

```
meshcore/
├── include/
│   ├── frame_protocol.h      # 帧协议定义
│   ├── frame_parser.h        # 帧解析器
│   ├── frame_builder.h       # 帧构建器
│   ├── from_radio.h          # FromRadio 解析
│   ├── mesh_packet.h         # MeshPacket 定义
│   ├── portnum_handler.h     # PortNum 处理
│   ├── payload_builder.h     # Payload 构建
│   ├── admin_builder.h       # Admin 消息构建
│   ├── private_config.h      # 私有配置协议
│   ├── wakeup_comm.h         # 唤醒命令协议
│   ├── pki_crypto.h          # PKI 加密
│   ├── session_manager.h     # Session 管理
│   ├── heartbeat.h           # 心跳管理
│   ├── connection_manager.h  # 连接管理
│   ├── json_builder.h        # JSON 构建
│   └── serial_port.h         # 串口封装
├── src/
│   ├── frame_parser.c
│   ├── frame_builder.c
│   ├── from_radio.c
│   ├── mesh_packet.c
│   ├── portnum_handler.c
│   ├── payload_builder.c
│   ├── admin_builder.c
│   ├── private_config.c
│   ├── wakeup_comm.c
│   ├── pki_crypto.c
│   ├── session_manager.c
│   ├── heartbeat.c
│   ├── connection_manager.c
│   ├── json_builder.c
│   └── serial_port.c
├── proto/                    # nanopb 生成的文件
│   ├── mesh.pb.h
│   ├── mesh.pb.c
│   ├── admin.pb.h
│   ├── admin.pb.c
│   ├── telemetry.pb.h
│   ├── telemetry.pb.c
│   └── ...
└── CMakeLists.txt
```

---

## 依赖库

| 库 | 用途 | 可选 |
|-----|------|------|
| nanopb | Protobuf 编解码 | 必需 |
| mbedtls / openssl | X25519, AES-256-CCM | 可选 (PKI加密) |
| serial / libserialport | 串口通信 | 必需 |

---

## 快速开始

### 1. 编译 nanopb 文件

```bash
# 使用 nanopb 生成 C 代码
protoc --nanopb_out=. mesh.proto
protoc --nanopb_out=. admin.proto
protoc --nanopb_out=. telemetry.proto
```

### 2. 基本使用示例

```c
#include "frame_parser.h"
#include "from_radio.h"
#include "connection_manager.h"

int main() {
    // 1. 初始化
    connection_manager_t conn;
    connection_manager_init(&conn, NULL);
    
    // 2. 打开串口
    serial_port_t *port = serial_open("COM3", 115200);
    
    // 3. 开始连接
    connection_manager_connect(&conn);
    
    // 4. 主循环
    while (running) {
        // 读取并处理数据...
        // 心跳检查...
    }
    
    // 5. 清理
    serial_close(port);
    return 0;
}
```

---

## 注意事项

1. **字节序:** 帧长度字段是大端，Protobuf 内部是小端 varint
2. **节点ID:** 格式为 `!xxxxxxxx`，广播地址为 `broadcast`
3. **Session Passkey:** Admin SET 操作需要注入，有效期 ~270 秒
4. **PKI 加密:** 需要 mbedtls 或 openssl 支持
5. **JSON 格式:** 统一使用 snake_case 字段名

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2024-01-15 | 初始版本 |