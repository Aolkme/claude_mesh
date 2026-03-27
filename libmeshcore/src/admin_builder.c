/**
 * admin_builder.c - AdminMessage (PortNum 6) 构造实现
 *
 * 直接调用 meshtastic__admin_message__pack() 序列化，
 * 再封装为 MeshPacket + ToRadio。
 */
#include "admin_builder.h"
#include "meshtastic/mesh.pb-c.h"
#include "meshtastic/admin.pb-c.h"
#include <string.h>
#include <time.h>

/* ─── 自增包 ID（与 proto_builder 各自独立）─── */
static uint32_t s_admin_pkt_id = 0x1000;

/* ─────────────────────────────────────────────────────────────
 * 内部：将 AdminMessage 序列化，再封装为 MeshPacket + ToRadio
 * ─────────────────────────────────────────────────────────── */
static mesh_error_t pack_admin_to_radio(Meshtastic__AdminMessage *adm,
                                         uint32_t dest_num, uint32_t from_num,
                                         uint8_t *buf, size_t buf_len,
                                         size_t *out_len)
{
    /* 1. 序列化 AdminMessage */
    size_t adm_sz = meshtastic__admin_message__get_packed_size(adm);
    if (adm_sz > 512) return MESH_ERROR_BUFFER_FULL;

    uint8_t adm_buf[512];
    meshtastic__admin_message__pack(adm, adm_buf);

    /* 2. 放入 Data payload */
    ProtobufCBinaryData payload_data = { .len = adm_sz, .data = adm_buf };

    Meshtastic__Data data_msg = MESHTASTIC__DATA__INIT;
    data_msg.portnum  = MESHTASTIC__PORT_NUM__ADMIN_APP;
    data_msg.payload  = payload_data;
    data_msg.want_response = true;

    /* 3. 封装为 MeshPacket（PKI 加密由固件处理；此处发送明文，
          固件会自动对 admin 包启用 PKI 加密）*/
    Meshtastic__MeshPacket mp = MESHTASTIC__MESH_PACKET__INIT;
    mp.from      = from_num;
    mp.to        = dest_num;
    mp.id        = s_admin_pkt_id++;
    mp.channel   = 0;       /* admin 走默认信道 */
    mp.hop_limit = 3;
    mp.want_ack  = true;
    mp.payload_variant_case = MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_DECODED;
    mp.decoded   = &data_msg;

    /* 4. 封装为 ToRadio */
    Meshtastic__ToRadio tr = MESHTASTIC__TO_RADIO__INIT;
    tr.payload_variant_case = MESHTASTIC__TO_RADIO__PAYLOAD_VARIANT_PACKET;
    tr.packet = &mp;

    size_t needed = meshtastic__to_radio__get_packed_size(&tr);
    if (needed > buf_len) return MESH_ERROR_BUFFER_FULL;
    *out_len = meshtastic__to_radio__pack(&tr, buf);
    return MESH_OK;
}

/* ─── 工具：将 uint32_t passkey 填充为 4 字节 binary data ─── */
static void set_passkey(Meshtastic__AdminMessage *adm,
                        uint32_t passkey, uint8_t *pk_buf)
{
    if (passkey == 0) {
        adm->session_passkey.len  = 0;
        adm->session_passkey.data = NULL;
        return;
    }
    /* little-endian */
    pk_buf[0] = (uint8_t)(passkey & 0xFF);
    pk_buf[1] = (uint8_t)((passkey >> 8)  & 0xFF);
    pk_buf[2] = (uint8_t)((passkey >> 16) & 0xFF);
    pk_buf[3] = (uint8_t)((passkey >> 24) & 0xFF);
    adm->session_passkey.len  = 4;
    adm->session_passkey.data = pk_buf;
}

/* ─────────────────── 公开接口实现 ─────────────────── */

mesh_error_t admin_build_get_session_passkey(
    uint32_t dest_num, uint32_t from_num,
    uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    Meshtastic__AdminMessage adm = MESHTASTIC__ADMIN_MESSAGE__INIT;
    /* get_session_passkey_request 是一个 bool，设置为 true */
    adm.payload_variant_case =
        MESHTASTIC__ADMIN_MESSAGE__PAYLOAD_VARIANT_GET_CHANNEL_REQUEST;
    /* 注意：get_session_passkey_request 没有独立的 variant case 编号——
       它是字段 101，需要直接访问结构体成员（如果存在）。
       若编译器提示不存在，直接使用 bool get_session_passkey_request。
       这里先按标准方式构造，若固件版本中已有该字段再调整。*/

    /* 实际实现：使用 uint32 字段 get_channel_request = 0 来触发 passkey 响应，
       固件对 admin 包的任意请求都会返回当前 session_passkey。
       TODO: 若固件支持 get_session_passkey_request 字段，在此处理。*/
    adm.payload_variant_case =
        MESHTASTIC__ADMIN_MESSAGE__PAYLOAD_VARIANT_GET_CHANNEL_REQUEST;
    adm.get_channel_request = 0;   /* 请求第 0 信道，顺便获取 session_passkey */
    /* session_passkey 不需要填写（此包无 passkey）*/
    adm.session_passkey.len  = 0;
    adm.session_passkey.data = NULL;

    return pack_admin_to_radio(&adm, dest_num, from_num, buf, buf_len, out_len);
}

mesh_error_t admin_build_get_config(
    uint32_t dest_num, uint32_t from_num,
    uint32_t config_type, uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    uint8_t pk_buf[4];
    Meshtastic__AdminMessage adm = MESHTASTIC__ADMIN_MESSAGE__INIT;
    set_passkey(&adm, passkey, pk_buf);

    adm.payload_variant_case =
        MESHTASTIC__ADMIN_MESSAGE__PAYLOAD_VARIANT_GET_CONFIG_REQUEST;
    adm.get_config_request =
        (Meshtastic__AdminMessage__ConfigType)config_type;

    return pack_admin_to_radio(&adm, dest_num, from_num, buf, buf_len, out_len);
}

mesh_error_t admin_build_get_channel(
    uint32_t dest_num, uint32_t from_num,
    uint32_t channel_index, uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    uint8_t pk_buf[4];
    Meshtastic__AdminMessage adm = MESHTASTIC__ADMIN_MESSAGE__INIT;
    set_passkey(&adm, passkey, pk_buf);

    adm.payload_variant_case =
        MESHTASTIC__ADMIN_MESSAGE__PAYLOAD_VARIANT_GET_CHANNEL_REQUEST;
    adm.get_channel_request = channel_index;

    return pack_admin_to_radio(&adm, dest_num, from_num, buf, buf_len, out_len);
}

mesh_error_t admin_build_reboot(
    uint32_t dest_num, uint32_t from_num,
    int32_t delay_s, uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    uint8_t pk_buf[4];
    Meshtastic__AdminMessage adm = MESHTASTIC__ADMIN_MESSAGE__INIT;
    set_passkey(&adm, passkey, pk_buf);

    adm.payload_variant_case =
        MESHTASTIC__ADMIN_MESSAGE__PAYLOAD_VARIANT_REBOOT_SECONDS;
    adm.reboot_seconds = (delay_s > 0) ? delay_s : 5;

    return pack_admin_to_radio(&adm, dest_num, from_num, buf, buf_len, out_len);
}

mesh_error_t admin_build_factory_reset_config(
    uint32_t dest_num, uint32_t from_num,
    uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!buf || !out_len) return MESH_ERROR_INVALID_PARAM;

    uint8_t pk_buf[4];
    Meshtastic__AdminMessage adm = MESHTASTIC__ADMIN_MESSAGE__INIT;
    set_passkey(&adm, passkey, pk_buf);

    adm.payload_variant_case =
        MESHTASTIC__ADMIN_MESSAGE__PAYLOAD_VARIANT_FACTORY_RESET_CONFIG;
    adm.factory_reset_config = 1;

    return pack_admin_to_radio(&adm, dest_num, from_num, buf, buf_len, out_len);
}
