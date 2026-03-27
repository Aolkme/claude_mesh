/**
 * proto_builder.h - ToRadio / MeshPacket 构造接口
 *
 * 提供完整字段参数的消息构造函数。
 * 基础控制消息（want_config, heartbeat）保留在 proto_parser.h。
 * Admin 消息构造由 admin_builder.h 提供。
 */
#ifndef PROTO_BUILDER_H
#define PROTO_BUILDER_H

#include "mesh_types.h"

/**
 * 构造文本消息 MeshPacket 的 ToRadio protobuf payload
 *
 * @param from_num   发送节点号（网关自身 node_num）
 * @param to_num     目标节点号（MESH_BROADCAST_NUM = 广播）
 * @param text       文本内容（UTF-8，最大 MESH_TEXT_MAX_LEN）
 * @param channel    信道索引（0–7）
 * @param hop_limit  跳数限制（0 = 使用固件默认值 3）
 * @param want_ack   是否请求 ACK
 * @param packet_id  包 ID（0 = 自动递增）
 */
mesh_error_t proto_builder_text(uint32_t from_num, uint32_t to_num,
                                 const char *text, uint8_t channel,
                                 uint8_t hop_limit, bool want_ack,
                                 uint32_t packet_id,
                                 uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 构造位置消息 MeshPacket 的 ToRadio protobuf payload
 *
 * @param from_num   发送节点号
 * @param to_num     目标节点号（通常广播）
 * @param lat        纬度（度）
 * @param lon        经度（度）
 * @param alt        海拔高度（米）
 * @param channel    信道索引
 */
mesh_error_t proto_builder_position(uint32_t from_num, uint32_t to_num,
                                     double lat, double lon, int32_t alt,
                                     uint8_t channel,
                                     uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 通用 MeshPacket → ToRadio 封装
 * 供 admin_builder 等模块将已构造的 MeshPacket 打包为 ToRadio
 *
 * @param mp_buf     已序列化的 MeshPacket protobuf（由调用者构造）
 * @param mp_len     MeshPacket 长度
 * @param buf        输出缓冲区
 * @param buf_len    输出缓冲区大小
 * @param out_len    实际序列化长度
 */
mesh_error_t proto_builder_wrap_to_radio(const uint8_t *mp_buf, size_t mp_len,
                                          uint8_t *buf, size_t buf_len,
                                          size_t *out_len);

#endif /* PROTO_BUILDER_H */
