/**
 * heartbeat.h - 心跳管理接口
 *
 * Meshtastic 固件 15 分钟无活动断连，需每 10 分钟发 want_config_id 心跳
 */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdint.h>
#include <stdbool.h>

/* 心跳发送回调，由调用者负责实际串口发送 */
typedef void (*heartbeat_send_fn)(void *userdata);

/**
 * 初始化心跳管理器
 * @param interval_ms  心跳间隔（毫秒，建议 600000 = 10分钟）
 * @param send_fn      发送回调
 * @param userdata     回调用户数据
 */
void heartbeat_init(uint64_t interval_ms, heartbeat_send_fn send_fn, void *userdata);

/** 通知已有活动（收到帧时调用，重置计时器）*/
void heartbeat_activity(void);

/** 主循环中定期调用（每秒一次），检查是否需要发送心跳 */
void heartbeat_tick(void);

/** 获取距下次心跳的剩余毫秒数 */
uint64_t heartbeat_remaining_ms(void);

#endif /* HEARTBEAT_H */
