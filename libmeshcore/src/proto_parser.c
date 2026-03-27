/**
 * proto_parser.c - FromRadio protobuf-c 解析实现
 *
 * 直接使用 protobufs_protobuf-c/ 下的编译文件，不重新实现协议逻辑。
 * 参考：doc/spec/02_解析器内核设计.md
 */
#include "proto_parser.h"
#include "meshtastic/mesh.pb-c.h"
#include "meshtastic/admin.pb-c.h"
#include "meshtastic/telemetry.pb-c.h"
#include "thingseye/privateconfig.pb-c.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ─── 工具函数 ─── */

static void num_to_id(uint32_t num, char *buf) {
    buf[0] = '!';
    for (int i = 8; i >= 1; i--) {
        buf[i] = "0123456789abcdef"[num & 0xF];
        num >>= 4;
    }
    buf[9] = '\0';
}

/* ─── 解析 NodeInfo → mesh_node_t ─── */

static void parse_node_info(const Meshtastic__NodeInfo *ni, mesh_node_t *node) {
    memset(node, 0, sizeof(mesh_node_t));
    node->node_num = ni->num;
    num_to_id(ni->num, node->node_id);

    if (ni->user) {
        if (ni->user->long_name)
            strncpy(node->long_name, ni->user->long_name, sizeof(node->long_name) - 1);
        if (ni->user->short_name)
            strncpy(node->short_name, ni->user->short_name, sizeof(node->short_name) - 1);
        node->hw_model = (uint8_t)ni->user->hw_model;
        node->role     = (uint8_t)ni->user->role;
        if (ni->user->public_key.len == MESH_PUBKEY_LEN) {
            memcpy(node->public_key, ni->user->public_key.data, MESH_PUBKEY_LEN);
            node->has_public_key = true;
        }
    }
    if (ni->position) {
        node->latitude      = ni->position->latitude_i  * 1e-7;
        node->longitude     = ni->position->longitude_i * 1e-7;
        node->altitude      = ni->position->altitude;
        node->position_time = ni->position->timestamp;
    }
    if (ni->device_metrics) {
        node->battery_level  = ni->device_metrics->battery_level;
        node->voltage        = ni->device_metrics->voltage;
        node->uptime_seconds = ni->device_metrics->uptime_seconds;
        node->channel_utilization = ni->device_metrics->channel_utilization;
        node->air_util_tx    = ni->device_metrics->air_util_tx;
    }
    node->snr           = ni->snr;
    node->last_heard_ms = mesh_time_ms();
    node->is_valid      = true;
}

/* ─── 按 PortNum 解析 MeshPacket decoded payload ─── */

static void parse_payload_by_portnum(const Meshtastic__Data *decoded,
                                      mesh_packet_t *pkt)
{
    const uint8_t *pdata = decoded->payload.data;
    size_t         plen  = decoded->payload.len;

    switch ((mesh_portnum_t)decoded->portnum) {

    /* ── TEXT_MESSAGE(1) ── */
    case PORTNUM_TEXT_MESSAGE: {
        size_t tlen = plen;
        if (tlen >= sizeof(pkt->text)) tlen = sizeof(pkt->text) - 1;
        memcpy(pkt->text, pdata, tlen);
        pkt->text[tlen] = '\0';
        pkt->is_text    = true;
        break;
    }

    /* ── POSITION(3) ── */
    case PORTNUM_POSITION: {
        Meshtastic__Position *pos =
            meshtastic__position__unpack(NULL, plen, pdata);
        if (pos) {
            pkt->position.latitude  = pos->latitude_i  * 1e-7;
            pkt->position.longitude = pos->longitude_i * 1e-7;
            pkt->position.altitude  = pos->altitude;
            pkt->position.timestamp = pos->timestamp;
            pkt->position.pdop      = pos->pdop;
            pkt->position.valid     = true;
            meshtastic__position__free_unpacked(pos, NULL);
        }
        /* 同时保存原始 payload */
        if (plen <= sizeof(pkt->payload)) {
            memcpy(pkt->payload, pdata, plen);
            pkt->payload_len = (uint16_t)plen;
        }
        break;
    }

    /* ── NODEINFO(4) ── */
    case PORTNUM_NODEINFO: {
        Meshtastic__User *u =
            meshtastic__user__unpack(NULL, plen, pdata);
        if (u) {
            if (u->long_name)
                strncpy(pkt->user.long_name, u->long_name,
                        sizeof(pkt->user.long_name) - 1);
            if (u->short_name)
                strncpy(pkt->user.short_name, u->short_name,
                        sizeof(pkt->user.short_name) - 1);
            pkt->user.hw_model = (uint8_t)u->hw_model;
            pkt->user.role     = (uint8_t)u->role;
            if (u->public_key.len == MESH_PUBKEY_LEN) {
                memcpy(pkt->user.public_key, u->public_key.data, MESH_PUBKEY_LEN);
                pkt->user.has_public_key = true;
            }
            pkt->user.valid = true;
            meshtastic__user__free_unpacked(u, NULL);
        }
        if (plen <= sizeof(pkt->payload)) {
            memcpy(pkt->payload, pdata, plen);
            pkt->payload_len = (uint16_t)plen;
        }
        break;
    }

    /* ── ROUTING(5) ── */
    case PORTNUM_ROUTING: {
        Meshtastic__Routing *r =
            meshtastic__routing__unpack(NULL, plen, pdata);
        if (r) {
            if (r->variant_case ==
                    MESHTASTIC__ROUTING__VARIANT_ERROR_REASON) {
                pkt->routing.error_reason = (uint8_t)r->error_reason;
            }
            pkt->routing.valid = true;
            meshtastic__routing__free_unpacked(r, NULL);
        }
        if (plen <= sizeof(pkt->payload)) {
            memcpy(pkt->payload, pdata, plen);
            pkt->payload_len = (uint16_t)plen;
        }
        break;
    }

    /* ── ADMIN(6) ── 保存原始二进制，供 admin_builder 层解析 */
    case PORTNUM_ADMIN: {
        size_t copy_len = plen;
        if (copy_len > sizeof(pkt->admin_payload))
            copy_len = sizeof(pkt->admin_payload);
        memcpy(pkt->admin_payload, pdata, copy_len);
        pkt->admin_payload_len = (uint16_t)copy_len;
        break;
    }

    /* ── TELEMETRY(67) ── */
    case PORTNUM_TELEMETRY: {
        Meshtastic__Telemetry *t =
            meshtastic__telemetry__unpack(NULL, plen, pdata);
        if (t) {
            if (t->variant_case ==
                    MESHTASTIC__TELEMETRY__VARIANT_DEVICE_METRICS &&
                t->device_metrics) {
                pkt->telemetry.battery_level  = t->device_metrics->battery_level;
                pkt->telemetry.voltage        = t->device_metrics->voltage;
                pkt->telemetry.channel_utilization =
                                               t->device_metrics->channel_utilization;
                pkt->telemetry.air_util_tx    = t->device_metrics->air_util_tx;
                pkt->telemetry.uptime_seconds = t->device_metrics->uptime_seconds;
                pkt->telemetry.has_device_metrics = true;
            }
            if (t->variant_case ==
                    MESHTASTIC__TELEMETRY__VARIANT_ENVIRONMENT_METRICS &&
                t->environment_metrics) {
                pkt->telemetry.temperature          = t->environment_metrics->temperature;
                pkt->telemetry.relative_humidity    = t->environment_metrics->relative_humidity;
                pkt->telemetry.barometric_pressure  = t->environment_metrics->barometric_pressure;
                pkt->telemetry.has_environment_metrics = true;
            }
            pkt->telemetry.valid = true;
            meshtastic__telemetry__free_unpacked(t, NULL);
        }
        if (plen <= sizeof(pkt->payload)) {
            memcpy(pkt->payload, pdata, plen);
            pkt->payload_len = (uint16_t)plen;
        }
        break;
    }

    /* ── PRIVATE_CONFIG(287) ── 保存原始二进制，供 private_config_handler 解析 */
    case PORTNUM_PRIVATE_CONFIG: {
        size_t copy_len = plen;
        if (copy_len > sizeof(pkt->private_payload))
            copy_len = sizeof(pkt->private_payload);
        memcpy(pkt->private_payload, pdata, copy_len);
        pkt->private_payload_len = (uint16_t)copy_len;
        break;
    }

    /* ── 其他 PortNum：保存原始 payload ── */
    default: {
        size_t copy_len = plen;
        if (copy_len > sizeof(pkt->payload))
            copy_len = sizeof(pkt->payload);
        memcpy(pkt->payload, pdata, copy_len);
        pkt->payload_len = (uint16_t)copy_len;
        break;
    }
    }
}

/* ─── 解析 MeshPacket → mesh_packet_t ─── */

static void parse_mesh_packet(const Meshtastic__MeshPacket *mp, mesh_packet_t *pkt) {
    memset(pkt, 0, sizeof(mesh_packet_t));

    pkt->packet_id  = mp->id;
    pkt->from_num   = mp->from;
    pkt->to_num     = mp->to;
    num_to_id(mp->from, pkt->from_id);

    if (mp->to == MESH_BROADCAST_NUM)
        strncpy(pkt->to_id, MESH_BROADCAST_ID, sizeof(pkt->to_id) - 1);
    else
        num_to_id(mp->to, pkt->to_id);

    pkt->channel      = mp->channel;
    pkt->hop_limit    = mp->hop_limit;
    pkt->hop_start    = mp->hop_start;
    pkt->rx_snr_x4    = (int8_t)(mp->rx_snr * 4.0f);
    pkt->rx_rssi      = mp->rx_rssi;
    pkt->want_ack     = mp->want_ack;
    pkt->via_mqtt     = mp->via_mqtt;
    pkt->pki_encrypted= mp->pki_encrypted;

    if (mp->payload_variant_case == MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_DECODED &&
        mp->decoded) {
        pkt->decoded = true;
        pkt->portnum = (mesh_portnum_t)mp->decoded->portnum;
        parse_payload_by_portnum(mp->decoded, pkt);
    } else if (mp->payload_variant_case == MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_ENCRYPTED) {
        /* 加密包：保存原始 encrypted 数据 */
        size_t copy_len = mp->encrypted.len;
        if (copy_len > sizeof(pkt->payload)) copy_len = sizeof(pkt->payload);
        memcpy(pkt->payload, mp->encrypted.data, copy_len);
        pkt->payload_len = (uint16_t)copy_len;
    }
}

/* ─────────────────── 公开解析接口 ─────────────────── */

mesh_error_t proto_parse_from_radio(const uint8_t *data, uint16_t len,
                                     mesh_event_t *event_out)
{
    if (!data || !event_out) return MESH_ERROR_INVALID_PARAM;

    Meshtastic__FromRadio *fr = meshtastic__from_radio__unpack(NULL, len, data);
    if (!fr) return MESH_ERROR_PROTO_DECODE;

    memset(event_out, 0, sizeof(mesh_event_t));
    event_out->received_ms = mesh_time_ms();

    switch (fr->payload_variant_case) {

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_PACKET:
        if (fr->packet) {
            event_out->type = MESH_EVENT_PACKET;
            parse_mesh_packet(fr->packet, &event_out->data.packet);
        }
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_MY_INFO:
        if (fr->my_info) {
            event_out->type = MESH_EVENT_MY_INFO;
            event_out->data.my_node_num = fr->my_info->my_node_num;
        }
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_NODE_INFO:
        if (fr->node_info) {
            event_out->type = MESH_EVENT_NODE_INFO;
            parse_node_info(fr->node_info, &event_out->data.node);
        }
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_CONFIG:
        /* CONFIG 握手期消息，type 标记即可，上层按需存储 */
        event_out->type = MESH_EVENT_CONFIG;
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_MODULE_CONFIG:
        event_out->type = MESH_EVENT_MODULE_CONFIG;
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_CHANNEL:
        event_out->type = MESH_EVENT_CHANNEL;
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_CONFIG_COMPLETE_ID:
        event_out->type = MESH_EVENT_CONFIG_COMPLETE;
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_REBOOTED:
        event_out->type = MESH_EVENT_REBOOTED;
        break;

    case MESHTASTIC__FROM_RADIO__PAYLOAD_VARIANT_LOG_RECORD:
        if (fr->log_record && fr->log_record->message) {
            event_out->type = MESH_EVENT_LOG;
            strncpy(event_out->data.log_text, fr->log_record->message,
                    sizeof(event_out->data.log_text) - 1);
        }
        break;

    default:
        event_out->type = MESH_EVENT_UNKNOWN;
        break;
    }

    meshtastic__from_radio__free_unpacked(fr, NULL);
    return MESH_OK;
}

/* ─────────────────── 基础构造接口 ─────────────────── */

mesh_error_t proto_build_want_config(uint32_t config_id,
                                      uint8_t *buf, size_t buf_len,
                                      size_t *out_len)
{
    Meshtastic__ToRadio tr = MESHTASTIC__TO_RADIO__INIT;
    tr.payload_variant_case = MESHTASTIC__TO_RADIO__PAYLOAD_VARIANT_WANT_CONFIG_ID;
    tr.want_config_id = config_id;

    size_t needed = meshtastic__to_radio__get_packed_size(&tr);
    if (needed > buf_len) return MESH_ERROR_BUFFER_FULL;
    *out_len = meshtastic__to_radio__pack(&tr, buf);
    return MESH_OK;
}

mesh_error_t proto_build_heartbeat(uint8_t *buf, size_t buf_len, size_t *out_len) {
    Meshtastic__ToRadio tr = MESHTASTIC__TO_RADIO__INIT;
    Meshtastic__Heartbeat hb = MESHTASTIC__HEARTBEAT__INIT;
    tr.payload_variant_case = MESHTASTIC__TO_RADIO__PAYLOAD_VARIANT_HEARTBEAT;
    tr.heartbeat = &hb;

    size_t needed = meshtastic__to_radio__get_packed_size(&tr);
    if (needed > buf_len) return MESH_ERROR_BUFFER_FULL;
    *out_len = meshtastic__to_radio__pack(&tr, buf);
    return MESH_OK;
}

mesh_error_t proto_build_text_packet(uint32_t from_num, uint32_t to_num,
                                      const char *text, uint8_t channel,
                                      uint8_t hop_limit, bool want_ack,
                                      uint32_t packet_id,
                                      uint8_t *buf, size_t buf_len,
                                      size_t *out_len)
{
    static uint32_t s_id = 1;
    if (!text) return MESH_ERROR_INVALID_PARAM;
    if (packet_id == 0) packet_id = s_id++;

    ProtobufCBinaryData payload_data = {
        .len  = strlen(text),
        .data = (uint8_t *)text
    };

    Meshtastic__Data data_msg = MESHTASTIC__DATA__INIT;
    data_msg.portnum = MESHTASTIC__PORT_NUM__TEXT_MESSAGE_APP;
    data_msg.payload = payload_data;
    data_msg.want_response = false;

    Meshtastic__MeshPacket mp = MESHTASTIC__MESH_PACKET__INIT;
    mp.from      = from_num;
    mp.to        = to_num;
    mp.id        = packet_id;
    mp.channel   = channel;
    mp.hop_limit = hop_limit ? hop_limit : 3;   /* 默认 3 跳 */
    mp.want_ack  = want_ack;
    mp.payload_variant_case = MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_DECODED;
    mp.decoded   = &data_msg;

    Meshtastic__ToRadio tr = MESHTASTIC__TO_RADIO__INIT;
    tr.payload_variant_case = MESHTASTIC__TO_RADIO__PAYLOAD_VARIANT_PACKET;
    tr.packet = &mp;

    size_t needed = meshtastic__to_radio__get_packed_size(&tr);
    if (needed > buf_len) return MESH_ERROR_BUFFER_FULL;
    *out_len = meshtastic__to_radio__pack(&tr, buf);
    return MESH_OK;
}
