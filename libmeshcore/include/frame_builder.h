/**
 * frame_builder.h - 串口帧构造接口
 *
 * 统一添加 Meshtastic 串口帧头 [0x94][0xC3][len_H][len_L]，
 * 所有 ToRadio 发送路径必须经过此接口，禁止手写帧头。
 */
#ifndef FRAME_BUILDER_H
#define FRAME_BUILDER_H

#include <stdint.h>
#include <stddef.h>
#include "mesh_types.h"

/**
 * 将 protobuf payload 编码为完整串口帧
 *
 * 帧格式：[0x94][0xC3][len_H][len_L][payload...]
 *
 * @param payload     protobuf 序列化后的数据
 * @param payload_len payload 长度（字节），最大 65535
 * @param buf_out     输出缓冲区（调用者提供）
 * @param buf_len     输出缓冲区大小（需 >= payload_len + 4）
 * @param frame_len   输出：编码后的完整帧长度
 * @return MESH_OK 成功，MESH_ERROR_BUFFER_FULL 缓冲区不足，
 *         MESH_ERROR_INVALID_PARAM 参数无效
 */
mesh_error_t frame_builder_encode(const uint8_t *payload, size_t payload_len,
                                   uint8_t *buf_out, size_t buf_len,
                                   size_t *frame_len);

/**
 * 获取编码后帧的总长度（不实际编码）
 *
 * @param payload_len  payload 长度
 * @return 帧总长度（payload_len + 4）
 */
static inline size_t frame_builder_frame_size(size_t payload_len) {
    return payload_len + 4;
}

#endif /* FRAME_BUILDER_H */
