/**
 * heartbeat.c - 心跳状态机实现
 *
 * 状态转换：
 *   IDLE     → (到达心跳间隔) → 发送心跳 → SENT
 *   SENT     → (收到任意帧)   → IDLE（重置计时）
 *   SENT     → (超时)         → RETRYING（retries < max_retries）
 *   RETRYING → (发送重试包)   → SENT
 *   RETRYING → (超时 & retries == max_retries) → FAILED
 *   FAILED   → (heartbeat_reset / heartbeat_activity) → IDLE
 */
#include "heartbeat.h"
#include "mesh_types.h"
#include <string.h>

/* 默认值 */
#define HB_DEFAULT_INTERVAL_MS  600000ULL   /* 10 分钟 */
#define HB_DEFAULT_TIMEOUT_MS    30000ULL   /* 30 秒   */
#define HB_DEFAULT_MAX_RETRIES       3

static struct {
    uint64_t          interval_ms;
    uint64_t          timeout_ms;
    int               max_retries;
    heartbeat_send_fn send_fn;
    heartbeat_fail_fn fail_fn;
    void             *userdata;

    heartbeat_state_t state;
    uint64_t          last_activity_ms;  /* 最后一次收到帧的时间 */
    uint64_t          sent_at_ms;        /* 最后一次发送心跳的时间 */
    int               retry_count;
} g_hb;

/* ─── 内部：执行发送 ─── */
static void do_send(void) {
    if (g_hb.send_fn) g_hb.send_fn(g_hb.userdata);
    g_hb.sent_at_ms = mesh_time_ms();
    g_hb.state      = HB_STATE_SENT;
}

/* ─── 公开接口 ─── */

void heartbeat_init(uint64_t interval_ms,
                    uint64_t timeout_ms,
                    int      max_retries,
                    heartbeat_send_fn send_fn,
                    heartbeat_fail_fn fail_fn,
                    void *userdata)
{
    memset(&g_hb, 0, sizeof(g_hb));
    g_hb.interval_ms    = interval_ms  ? interval_ms  : HB_DEFAULT_INTERVAL_MS;
    g_hb.timeout_ms     = timeout_ms   ? timeout_ms   : HB_DEFAULT_TIMEOUT_MS;
    g_hb.max_retries    = max_retries  ? max_retries  : HB_DEFAULT_MAX_RETRIES;
    g_hb.send_fn        = send_fn;
    g_hb.fail_fn        = fail_fn;
    g_hb.userdata       = userdata;
    g_hb.state          = HB_STATE_IDLE;
    g_hb.last_activity_ms = mesh_time_ms();
}

void heartbeat_activity(void) {
    g_hb.last_activity_ms = mesh_time_ms();
    g_hb.retry_count      = 0;

    /* 任意帧到达 → 回到 IDLE */
    if (g_hb.state != HB_STATE_FAILED)
        g_hb.state = HB_STATE_IDLE;
}

void heartbeat_tick(void) {
    if (!g_hb.send_fn) return;

    uint64_t now = mesh_time_ms();

    switch (g_hb.state) {

    case HB_STATE_IDLE:
        /* 判断是否到达发送时机 */
        if ((now - g_hb.last_activity_ms) >= g_hb.interval_ms) {
            g_hb.retry_count = 0;
            do_send();
        }
        break;

    case HB_STATE_SENT:
    case HB_STATE_WAITING:
        /* 等待超时 */
        if ((now - g_hb.sent_at_ms) >= g_hb.timeout_ms) {
            g_hb.retry_count++;
            if (g_hb.retry_count >= g_hb.max_retries) {
                /* 重试耗尽 → FAILED */
                g_hb.state = HB_STATE_FAILED;
                if (g_hb.fail_fn) g_hb.fail_fn(g_hb.userdata);
            } else {
                /* 发起重试 */
                g_hb.state = HB_STATE_RETRYING;
                do_send();
            }
        }
        break;

    case HB_STATE_RETRYING:
        /* 重试发送后进入 SENT 等待，do_send 已切换 state */
        break;

    case HB_STATE_FAILED:
        /* 等待外部调用 heartbeat_reset 或 heartbeat_activity */
        break;
    }
}

heartbeat_state_t heartbeat_get_state(void) {
    return g_hb.state;
}

uint64_t heartbeat_remaining_ms(void) {
    if (g_hb.state != HB_STATE_IDLE) return 0;
    uint64_t now     = mesh_time_ms();
    uint64_t elapsed = now - g_hb.last_activity_ms;
    return (elapsed >= g_hb.interval_ms) ? 0 : (g_hb.interval_ms - elapsed);
}

void heartbeat_reset(void) {
    g_hb.state          = HB_STATE_IDLE;
    g_hb.retry_count    = 0;
    g_hb.last_activity_ms = mesh_time_ms();
}
