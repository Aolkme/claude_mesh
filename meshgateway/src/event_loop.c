/**
 * event_loop.c - select() 跨平台事件循环实现
 */
#include "event_loop.h"
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

/* 串口帧完成后处理：解析 protobuf → 更新节点表 → 广播事件 */
static void handle_frame(event_loop_t *loop, const uint8_t *payload, uint16_t len) {
    mesh_event_t ev;
    mesh_error_t err = proto_parse_from_radio(payload, len, &ev);
    if (err != MESH_OK) {
        LOG_WARN("event_loop", "proto_parse_from_radio error %d", err);
        return;
    }

    loop->gstate->rx_count++;
    heartbeat_activity();

    switch (ev.type) {
    case MESH_EVENT_MY_INFO:
        loop->gstate->my_node_num = ev.data.my_node_num;
        loop->gstate->connected   = true;
        LOG_INFO("event_loop", "MyInfo: node_num=0x%08X", ev.data.my_node_num);
        break;

    case MESH_EVENT_NODE_INFO: {
        node_manager_update(&ev.data.node);
        LOG_DEBUG("event_loop", "NodeInfo: %s (%s)",
                  ev.data.node.node_id, ev.data.node.long_name);
        /* 广播节点更新事件 */
        char json[512];
        snprintf(json, sizeof(json),
            "{\"event\":\"node_update\","
            "\"node_id\":\"%s\","
            "\"long_name\":\"%s\","
            "\"short_name\":\"%s\","
            "\"snr\":%.1f}",
            ev.data.node.node_id,
            ev.data.node.long_name,
            ev.data.node.short_name,
            ev.data.node.snr);
        tcp_server_broadcast_event(loop->tcp, json);
        break;
    }

    case MESH_EVENT_PACKET: {
        const mesh_packet_t *pkt = &ev.data.packet;
        if (pkt->is_text) {
            LOG_INFO("event_loop", "TEXT %s → %s: %s",
                     pkt->from_id, pkt->to_id, pkt->text);
        } else {
            LOG_DEBUG("event_loop", "PKT portnum=%d from=%s",
                      pkt->portnum, pkt->from_id);
        }
        /* 广播数据包事件 */
        char json[768];
        int n = snprintf(json, sizeof(json),
            "{\"event\":\"packet\","
            "\"from\":\"%s\","
            "\"to\":\"%s\","
            "\"portnum\":%d",
            pkt->from_id, pkt->to_id, pkt->portnum);
        if (pkt->is_text && n > 0 && (size_t)n < sizeof(json) - 32) {
            snprintf(json + n, sizeof(json) - (size_t)n,
                     ",\"text\":\"%s\"}", pkt->text);
        } else {
            strncat(json, "}", sizeof(json) - strlen(json) - 1);
        }
        tcp_server_broadcast_event(loop->tcp, json);
        break;
    }

    case MESH_EVENT_CONFIG_COMPLETE:
        LOG_INFO("event_loop", "Config complete, node DB ready");
        break;

    case MESH_EVENT_REBOOTED:
        LOG_WARN("event_loop", "Device rebooted");
        loop->gstate->connected = false;
        break;

    case MESH_EVENT_LOG:
        LOG_DEBUG("event_loop", "[device] %s", ev.data.log_text);
        break;

    default:
        break;
    }
}

void event_loop_run(event_loop_t *loop) {
    LOG_INFO("event_loop", "Starting event loop");

    int serial_fd = serial_port_fd(loop->serial); /* 获取底层 fd（POSIX）或 -1（Windows）*/

    while (*loop->running) {
        fd_set rfds;
        FD_ZERO(&rfds);

        /* TCP server fds */
        sock_t maxfd = tcp_server_fill_fdset(loop->tcp, &rfds);

#ifndef _WIN32
        /* POSIX: 串口 fd 加入 select */
        if (serial_fd >= 0) {
            FD_SET(serial_fd, &rfds);
            if ((sock_t)serial_fd > maxfd) maxfd = (sock_t)serial_fd;
        }
#endif

        struct timeval tv = { 0, 100000 }; /* 100ms */

#ifdef _WIN32
        /* Windows: select 仅用于 TCP socket；串口通过超时读取 */
        select((int)(maxfd + 1), &rfds, NULL, NULL, &tv);
        tcp_server_handle(loop->tcp, &rfds);

        /* 非阻塞串口轮询（ReadFile with 100ms timeout in serial_port.c） */
        uint8_t byte;
        while (serial_port_read(loop->serial, &byte, 1, 0) == 1) {
            frame_result_t r = frame_parser_feed(loop->parser, byte);
            if (r == FRAME_RESULT_COMPLETE) {
                uint16_t plen = 0;
                const uint8_t *payload = frame_parser_get_payload(loop->parser, &plen);
                handle_frame(loop, payload, plen);
                frame_parser_reset(loop->parser);
            }
        }
#else
        int ret = select((int)(maxfd + 1), &rfds, NULL, NULL, &tv);
        if (ret < 0) break;

        tcp_server_handle(loop->tcp, &rfds);

        if (serial_fd >= 0 && FD_ISSET(serial_fd, &rfds)) {
            uint8_t byte;
            while (serial_port_read(loop->serial, &byte, 1, 0) == 1) {
                frame_result_t r = frame_parser_feed(loop->parser, byte);
                if (r == FRAME_RESULT_COMPLETE) {
                    uint16_t plen = 0;
                    const uint8_t *payload = frame_parser_get_payload(loop->parser, &plen);
                    handle_frame(loop, payload, plen);
                    frame_parser_reset(loop->parser);
                }
            }
        }
#endif
        heartbeat_tick();
    }

    LOG_INFO("event_loop", "Event loop stopped");
}
