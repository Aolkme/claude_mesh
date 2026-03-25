#include "heartbeat.h"
#include "mesh_types.h"
#include <string.h>

static struct {
    uint64_t          interval_ms;
    uint64_t          last_activity_ms;
    heartbeat_send_fn send_fn;
    void             *userdata;
} g_hb;

void heartbeat_init(uint64_t interval_ms, heartbeat_send_fn send_fn, void *userdata) {
    g_hb.interval_ms      = interval_ms ? interval_ms : 600000ULL;
    g_hb.last_activity_ms = mesh_time_ms();
    g_hb.send_fn          = send_fn;
    g_hb.userdata         = userdata;
}

void heartbeat_activity(void) {
    g_hb.last_activity_ms = mesh_time_ms();
}

void heartbeat_tick(void) {
    if (!g_hb.send_fn) return;
    uint64_t now = mesh_time_ms();
    if ((now - g_hb.last_activity_ms) >= g_hb.interval_ms) {
        g_hb.send_fn(g_hb.userdata);
        g_hb.last_activity_ms = now;
    }
}

uint64_t heartbeat_remaining_ms(void) {
    uint64_t now     = mesh_time_ms();
    uint64_t elapsed = now - g_hb.last_activity_ms;
    return (elapsed >= g_hb.interval_ms) ? 0 : (g_hb.interval_ms - elapsed);
}
