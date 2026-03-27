/**
 * frame_builder.c - 串口帧构造实现
 *
 * 帧格式：[0x94][0xC3][len_H][len_L][FromRadio/ToRadio protobuf payload]
 * 参考：doc/spec/01_帧协议规范.md §2
 */
#include "frame_builder.h"
#include <string.h>

/* 帧魔数 */
#define FRAME_MAGIC1  0x94u
#define FRAME_MAGIC2  0xC3u

mesh_error_t frame_builder_encode(const uint8_t *payload, size_t payload_len,
                                   uint8_t *buf_out, size_t buf_len,
                                   size_t *frame_len)
{
    if (!payload || !buf_out || !frame_len) return MESH_ERROR_INVALID_PARAM;
    if (payload_len > 0xFFFFu)             return MESH_ERROR_INVALID_PARAM;

    size_t total = payload_len + 4;
    if (buf_len < total)                   return MESH_ERROR_BUFFER_FULL;

    buf_out[0] = FRAME_MAGIC1;
    buf_out[1] = FRAME_MAGIC2;
    buf_out[2] = (uint8_t)((payload_len >> 8) & 0xFF);
    buf_out[3] = (uint8_t)(payload_len & 0xFF);
    memcpy(buf_out + 4, payload, payload_len);

    *frame_len = total;
    return MESH_OK;
}
