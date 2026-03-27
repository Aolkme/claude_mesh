# `PROJECT_PLAN.md` 验证报告

> 验证对象：`doc/plan/PROJECT_PLAN.md`
> 验证时间：2026-03-26
> 验证基线：当前工作区实际文件状态（包含未提交修改）
> 验证方式：静态代码核对 + 本地构建验证 + 帮助命令 smoke check

---

## 1. 验证范围与方法

本报告用于验证 [PROJECT_PLAN.md](./PROJECT_PLAN.md) 中的阶段进度总结、模块完成度描述与当前仓库实际状态是否一致。

本次验证基于以下事实来源：

- 计划文档正文与阶段状态声明
- `libmeshcore/`、`meshgateway/`、`meshgate-cli/`、`meshgateway-ui/` 当前源码
- `CMakeLists.txt` 与各子模块 `CMakeLists.txt`
- 本地构建结果与二进制帮助输出
- 当前工作树状态与可见文件清单

本次验证明确**不包含**以下内容：

- 真实串口设备连接验证
- Meshtastic 网络实机收发验证
- Web UI 在浏览器中的人工交互验收
- 长时间运行、异常恢复、性能指标、跨平台兼容性实测
- 与 `meshdebug/` Python 参考实现的逐字节行为比对

因此，本报告中的“已实现”表示：

- 仓库中已有对应代码或文件
- 当前构建链路可通过
- 能在静态层面找到端到端调用关系

本报告中的“未验证”或“部分吻合”表示：

- 代码存在，但未对真实行为进行完整验收
- 代码只完成了部分链路或仍存在 TODO / workaround / 未暴露接口
- 计划文档表述比当前事实更乐观

---

## 2. 总体结论

结论：**当前 `PROJECT_PLAN.md` 与实际情况“部分吻合”，但进度表述整体偏乐观。**

### 2.1 可以确认吻合的部分

- 项目已经明显超出“仅有规划”的阶段，核心库、守护进程、CLI、Web 端、前端目录都已有实质性实现。
- 阶段一描述的大部分核心能力已经在源码中落地，且当前可通过构建。
- 阶段二中的大部分主体工作已经落地，包括：
  - `cJSON` 集成
  - `Mongoose` 集成
  - `proto_builder`
  - `admin_builder`
  - `command_handler` 扩展
  - `meshgate-cli` 命令扩展
  - `meshgateway-ui` 目录与生产静态资源

### 2.2 明显不完全吻合的部分

- 计划首页状态写的是“**阶段二已完成，阶段三开发中（入网流程 + PKI）**”，这一表述比代码现实更乐观。
- 阶段二虽然主体已经成形，但仍有若干“已具备基础实现”与“已完整闭环”之间的差距。
- 阶段三在文档中被描述为“当前阶段”，但从代码与构建系统看，**尚未进入实质实现状态**，更接近“需求与接口预埋阶段”。
- 文档末尾给出的“**阶段二完成度：100% / 总体完成度：约 68%**”缺少可从仓库直接验证的量化依据。

### 2.3 本报告给出的最终判断

若只问“目前进度总结与实际情况是否吻合”，本报告的结论是：

- **阶段一：基本吻合**
- **阶段二：大体吻合，但“已完成”偏乐观**
- **阶段三：不够吻合，‘开发中’的说法明显超前于当前代码**
- **阶段四：仍为计划项，当前无争议**

---

## 3. 验证证据摘要

### 3.1 计划文档中的关键声明

`PROJECT_PLAN.md` 中最关键的进度声明如下：

- 第 4 行：`状态：阶段二已完成，阶段三开发中（入网流程 + PKI）`
- 第 444 行起：阶段一被标记为“已完成”
- 第 488 行起：阶段二被标记为“已完成”
- 第 567 行起：阶段三被标记为“当前阶段”
- 第 643-667 行：模块完成度表与“总体完成度约 68%”

### 3.2 当前工作树状态

在 `doc/plan/` 目录内，本次验证前仅发现一个正式计划文件：

- `doc/plan/PROJECT_PLAN.md`

同时，当前工作树中 `doc/plan/PROJECT_PLAN.md` 处于未提交修改状态，因此本报告以**当前工作区中的实际文件内容**作为验证基线，而不是仅以最近一次提交为准。

### 3.3 已确认的可执行事实

本地执行结果如下：

```bash
cmake --build build
```

结果：构建成功，生成 `meshcore`、`meshgateway`、`meshgate-cli`。

```bash
./build/meshgateway/meshgateway -h
```

结果：帮助输出正常，且明确声明服务默认“不自动连接串口”，通过 CLI 命令运行时连接。

```bash
./build/meshgate-cli/meshgate-cli -h
```

结果：帮助输出正常，当前 CLI 命令面包括：

- `status`
- `gateway-info`
- `nodes`
- `node`
- `connect`
- `disconnect`
- `send`
- `send-pos`
- `monitor`
- `admin passkey`
- `admin get-config`
- `admin get-channel`
- `admin reboot`

上述结果证明：

- 当前代码不是“未完成草稿”
- 核心后端与 CLI 已至少达到“可构建、可启动帮助入口”的状态

但这些结果**不能**证明：

- 阶段二所有行为已完整闭环
- Web UI 已做真实浏览器验收
- 阶段三入网与 PKI 已开始落地

---

## 4. 分阶段核对

## 4.1 阶段一：协议内核完整化

计划文档将阶段一标记为“已完成”，总体判断：**基本吻合**。

| 项目 | 计划声明 | 实际证据 | 判断 |
|------|----------|----------|------|
| 延迟连接 | 启动时不自动连串口，由命令触发连接 | `meshgateway -h` 输出明确说明默认不连接串口，需 `meshgate-cli connect` 触发 | 吻合 |
| `frame_builder` | 已新增并统一帧头编码 | 仓库存在 `libmeshcore/include/frame_builder.h`、`libmeshcore/src/frame_builder.c` | 吻合 |
| `proto_parser` PortNum 补全 | 位置、用户、路由、遥测、ADMIN、287 已分发 | `libmeshcore/src/proto_parser.c` 已扩展；文件清单与接口说明可对上 | 基本吻合 |
| `mesh_types.h` 扩展 | 双表类型、私有字段、packet 子结构体 | `libmeshcore/include/mesh_types.h` 已明显扩展，含私有字段与事件类型 | 吻合 |
| `node_manager` 双表设计 | 网关单条 + 远程节点表 + 字段级更新 | `libmeshcore/src/node_manager.c` 与 `include/node_manager.h` 已提供相关能力 | 基本吻合 |
| `heartbeat` 状态机 | 超时、重试、失败回调 | `libmeshcore/src/heartbeat.c`、`include/heartbeat.h` 已存在且被计划引用 | 基本吻合 |

### 阶段一结论

阶段一的代码落地程度较高，本次验证没有发现足以推翻“阶段一已完成”的明显反证。

需要保留的边界是：

- 这里的“已完成”是指**代码层面与构建层面**
- 不是指所有协议场景都已做过真实设备回归测试

---

## 4.2 阶段二：完整发送能力 + CLI + Web UI

计划文档将阶段二标记为“已完成”，总体判断：**主体实现已落地，但“已完成”表述偏乐观**。

### 4.2.1 可确认已经落地的部分

| 项目 | 计划声明 | 实际证据 | 判断 |
|------|----------|----------|------|
| `cJSON` 集成 | 已引入并重写命令处理 | 存在 `third_party/cJSON/cJSON.c/.h`，`meshgateway/src/command_handler.c` 已使用 `cJSON` | 吻合 |
| `proto_builder` | 文本、位置构造已实现 | 存在 `libmeshcore/include/proto_builder.h`、`src/proto_builder.c` | 吻合 |
| `admin_builder` | 基础 admin 构造已实现 | 存在 `libmeshcore/include/admin_builder.h`、`src/admin_builder.c` | 基本吻合 |
| 命令处理器扩展 | 状态、节点、发送、admin、monitor 命令已接入 | `meshgateway/src/command_handler.c` 可见完整分发链路 | 吻合 |
| CLI 扩展 | connect / send / send-pos / admin 等命令存在 | `meshgate-cli -h` 可见对应命令面 | 吻合 |
| Web 服务 | HTTP + WS + 静态资源服务 | 存在 `meshgateway/src/web_server.c/.h` 与 `third_party/mongoose/` | 基本吻合 |
| Vue UI | 前端工程和静态构建产物已存在 | 存在 `meshgateway-ui/` 源码与 `meshgateway/static/` 生产文件 | 基本吻合 |

### 4.2.2 需要下调表述的部分

#### 1. “完整发送能力”仍应保留边界

`proto_builder.c` 当前只明确实现了：

- 文本消息构造
- 位置消息构造
- `MeshPacket -> ToRadio` 包装

这与计划状态表中的描述“文本包 + 位置包构造；nodeinfo 包待补”是一致的，但也说明“完整发送能力”更准确的理解应当是：

- **核心发送链路已建立**
- **并非所有可能的上行消息类型都已补齐**

#### 2. `admin_get_session_passkey()` 仍带有 workaround 性质

`libmeshcore/src/admin_builder.c` 中可见：

- 代码注释明确指出 `get_session_passkey_request` 字段处理仍需后续适配
- 当前实现通过 `get_channel_request = 0` 的方式顺带触发 passkey 响应
- 源码内仍保留 TODO

这说明该能力虽然“已有可工作的构造实现”，但不应被解读为：

- 阶段三 Session Passkey 管理已经闭环
- admin 链路已经达到最终形态

#### 3. Web UI 已存在，不等于已完成完整验收

现有证据只能证明：

- Vue 3 + Vite 工程存在
- `Dashboard`、`Nodes`、`Messages`、`Monitor` 视图文件存在
- 生产构建产物已出现在 `meshgateway/static/`
- `web_server.c` 具备 HTTP / API / WS 路由

但本次验证并未实际完成：

- 浏览器端点击验收
- WebSocket 自动重连行为验收
- 页面与 TCP/串口实时事件联动验收

因此阶段二里的“Web UI 可访问”目前只能判定为：

- **静态资源与服务端入口已具备**
- **功能性验收仍未被本报告覆盖**

### 4.2.3 阶段二结论

阶段二不应被描述为“空泛乐观”或“基本没做”，这不符合事实。

更准确的事实是：

- 阶段二的大件已经做出来了
- 后端、CLI、Web、前端四条主线都能找到对应实现
- 当前构建可以通过
- 但“已完成”这一说法隐含的“已经完整闭环并充分验证”在本次证据下无法成立

因此，本报告将阶段二判断为：**大体吻合，但完成度描述偏高。**

---

## 4.3 阶段三：入网流程与 PKI

计划文档将阶段三标记为“当前阶段”，并在首页状态中写成“阶段三开发中（入网流程 + PKI）”。本报告判断：**该表述与当前代码现实不够吻合。**

### 4.3.1 当前已看到的“准备动作”

阶段三并非完全没有痕迹，当前可见的准备动作包括：

- `mesh_types.h` 与 `node_manager.*` 已为私有配置字段预留结构
- `proto_parser.c` 已能保留 `PORTNUM_PRIVATE_CONFIG(287)` 的原始 payload
- `protobufs_protobuf-c/thingseye/privateconfig.pb-c.[ch]` 已在仓库中

这些动作说明：

- 阶段三需求已被纳入整体设计
- 数据模型与解析入口为后续实现做了铺垫

### 4.3.2 当前缺失的核心实现

本次文件清单中**未发现**以下阶段三核心文件：

- `libmeshcore/include/private_config_handler.h`
- `libmeshcore/src/private_config_handler.c`
- `libmeshcore/include/pki_crypto.h`
- `libmeshcore/src/pki_crypto.c`

同时，在根和子模块 `CMakeLists.txt` 中未检出：

- `libsodium`
- `mbedtls`
- `pki_crypto`
- `private_config_handler`

这说明以下能力目前尚未进入实际实现状态：

- thingseye 私有协议的构造与解析封装
- X25519 / Ed25519 PKI 能力
- 与构建系统绑定的加密库集成
- Session Passkey 缓存与自动刷新逻辑

### 4.3.3 阶段三命令面尚未出现

`PROJECT_PLAN.md` 中阶段三验证命令包括：

- `meshgate-cli enroll-device --node ...`
- `meshgate-cli get-node ...`
- `meshgate-cli admin set-channel ...`
- `meshgate-cli change-admin --node ...`

但当前 `meshgate-cli -h` 中并未出现：

- `enroll-device`
- `change-admin`
- `admin set-channel`

因此，阶段三文档中的验证命令当前无法作为“已经进入开发”的事实支撑。

### 4.3.4 阶段三结论

以当前仓库状态来看，阶段三最准确的表述应理解为：

- **需求与接口方向已明确**
- **部分数据结构与解析入口已提前铺垫**
- **真正的功能实现尚未开始或尚未形成可见代码面**

因此，本报告认为首页“阶段三开发中（入网流程 + PKI）”的说法**偏乐观**。

---

## 4.4 阶段四：生产就绪

阶段四在计划文档中本身就是未完成计划项，本次验证未发现与该判断冲突的证据。

换句话说，阶段四仍然处于：

- 设计已列出
- 代码未声称完成
- 仓库现状与计划表述基本一致

---

## 5. 模块级证据矩阵

下表用于将计划文档中的“模块完成度”与当前事实做更细粒度映射。

| 模块/能力 | 计划中的说法 | 当前事实 | 判断 |
|-----------|--------------|----------|------|
| `frame_parser` | 基本完整 | 已存在，阶段一核心链路可对上 | 基本吻合 |
| `frame_builder` | 全流程使用 | 文件已存在，计划与结构吻合 | 吻合 |
| `serial_port` | 跨平台实现完整 | 文件存在，但未做跨平台实测 | 基本吻合 |
| `proto_parser` | 全 PortNum 分发 | 已扩展到 TEXT/POS/NODEINFO/ROUTING/TELEMETRY/ADMIN/287 | 基本吻合 |
| `proto_builder` | 文本/位置已完成 | 当前仅明确落地文本与位置，状态表自身已保留边界 | 吻合 |
| `admin_builder` | passkey/config/channel/reboot/factory_reset | 构造函数存在，但 passkey 请求仍带 TODO / workaround | 部分吻合 |
| `node_manager` | 双表 + 私有字段 + 字段级更新 | 已能从结构和接口看到落地 | 基本吻合 |
| `heartbeat` | 状态机完整 | 文件存在并在事件循环中被驱动 | 基本吻合 |
| `main.c` | 延迟连接 + web_server 集成 | 帮助输出和文件结构均支持此结论 | 吻合 |
| `config.c/h` | web/heartbeat 等字段已补 | 结构存在，但未逐配置项做行为验收 | 基本吻合 |
| `event_loop.c` | 全消息分发 + TCP+WS 双路广播 | 可见 `broadcast_event()` 与多个事件分发分支 | 基本吻合 |
| `tcp_server.c` | broadcast_event 正常工作 | 监控广播与订阅标记逻辑存在 | 基本吻合 |
| `command_handler.c` | 14 命令全部实现 | 当前分发代码与帮助命令面支持此说法 | 吻合 |
| `web_server.c/h` | HTTP + WS + 静态文件服务 | 路由、回退 HTML、WS 入口均存在 | 基本吻合 |
| `meshgateway-ui` | 4 视图可用 | 4 视图文件存在，产物存在，但未做浏览器验收 | 部分吻合 |
| `meshgate-cli` | 全命令集实现 | 现有 help 支持阶段二命令，但不支持阶段三验证命令 | 部分吻合 |
| `private_config_handler` | 0%，待阶段三 | 当前确实未建 | 吻合 |
| `pki_crypto` | 0%，待阶段三 | 当前确实未建 | 吻合 |

---

## 6. 主要偏差清单

以下偏差是本次验证最关键的结论，不需要通读全文也应单独看到。

### 偏差 1：计划首页“阶段二已完成”表述偏乐观

原因：

- 阶段二主体功能已落地，但完整闭环证据不足
- Web UI 只确认了工程与静态产物存在，未确认实际交互验收
- `admin_get_session_passkey` 仍存在 TODO 与 workaround 痕迹

结论：

- 不能把“已完成”理解为“已经全面验收并闭环”

### 偏差 2：计划首页“阶段三开发中”明显超前于当前代码

原因：

- 阶段三核心文件 `private_config_handler.*`、`pki_crypto.*` 尚不存在
- 构建系统中未集成 `libsodium` 或 `mbedtls`
- CLI 未暴露阶段三的关键命令

结论：

- 当前更接近“阶段三设计已展开，尚未形成主体实现”

### 偏差 3：总体完成度“约 68%”无法从仓库直接验证

原因：

- 文档未给出量化口径
- 当前状态表是经验判断，而非由可追溯指标计算得出

结论：

- 该数字可以保留为估算，但不应在验证语义上被视为“已证实”

### 偏差 4：阶段二验证脚本与阶段三验证脚本不应被视为已全部执行

原因：

- 本次仅验证了构建与 `-h` 帮助输出
- 未验证串口、节点、浏览器、入网、管理员更换等真实行为

结论：

- 文档中的验证命令更像“目标验收脚本”，不是当前已全部完成的验收记录

---

## 7. 最终判定

综合全部证据，本报告给出以下最终判定：

| 维度 | 判定 |
|------|------|
| 计划是否严重失真 | 否 |
| 计划是否整体可信 | 基本可信 |
| 计划是否需要独立验证说明 | 需要 |
| 计划中的阶段状态是否完全贴合当前代码 | 否 |
| 最接近事实的总评 | “阶段一完成，阶段二主体完成但未完全收口，阶段三尚未形成实质实现” |

若后续继续沿用 [PROJECT_PLAN.md](./PROJECT_PLAN.md) 作为进度对外说明，应始终与本报告配套阅读；否则读者很容易把“代码已存在”误读成“行为已完全验证”，或把“阶段三方向已明确”误读成“阶段三已进入实现”。

---

## 8. 本次验证使用的主要命令

以下命令已在本地执行，并作为本报告的直接依据之一：

```bash
git status --short -- doc/plan
git diff --stat -- CMakeLists.txt libmeshcore meshgateway meshgate-cli meshgateway-ui doc/plan/PROJECT_PLAN.md
rg --files doc/plan
rg --files libmeshcore/include libmeshcore/src meshgateway/src meshgate-cli/src meshgateway-ui meshgateway/static protobufs_protobuf-c/thingseye third_party
rg -n "TODO|FIXME|Phase|阶段|Milestone|里程碑|完成|已完成|未完成|进行中|progress|plan" doc meshgateway libmeshcore meshgate-cli
rg -n "private_config|enroll|change-admin|change_admin|pki_|libsodium|mbedtls|session_passkey|set_channel|set_config" libmeshcore meshgateway meshgate-cli meshgateway-ui
cmake --build build
./build/meshgateway/meshgateway -h
./build/meshgate-cli/meshgate-cli -h
```

说明：

- `rg` 的“未命中”被用于佐证某些模块或依赖当前未出现
- 构建与帮助命令结果被用于佐证“代码已落地并可生成可执行入口”
- 这些命令不构成真实硬件验收，只构成当前仓库状态验证
