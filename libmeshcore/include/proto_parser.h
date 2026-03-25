/**
 * proto_parser.h - protobuf-c FromRadio 解析接口
 *
 * 将 FromRadio protobuf 二进制解析为 mesh_event_t 结构体
 * 参考：meshdebug/proto_parser.py
 */
#ifndef PROTO_PARSER_H
#define PROTO_PARSER_H

#include "mesh_types.h"

/**
 * 解析 FromRadio protobuf 二进制数据
 *
 * @param data        protobuf 二进制数据
 * @param len         数据长度
 * @param event_out   输出事件（调用者提供存储）
 * @return MESH_OK 成功，其他值表示错误
 */
mesh_error_t proto_parse_from_radio(const uint8_t *data, uint16_t len,
                                     mesh_event_t *event_out);

/**
 * 构造 ToRadio want_config_id 帧的 protobuf payload
 *
 * @param config_id    配置请求ID（通常是随机数）
 * @param buf          输出缓冲区
 * @param buf_len      缓冲区大小
 * @param out_len      输出：实际序列化长度
 * @return MESH_OK 成功
 */
mesh_error_t proto_build_want_config(uint32_t config_id,
                                      uint8_t *buf, size_t buf_len,
                                      size_t *out_len);

/**
 * 构造 ToRadio heartbeat 帧的 protobuf payload
 */
mesh_error_t proto_build_heartbeat(uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 构造 ToRadio 文本消息 MeshPacket 的 protobuf payload
 *
 * @param from_num  发送节点号
 * @param to_num    目标节点号（0xFFFFFFFF = 广播）
 * @param text      文本内容（UTF-8）
 * @param channel   信道索引（0-7）
 * @param packet_id 包ID（0 = 自动生成）
 */
mesh_error_t proto_build_text_packet(uint32_t from_num, uint32_t to_num,
                                      const char *text, uint8_t channel,
                                      uint32_t packet_id,
                                      uint8_t *buf, size_t buf_len,
                                      size_t *out_len);

#endif /* PROTO_PARSER_H */
