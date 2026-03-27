/**
 * node_manager.c - 双表节点管理实现
 *
 * 网关节点表（单条）+ 远程节点表（最多 256 条内存数组）
 */
#include "node_manager.h"
#include <string.h>
#include <stdio.h>

/* ─── 全局状态 ─── */

static struct {
    mesh_gateway_node_t    gateway;
    mesh_node_t            nodes[MESH_MAX_NODES];
    int                    count;
    node_update_callback_t callback;
    void                  *cb_userdata;
} g_nm;

/* ─── 初始化 / 销毁 ─── */

int node_manager_init(void) {
    memset(&g_nm, 0, sizeof(g_nm));
    return 0;
}

void node_manager_close(void) {
    memset(&g_nm, 0, sizeof(g_nm));
}

/* ─── 网关节点管理 ─── */

mesh_gateway_node_t *node_manager_gateway(void) {
    return &g_nm.gateway;
}

void node_manager_gateway_set_num(uint32_t node_num) {
    g_nm.gateway.node_num = node_num;
    mesh_node_num_to_id(node_num, g_nm.gateway.node_id,
                        sizeof(g_nm.gateway.node_id));
}

void node_manager_gateway_set_connected(bool connected,
                                         const char *device,
                                         uint32_t baudrate) {
    g_nm.gateway.serial_connected = connected;
    if (connected) {
        g_nm.gateway.connect_time_ms = mesh_time_ms();
        if (device)
            strncpy(g_nm.gateway.serial_device, device,
                    sizeof(g_nm.gateway.serial_device) - 1);
        g_nm.gateway.serial_baudrate = baudrate;
        g_nm.gateway.config_complete = false;  /* 重置握手状态 */
    }
}

void node_manager_gateway_set_config_complete(bool done) {
    g_nm.gateway.config_complete = done;
}

/* ─── 内部工具 ─── */

static int find_idx(const char *node_id) {
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (g_nm.nodes[i].is_valid &&
            strcmp(g_nm.nodes[i].node_id, node_id) == 0)
            return i;
    }
    return -1;
}

static int find_by_num(uint32_t node_num) {
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (g_nm.nodes[i].is_valid &&
            g_nm.nodes[i].node_num == node_num)
            return i;
    }
    return -1;
}

static int free_slot(void) {
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (!g_nm.nodes[i].is_valid) return i;
    }
    return -1;
}

/* ─── 远程节点全量更新 ─── */

mesh_error_t node_manager_update(const mesh_node_t *node) {
    if (!node || !node->node_id[0]) return MESH_ERROR_INVALID_PARAM;

    int  idx    = find_idx(node->node_id);
    bool is_new = (idx < 0);

    if (is_new) {
        idx = free_slot();
        if (idx < 0) return MESH_ERROR_NODE_DB_FULL;
        g_nm.count++;
    }

    /* 保留 first_seen_ms 和私有配置字段（全量更新会覆盖，但握手期 NODE_INFO
     * 不含私有配置，所以先保存再恢复）*/
    uint64_t first_seen = is_new ? mesh_time_ms() : g_nm.nodes[idx].first_seen_ms;

    /* 私有配置字段保护：握手期的 NODE_INFO 不应清除已有私有配置 */
    bool     saved_enrolled         = g_nm.nodes[idx].is_enrolled;
    uint8_t  saved_company_key[MESH_PUBKEY_LEN];
    memcpy(saved_company_key, g_nm.nodes[idx].company_public_key, MESH_PUBKEY_LEN);
    uint64_t saved_admin_ts         = g_nm.nodes[idx].last_admin_change_ts;
    bool     saved_admin_key_set    = g_nm.nodes[idx].is_admin_key_set;
    uint32_t saved_cfg_version      = g_nm.nodes[idx].private_config_version;
    char     saved_device_name[MESH_DEVICE_NAME_LEN];
    strncpy(saved_device_name, g_nm.nodes[idx].device_name,
            sizeof(saved_device_name) - 1);

    memcpy(&g_nm.nodes[idx], node, sizeof(mesh_node_t));
    g_nm.nodes[idx].first_seen_ms      = first_seen;
    g_nm.nodes[idx].last_heard_ms      = mesh_time_ms();
    g_nm.nodes[idx].is_valid           = true;

    /* 恢复私有配置字段（仅当传入值为零/空时恢复）*/
    if (!node->is_enrolled && saved_enrolled) {
        g_nm.nodes[idx].is_enrolled     = saved_enrolled;
        memcpy(g_nm.nodes[idx].company_public_key, saved_company_key, MESH_PUBKEY_LEN);
        g_nm.nodes[idx].last_admin_change_ts = saved_admin_ts;
    }
    if (!node->is_admin_key_set && saved_admin_key_set)
        g_nm.nodes[idx].is_admin_key_set = saved_admin_key_set;
    if (!node->private_config_version && saved_cfg_version)
        g_nm.nodes[idx].private_config_version = saved_cfg_version;
    if (!node->device_name[0] && saved_device_name[0])
        strncpy(g_nm.nodes[idx].device_name, saved_device_name,
                sizeof(g_nm.nodes[idx].device_name) - 1);

    if (g_nm.callback)
        g_nm.callback(&g_nm.nodes[idx], is_new, g_nm.cb_userdata);

    return MESH_OK;
}

/* ─── 字段级更新：私有配置 ─── */

mesh_error_t node_manager_set_enrolled(const char *node_id,
                                        bool is_enrolled,
                                        const uint8_t *company_pub_key,
                                        uint64_t change_ts)
{
    int idx = find_idx(node_id);
    if (idx < 0) return MESH_ERROR_NODE_NOT_FOUND;

    g_nm.nodes[idx].is_enrolled         = is_enrolled;
    g_nm.nodes[idx].last_admin_change_ts = change_ts;
    if (company_pub_key)
        memcpy(g_nm.nodes[idx].company_public_key, company_pub_key, MESH_PUBKEY_LEN);
    g_nm.nodes[idx].last_heard_ms = mesh_time_ms();

    if (g_nm.callback)
        g_nm.callback(&g_nm.nodes[idx], false, g_nm.cb_userdata);
    return MESH_OK;
}

mesh_error_t node_manager_set_private_config(const char *node_id,
                                              bool is_admin_key_set,
                                              uint32_t version,
                                              const char *device_name)
{
    int idx = find_idx(node_id);
    if (idx < 0) return MESH_ERROR_NODE_NOT_FOUND;

    g_nm.nodes[idx].is_admin_key_set      = is_admin_key_set;
    g_nm.nodes[idx].private_config_version = version;
    if (device_name)
        strncpy(g_nm.nodes[idx].device_name, device_name,
                sizeof(g_nm.nodes[idx].device_name) - 1);
    g_nm.nodes[idx].last_heard_ms = mesh_time_ms();

    if (g_nm.callback)
        g_nm.callback(&g_nm.nodes[idx], false, g_nm.cb_userdata);
    return MESH_OK;
}

/* ─── 从 PACKET 事件更新节点 ─── */

mesh_error_t node_manager_update_from_packet(const mesh_packet_t *pkt) {
    if (!pkt) return MESH_ERROR_INVALID_PARAM;

    /* 通过 from_num 找到节点（若不存在则创建基础条目）*/
    int idx = find_by_num(pkt->from_num);
    if (idx < 0) {
        /* 节点不在表中，创建基础条目 */
        idx = free_slot();
        if (idx < 0) return MESH_ERROR_NODE_DB_FULL;
        memset(&g_nm.nodes[idx], 0, sizeof(mesh_node_t));
        g_nm.nodes[idx].node_num = pkt->from_num;
        mesh_node_num_to_id(pkt->from_num, g_nm.nodes[idx].node_id,
                            sizeof(g_nm.nodes[idx].node_id));
        g_nm.nodes[idx].first_seen_ms = mesh_time_ms();
        g_nm.nodes[idx].is_valid      = true;
        g_nm.count++;
    }

    mesh_node_t *n = &g_nm.nodes[idx];

    /* 更新信号强度 */
    n->snr  = pkt->rx_snr_x4 / 4.0f;
    n->rssi = pkt->rx_rssi;
    if (pkt->hop_start > pkt->hop_limit)
        n->hops_away = pkt->hop_start - pkt->hop_limit;
    n->last_heard_ms = mesh_time_ms();

    /* 按 PortNum 更新对应字段 */
    switch (pkt->portnum) {
    case PORTNUM_POSITION:
        if (pkt->position.valid) {
            n->latitude      = pkt->position.latitude;
            n->longitude     = pkt->position.longitude;
            n->altitude      = pkt->position.altitude;
            n->position_time = pkt->position.timestamp;
        }
        break;

    case PORTNUM_NODEINFO:
        if (pkt->user.valid) {
            strncpy(n->long_name, pkt->user.long_name,
                    sizeof(n->long_name) - 1);
            strncpy(n->short_name, pkt->user.short_name,
                    sizeof(n->short_name) - 1);
            n->hw_model = pkt->user.hw_model;
            n->role     = pkt->user.role;
            if (pkt->user.has_public_key) {
                memcpy(n->public_key, pkt->user.public_key, MESH_PUBKEY_LEN);
                n->has_public_key = true;
            }
        }
        break;

    case PORTNUM_TELEMETRY:
        if (pkt->telemetry.has_device_metrics) {
            n->battery_level       = pkt->telemetry.battery_level;
            n->voltage             = pkt->telemetry.voltage;
            n->channel_utilization = pkt->telemetry.channel_utilization;
            n->air_util_tx         = pkt->telemetry.air_util_tx;
            n->uptime_seconds      = pkt->telemetry.uptime_seconds;
        }
        break;

    case PORTNUM_PRIVATE_CONFIG:
        /* private_payload 由 private_config_handler 处理后
         * 再调用 node_manager_set_enrolled / set_private_config */
        break;

    default:
        break;
    }

    if (g_nm.callback)
        g_nm.callback(n, false, g_nm.cb_userdata);

    return MESH_OK;
}

/* ─── 查询接口 ─── */

const mesh_node_t *node_manager_get(const char *node_id) {
    int idx = find_idx(node_id);
    return (idx >= 0) ? &g_nm.nodes[idx] : NULL;
}

const mesh_node_t *node_manager_get_by_num(uint32_t node_num) {
    int idx = find_by_num(node_num);
    return (idx >= 0) ? &g_nm.nodes[idx] : NULL;
}

int node_manager_get_all(mesh_node_t *out, int max_count) {
    int n = 0;
    for (int i = 0; i < MESH_MAX_NODES && n < max_count; i++) {
        if (g_nm.nodes[i].is_valid)
            memcpy(&out[n++], &g_nm.nodes[i], sizeof(mesh_node_t));
    }
    return n;
}

int node_manager_get_count(void) { return g_nm.count; }

void node_manager_expire(uint64_t expire_ms) {
    uint64_t now = mesh_time_ms();
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (g_nm.nodes[i].is_valid &&
            (now - g_nm.nodes[i].last_heard_ms) > expire_ms) {
            g_nm.nodes[i].is_valid = false;
            g_nm.count--;
        }
    }
}

void node_manager_set_callback(node_update_callback_t cb, void *userdata) {
    g_nm.callback    = cb;
    g_nm.cb_userdata = userdata;
}
