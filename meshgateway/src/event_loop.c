/**
 * event_loop.c - select() 跨平台事件循环实现
 *
 * 支持 serial=NULL（未连接状态），此时仅处理 TCP 命令
 */
#include "event_loop.h"
#include "web_server.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

/* ─── 串口帧处理：解析 → 更新节点表 → 广播事件 ─── */

/* 同时向 TCP 监控客户端和 WebSocket 客户端广播 */
static void broadcast_event(event_loop_t *loop, const char *json) {
    tcp_server_broadcast_event(loop->tcp, json);
    if (loop->web)
        web_server_broadcast(loop->web, json);
}

static void handle_frame(event_loop_t *loop, const uint8_t *payload, uint16_t len) {
    mesh_event_t ev;
    mesh_error_t err = proto_parse_from_radio(payload, len, &ev);
    if (err != MESH_OK) {
        LOG_WARN("event_loop", "proto_parse_from_radio error %d (len=%u)", err, len);
        return;
    }

    loop->gstate->rx_count++;
    heartbeat_activity();   /* 任意帧到达：重置心跳计时 */

    switch (ev.type) {

    /* ── 网关自身信息 ── */
    case MESH_EVENT_MY_INFO:
        loop->gstate->my_node_num = ev.data.my_node_num;
        loop->gstate->connected   = true;
        node_manager_gateway_set_num(ev.data.my_node_num);
        LOG_INFO("event_loop", "MyInfo: gateway node_num=0x%08X (%s)",
                 ev.data.my_node_num,
                 node_manager_gateway()->node_id);
        break;

    /* ── 远程节点信息（握手期批量推送）── */
    case MESH_EVENT_NODE_INFO: {
        bool is_new = (node_manager_get(ev.data.node.node_id) == NULL);
        node_manager_update(&ev.data.node);
        LOG_DEBUG("event_loop", "NodeInfo: %s \"%s\" battery=%u%%",
                  ev.data.node.node_id,
                  ev.data.node.long_name,
                  ev.data.node.battery_level);
        /* 广播节点信息事件 */
        char json[512];
        snprintf(json, sizeof(json),
            "{\"event\":\"%s\","
            "\"node_id\":\"%s\","
            "\"long_name\":\"%s\","
            "\"short_name\":\"%s\","
            "\"battery\":%u,"
            "\"snr\":%.1f,"
            "\"is_enrolled\":%s}",
            is_new ? "node_new" : "node_updated",
            ev.data.node.node_id,
            ev.data.node.long_name,
            ev.data.node.short_name,
            ev.data.node.battery_level,
            ev.data.node.snr,
            ev.data.node.is_enrolled ? "true" : "false");
        broadcast_event(loop, json);
        break;
    }

    /* ── 收到 MeshPacket ── */
    case MESH_EVENT_PACKET: {
        const mesh_packet_t *pkt = &ev.data.packet;

        /* 更新节点表（信号/位置/遥测/用户信息）*/
        node_manager_update_from_packet(pkt);

        /* 日志输出 */
        switch (pkt->portnum) {
        case PORTNUM_TEXT_MESSAGE:
            LOG_INFO("event_loop", "TEXT  %s → %s [ch%u]: %s",
                     pkt->from_id, pkt->to_id, pkt->channel, pkt->text);
            break;
        case PORTNUM_POSITION:
            LOG_DEBUG("event_loop", "POS   %s lat=%.5f lon=%.5f",
                      pkt->from_id,
                      pkt->position.latitude, pkt->position.longitude);
            break;
        case PORTNUM_TELEMETRY:
            LOG_DEBUG("event_loop", "TELEM %s bat=%u%% volt=%.2fV",
                      pkt->from_id,
                      pkt->telemetry.battery_level,
                      pkt->telemetry.voltage);
            break;
        case PORTNUM_NODEINFO:
            LOG_DEBUG("event_loop", "NINFO %s \"%s\"",
                      pkt->from_id, pkt->user.long_name);
            break;
        case PORTNUM_PRIVATE_CONFIG:
            LOG_DEBUG("event_loop", "PRIV_CFG %s → %s (%u bytes)",
                      pkt->from_id, pkt->to_id, pkt->private_payload_len);
            break;
        case PORTNUM_ADMIN:
            LOG_DEBUG("event_loop", "ADMIN %s → %s (%u bytes)",
                      pkt->from_id, pkt->to_id, pkt->admin_payload_len);
            break;
        default:
            LOG_DEBUG("event_loop", "PKT   portnum=%d %s → %s",
                      pkt->portnum, pkt->from_id, pkt->to_id);
            break;
        }

        /* 广播数据包事件给订阅客户端 */
        char json[896];
        int  n = snprintf(json, sizeof(json),
            "{\"event\":\"packet_received\","
            "\"portnum\":%d,"
            "\"from\":\"%s\","
            "\"to\":\"%s\","
            "\"channel\":%u,"
            "\"snr\":%.1f,"
            "\"rssi\":%d",
            pkt->portnum,
            pkt->from_id, pkt->to_id,
            pkt->channel,
            pkt->rx_snr_x4 / 4.0f,
            pkt->rx_rssi);

        /* 附加文本内容 */
        if (pkt->is_text && n > 0 && (size_t)n < sizeof(json) - 32) {
            n += snprintf(json + n, sizeof(json) - (size_t)n,
                          ",\"text\":\"%s\"", pkt->text);
        }
        if (n > 0 && (size_t)n < sizeof(json) - 2)
            json[n++] = '}', json[n] = '\0';

        broadcast_event(loop, json);
        break;
    }

    /* ── 握手期配置推送（CONFIG/CHANNEL）── */
    case MESH_EVENT_CONFIG:
        LOG_DEBUG("event_loop", "Config variant received");
        break;

    case MESH_EVENT_MODULE_CONFIG:
        LOG_DEBUG("event_loop", "ModuleConfig variant received");
        break;

    case MESH_EVENT_CHANNEL:
        LOG_DEBUG("event_loop", "Channel variant received");
        break;

    /* ── 握手完成 ── */
    case MESH_EVENT_CONFIG_COMPLETE:
        node_manager_gateway_set_config_complete(true);
        LOG_INFO("event_loop", "Config complete — node DB ready (%d nodes)",
                 node_manager_get_count());
        {
            char json[64];
            snprintf(json, sizeof(json), "{\"event\":\"config_complete\","
                     "\"node_count\":%d}", node_manager_get_count());
            tcp_server_broadcast_event(loop->tcp, json);  /* keep for compat */
            if (loop->web) web_server_broadcast(loop->web, json);
        }
        break;

    /* ── 设备重启 ── */
    case MESH_EVENT_REBOOTED:
        LOG_WARN("event_loop", "Device rebooted — resetting connection state");
        loop->gstate->connected = false;
        node_manager_gateway_set_config_complete(false);
        heartbeat_reset();
        broadcast_event(loop,
            "{\"event\":\"serial_disconnected\",\"reason\":\"device_rebooted\"}");
        break;

    /* ── 设备 proto 日志 ── */
    case MESH_EVENT_LOG:
        LOG_DEBUG("event_loop", "[device-log] %s", ev.data.log_text);
        break;

    default:
        break;
    }
}

/* ─── 主循环 ─── */

void event_loop_run(event_loop_t *loop) {
    LOG_INFO("event_loop", "Starting event loop");

    while (*loop->running) {
        fd_set rfds;
        FD_ZERO(&rfds);

        /* TCP server fds */
        sock_t maxfd = tcp_server_fill_fdset(loop->tcp, &rfds);

        /* 获取当前串口 fd（serial 可能在运行时通过命令动态设置）*/
        serial_port_t *sp     = loop->gstate->serial;
        int            serial_fd = sp ? serial_port_fd(sp) : -1;

#ifndef _WIN32
        if (serial_fd >= 0) {
            FD_SET(serial_fd, &rfds);
            if ((sock_t)serial_fd > maxfd) maxfd = (sock_t)serial_fd;
        }
#endif

        struct timeval tv = { 0, 100000 }; /* 100ms */

#ifdef _WIN32
        select((int)(maxfd + 1), &rfds, NULL, NULL, &tv);
        tcp_server_handle(loop->tcp, &rfds);

        if (sp) {
            uint8_t byte;
            while (serial_port_read(sp, &byte, 1, 0) == 1) {
                frame_result_t r = frame_parser_feed(loop->parser, byte);
                if (r == FRAME_RESULT_COMPLETE) {
                    uint16_t plen = 0;
                    const uint8_t *payload = frame_parser_get_payload(loop->parser, &plen);
                    handle_frame(loop, payload, plen);
                    frame_parser_reset(loop->parser);
                } else if (r == FRAME_RESULT_TEXT_BYTE) {
                    /* 设备调试日志（非 proto 数据）*/
                    size_t tlen = 0;
                    const char *txt = (const char *)frame_parser_get_text(loop->parser, &tlen);
                    if (txt && tlen > 0) {
                        LOG_DEBUG("serial", "[dbg] %.*s", (int)tlen, txt);
                        /* 广播 debug_log 事件给订阅客户端 */
                        char json[320];
                        snprintf(json, sizeof(json),
                                 "{\"event\":\"debug_log\",\"text\":\"%.*s\"}",
                                 (int)(tlen < 256 ? tlen : 256), txt);
                        tcp_server_broadcast_event(loop->tcp, json);
                        if (loop->web) web_server_broadcast(loop->web, json);
                    }
                }
            }
        }
#else
        int ret = select((int)(maxfd + 1), &rfds, NULL, NULL, &tv);
        if (ret < 0) break;

        tcp_server_handle(loop->tcp, &rfds);

        if (serial_fd >= 0 && FD_ISSET(serial_fd, &rfds)) {
            uint8_t byte;
            while (serial_port_read(sp, &byte, 1, 0) == 1) {
                frame_result_t r = frame_parser_feed(loop->parser, byte);
                if (r == FRAME_RESULT_COMPLETE) {
                    uint16_t plen = 0;
                    const uint8_t *payload = frame_parser_get_payload(loop->parser, &plen);
                    handle_frame(loop, payload, plen);
                    frame_parser_reset(loop->parser);
                } else if (r == FRAME_RESULT_TEXT_BYTE) {
                    /* 设备调试日志（非 proto 数据）*/
                    size_t tlen = 0;
                    const char *txt = (const char *)frame_parser_get_text(loop->parser, &tlen);
                    if (txt && tlen > 0) {
                        LOG_DEBUG("serial", "[dbg] %.*s", (int)tlen, txt);
                        char json[320];
                        snprintf(json, sizeof(json),
                                 "{\"event\":\"debug_log\",\"text\":\"%.*s\"}",
                                 (int)(tlen < 256 ? tlen : 256), txt);
                        tcp_server_broadcast_event(loop->tcp, json);
                        if (loop->web) web_server_broadcast(loop->web, json);
                    }
                }
            }
        }
#endif
        /* 每次循环驱动心跳状态机 */
        if (loop->gstate->connected)
            heartbeat_tick();

        /* 驱动 Mongoose web 服务器（非阻塞）*/
        web_server_poll(loop->web);
    }

    LOG_INFO("event_loop", "Event loop stopped");
}
