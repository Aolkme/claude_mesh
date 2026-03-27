/**
 * proto_parser.h - protobuf-c FromRadio 解析 + 基础 ToRadio 构造接口
 *
 * 解析：FromRadio protobuf → mesh_event_t（含完整 PortNum payload 分发）
 * 构造：基础 ToRadio 消息（want_config、heartbeat、文本包）
 *
 * 注意：完整的 packet 构造由 proto_builder.h 提供；
 *       Admin 消息构造由 admin_builder.h 提供；
 *       私有配置(287)收发由 private_config_handler.h 提供。
 */
#ifndef PROTO_PARSER_H
#define PROTO_PARSER_H

#include "mesh_types.h"

/* ─────────────── 解析接口 ─────────────── */

/**
 * 解析 FromRadio protobuf 二进制数据 → mesh_event_t
 *
 * 对 MESH_EVENT_PACKET 事件，会按 PortNum 自动分发解析：
 *   - PORTNUM_TEXT_MESSAGE(1)   → packet.text / packet.is_text
 *   - PORTNUM_POSITION(3)       → packet.position
 *   - PORTNUM_NODEINFO(4)       → packet.user
 *   - PORTNUM_ROUTING(5)        → packet.routing
 *   - PORTNUM_TELEMETRY(67)     → packet.telemetry
 *   - PORTNUM_ADMIN(6)          → packet.admin_payload（原始二进制，供 admin_builder 解析）
 *   - PORTNUM_PRIVATE_CONFIG(287) → packet.private_payload（原始二进制，供 private_config_handler 解析）
 *   - 其他                      → packet.payload（原始二进制）
 *
 * @param data       protobuf 二进制数据
 * @param len        数据长度
 * @param event_out  输出事件（调用者提供存储）
 * @return MESH_OK 成功，MESH_ERROR_PROTO_DECODE 解析失败
 */
mesh_error_t proto_parse_from_radio(const uint8_t *data, uint16_t len,
                                     mesh_event_t *event_out);

/* ─────────────── 基础构造接口 ─────────────── */

/**
 * 构造 ToRadio want_config_id 的 protobuf payload
 *
 * @param config_id  配置请求 ID（通常用 time(NULL) 生成）
 * @param buf        输出缓冲区
 * @param buf_len    缓冲区大小
 * @param out_len    实际序列化长度
 */
mesh_error_t proto_build_want_config(uint32_t config_id,
                                      uint8_t *buf, size_t buf_len,
                                      size_t *out_len);

/**
 * 构造 ToRadio heartbeat 的 protobuf payload
 */
mesh_error_t proto_build_heartbeat(uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 构造文本消息 MeshPacket 的 ToRadio protobuf payload
 *
 * @param from_num   发送节点号（网关自身 node_num）
 * @param to_num     目标节点号（MESH_BROADCAST_NUM = 广播）
 * @param text       文本内容（UTF-8，最大 MESH_TEXT_MAX_LEN）
 * @param channel    信道索引（0-7）
 * @param hop_limit  跳数限制（0 = 使用固件默认值 3）
 * @param want_ack   是否需要 ACK
 * @param packet_id  包 ID（0 = 自动生成）
 */
mesh_error_t proto_build_text_packet(uint32_t from_num, uint32_t to_num,
                                      const char *text, uint8_t channel,
                                      uint8_t hop_limit, bool want_ack,
                                      uint32_t packet_id,
                                      uint8_t *buf, size_t buf_len,
                                      size_t *out_len);

#endif /* PROTO_PARSER_H */
