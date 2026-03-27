/**
 * heartbeat.h - 心跳管理接口（含超时重试状态机）
 *
 * Meshtastic 固件 15 分钟无活动断连，需每 10 分钟发 want_config_id 心跳保活。
 * 状态机：IDLE → SENT → WAITING → RETRYING → FAILED
 */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdint.h>
#include <stdbool.h>

/* 心跳发送回调 */
typedef void (*heartbeat_send_fn)(void *userdata);

/* 连接失败回调（重试耗尽后触发，通知上层重连）*/
typedef void (*heartbeat_fail_fn)(void *userdata);

/* 心跳状态 */
typedef enum {
    HB_STATE_IDLE     = 0,  /* 空闲，等待下次心跳时机 */
    HB_STATE_SENT     = 1,  /* 心跳已发出，等待任意响应确认 */
    HB_STATE_WAITING  = 2,  /* 等待超时中 */
    HB_STATE_RETRYING = 3,  /* 重试中 */
    HB_STATE_FAILED   = 4,  /* 重试耗尽，连接失败 */
} heartbeat_state_t;

/**
 * 初始化心跳管理器
 *
 * @param interval_ms   心跳间隔（毫秒，推荐 600000 = 10分钟）
 * @param timeout_ms    单次超时（毫秒，推荐 30000 = 30秒）
 * @param max_retries   最大重试次数（推荐 3）
 * @param send_fn       心跳发送回调
 * @param fail_fn       连接失败回调（可为 NULL）
 * @param userdata      回调用户数据
 */
void heartbeat_init(uint64_t interval_ms,
                    uint64_t timeout_ms,
                    int      max_retries,
                    heartbeat_send_fn send_fn,
                    heartbeat_fail_fn fail_fn,
                    void *userdata);

/** 通知有活动（收到任意 FromRadio 帧时调用，重置计时器和重试计数）*/
void heartbeat_activity(void);

/** 主循环中定期调用（建议每 100~500ms 一次），驱动状态机 */
void heartbeat_tick(void);

/** 获取当前心跳状态 */
heartbeat_state_t heartbeat_get_state(void);

/** 获取距下次心跳的剩余毫秒数（IDLE 状态有效）*/
uint64_t heartbeat_remaining_ms(void);

/** 重置状态机（串口重连后调用）*/
void heartbeat_reset(void);

#endif /* HEARTBEAT_H */
