/**
 * event_loop.h - 跨平台 select() 事件循环接口
 */
#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "config.h"
#include "tcp_server.h"
#include "../libmeshcore/include/serial_port.h"
#include "../libmeshcore/include/frame_parser.h"
#include "../libmeshcore/include/proto_parser.h"
#include "../libmeshcore/include/heartbeat.h"
#include "../libmeshcore/include/node_manager.h"
#include "command_handler.h"
#include <stdbool.h>

typedef struct {
    config_t       *cfg;
    serial_port_t  *serial;
    tcp_server_t   *tcp;
    frame_parser_t *parser;
    gateway_state_t *gstate;
    volatile bool  *running;  /* 主循环退出标志 */
} event_loop_t;

/**
 * 运行事件循环（阻塞，直到 *running = false）
 * select() 超时 100ms，定期触发 heartbeat_tick()
 */
void event_loop_run(event_loop_t *loop);

#endif /* EVENT_LOOP_H */
