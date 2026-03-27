/**
 * proto_builder.c - ToRadio / MeshPacket 构造实现
 *
 * 直接使用 protobufs_protobuf-c/ 下的编译文件构造消息。
 */
#include "proto_builder.h"
#include "meshtastic/mesh.pb-c.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ─── 自增包 ID ─── */
static uint32_t s_packet_id = 1;
static uint32_t next_packet_id(uint32_t hint) {
    if (hint != 0) return hint;
    if (s_packet_id == 0) s_packet_id = 1;
    return s_packet_id++;
}

/* ─── 内部：将 MeshPacket 封装为 ToRadio 并序列化 ─── */
static mesh_error_t pack_to_radio_with_packet(Meshtastic__MeshPacket *mp,
                                               uint8_t *buf, size_t buf_len,
                                               size_t *out_len)
{
    Meshtastic__ToRadio tr = MESHTASTIC__TO_RADIO__INIT;
    tr.payload_variant_case = MESHTASTIC__TO_RADIO__PAYLOAD_VARIANT_PACKET;
    tr.packet = mp;

    size_t needed = meshtastic__to_radio__get_packed_size(&tr);
    if (needed > buf_len) return MESH_ERROR_BUFFER_FULL;
    *out_len = meshtastic__to_radio__pack(&tr, buf);
    return MESH_OK;
}

/* ─────────────────── 文本消息 ─────────────────── */

mesh_error_t proto_builder_text(uint32_t from_num, uint32_t to_num,
                                 const char *text, uint8_t channel,
                                 uint8_t hop_limit, bool want_ack,
                                 uint32_t packet_id,
                                 uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!text || !buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    ProtobufCBinaryData payload_data = {
        .len  = strlen(text),
        .data = (uint8_t *)(uintptr_t)text   /* protobuf-c 只读 */
    };

    Meshtastic__Data data_msg = MESHTASTIC__DATA__INIT;
    data_msg.portnum       = MESHTASTIC__PORT_NUM__TEXT_MESSAGE_APP;
    data_msg.payload       = payload_data;
    data_msg.want_response = false;

    Meshtastic__MeshPacket mp = MESHTASTIC__MESH_PACKET__INIT;
    mp.from      = from_num;
    mp.to        = to_num;
    mp.id        = next_packet_id(packet_id);
    mp.channel   = channel;
    mp.hop_limit = hop_limit ? hop_limit : 3;
    mp.want_ack  = want_ack;
    mp.payload_variant_case = MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_DECODED;
    mp.decoded   = &data_msg;

    return pack_to_radio_with_packet(&mp, buf, buf_len, out_len);
}

/* ─────────────────── 位置消息 ─────────────────── */

mesh_error_t proto_builder_position(uint32_t from_num, uint32_t to_num,
                                     double lat, double lon, int32_t alt,
                                     uint8_t channel,
                                     uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    /* 构造 Position payload */
    Meshtastic__Position pos = MESHTASTIC__POSITION__INIT;
    pos.latitude_i  = (int32_t)(lat * 1e7);
    pos.longitude_i = (int32_t)(lon * 1e7);
    pos.altitude    = alt;
    pos.timestamp   = (uint32_t)time(NULL);
    pos._latitude_i_case  = MESHTASTIC__POSITION___LATITUDE_I_LATITUDE_I;
    pos._longitude_i_case = MESHTASTIC__POSITION___LONGITUDE_I_LONGITUDE_I;

    /* 先序列化 Position 为中间 buf */
    size_t pos_sz = meshtastic__position__get_packed_size(&pos);
    uint8_t pos_buf[256];
    if (pos_sz > sizeof(pos_buf)) return MESH_ERROR_BUFFER_FULL;
    meshtastic__position__pack(&pos, pos_buf);

    ProtobufCBinaryData payload_data = { .len = pos_sz, .data = pos_buf };

    Meshtastic__Data data_msg = MESHTASTIC__DATA__INIT;
    data_msg.portnum       = MESHTASTIC__PORT_NUM__POSITION_APP;
    data_msg.payload       = payload_data;
    data_msg.want_response = false;

    Meshtastic__MeshPacket mp = MESHTASTIC__MESH_PACKET__INIT;
    mp.from      = from_num;
    mp.to        = to_num;
    mp.id        = next_packet_id(0);
    mp.channel   = channel;
    mp.hop_limit = 3;
    mp.payload_variant_case = MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_DECODED;
    mp.decoded   = &data_msg;

    return pack_to_radio_with_packet(&mp, buf, buf_len, out_len);
}

/* ─────────────────── 通用 ToRadio 封装 ─────────────────── */

mesh_error_t proto_builder_wrap_to_radio(const uint8_t *mp_buf, size_t mp_len,
                                          uint8_t *buf, size_t buf_len,
                                          size_t *out_len)
{
    if (!mp_buf || !buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    /* mp_buf 是已序列化的 MeshPacket，直接解包再包为 ToRadio */
    Meshtastic__MeshPacket *mp =
        meshtastic__mesh_packet__unpack(NULL, mp_len, mp_buf);
    if (!mp) return MESH_ERROR_PROTO_DECODE;

    mesh_error_t err = pack_to_radio_with_packet(mp, buf, buf_len, out_len);
    meshtastic__mesh_packet__free_unpacked(mp, NULL);
    return err;
}
