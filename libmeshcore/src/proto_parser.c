/**
 * proto_parser.c - FromRadio protobuf-c 解析实现
 *
 * 使用 protobuf-c 库解析 Meshtastic FromRadio 消息
 * 参考：meshdebug/proto_parser.py parse_from_radio()
 */
#include "proto_parser.h"
#include "meshtastic/mesh.pb-c.h"
#include "meshtastic/admin.pb-c.h"
#include "meshtastic/telemetry.pb-c.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 节点号 → "!xxxxxxxx" */
static void num_to_id(uint32_t num, char *buf) {
    buf[0] = '!';
    for (int i = 8; i >= 1; i--) {
        buf[i] = "0123456789abcdef"[num & 0xF];
        num >>= 4;
    }
    buf[9] = '\0';
}

/* 解析 NodeInfo → mesh_node_t */
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
        node->battery_level = ni->device_metrics->battery_level;
        node->voltage       = ni->device_metrics->voltage;
        node->uptime_seconds = ni->device_metrics->uptime_seconds;
    }
    node->snr  = ni->snr;
    node->last_heard_ms = mesh_time_ms();
    node->is_valid = true;
}

/* 解析 MeshPacket → mesh_packet_t */
static void parse_mesh_packet(const Meshtastic__MeshPacket *mp, mesh_packet_t *pkt) {
    memset(pkt, 0, sizeof(mesh_packet_t));
    pkt->packet_id = mp->id;
    pkt->from_num  = mp->from;
    pkt->to_num    = mp->to;
    num_to_id(mp->from, pkt->from_id);
    if (mp->to == MESH_BROADCAST_NUM)
        strncpy(pkt->to_id, MESH_BROADCAST_ID, sizeof(pkt->to_id) - 1);
    else
        num_to_id(mp->to, pkt->to_id);

    pkt->channel   = mp->channel;
    pkt->hop_limit = mp->hop_limit;
    pkt->rx_snr_x4 = (int8_t)(mp->rx_snr * 4);
    pkt->rx_rssi   = mp->rx_rssi;
    pkt->want_ack  = mp->want_ack;
    pkt->via_mqtt  = mp->via_mqtt;

    if (mp->payload_variant_case == MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_DECODED &&
        mp->decoded) {
        pkt->decoded = true;
        pkt->portnum = (mesh_portnum_t)mp->decoded->portnum;

        /* 文本消息 */
        if (pkt->portnum == PORTNUM_TEXT_MESSAGE) {
            size_t tlen = mp->decoded->payload.len;
            if (tlen >= sizeof(pkt->text)) tlen = sizeof(pkt->text) - 1;
            memcpy(pkt->text, mp->decoded->payload.data, tlen);
            pkt->text[tlen] = '\0';
            pkt->is_text = true;
        } else {
            /* 保存原始 payload */
            size_t plen = mp->decoded->payload.len;
            if (plen > sizeof(pkt->payload)) plen = sizeof(pkt->payload);
            memcpy(pkt->payload, mp->decoded->payload.data, plen);
            pkt->payload_len = (uint16_t)plen;
        }
    }
}

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

/* ─── want_config_id ─── */
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

/* ─── heartbeat ─── */
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

/* ─── text packet ─── */
mesh_error_t proto_build_text_packet(uint32_t from_num, uint32_t to_num,
                                      const char *text, uint8_t channel,
                                      uint32_t packet_id,
                                      uint8_t *buf, size_t buf_len,
                                      size_t *out_len)
{
    static uint32_t s_id = 1;
    if (packet_id == 0) packet_id = s_id++;

    ProtobufCBinaryData payload_data = {
        .len  = strlen(text),
        .data = (uint8_t *)text
    };

    Meshtastic__Data data = MESHTASTIC__DATA__INIT;
    data.portnum = MESHTASTIC__PORT_NUM__TEXT_MESSAGE_APP;
    data.payload = payload_data;

    Meshtastic__MeshPacket mp = MESHTASTIC__MESH_PACKET__INIT;
    mp.from    = from_num;
    mp.to      = to_num;
    mp.id      = packet_id;
    mp.channel = channel;
    mp.want_ack = true;
    mp.payload_variant_case = MESHTASTIC__MESH_PACKET__PAYLOAD_VARIANT_DECODED;
    mp.decoded = &data;

    Meshtastic__ToRadio tr = MESHTASTIC__TO_RADIO__INIT;
    tr.payload_variant_case = MESHTASTIC__TO_RADIO__PAYLOAD_VARIANT_PACKET;
    tr.packet = &mp;

    size_t needed = meshtastic__to_radio__get_packed_size(&tr);
    if (needed > buf_len) return MESH_ERROR_BUFFER_FULL;
    *out_len = meshtastic__to_radio__pack(&tr, buf);
    return MESH_OK;
}
