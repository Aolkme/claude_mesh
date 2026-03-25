/**
 * node_manager.c - 节点表实现（最大 256 节点，内存数组）
 */
#include "node_manager.h"
#include <string.h>

static struct {
    mesh_node_t            nodes[MESH_MAX_NODES];
    int                    count;
    node_update_callback_t callback;
    void                  *userdata;
} g_nm;

int node_manager_init(void) {
    memset(&g_nm, 0, sizeof(g_nm));
    return 0;
}

void node_manager_close(void) {
    memset(&g_nm, 0, sizeof(g_nm));
}

void node_manager_set_callback(node_update_callback_t cb, void *userdata) {
    g_nm.callback = cb;
    g_nm.userdata = userdata;
}

static int find_idx(const char *node_id) {
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (g_nm.nodes[i].is_valid &&
            strcmp(g_nm.nodes[i].node_id, node_id) == 0) return i;
    }
    return -1;
}

static int free_slot(void) {
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (!g_nm.nodes[i].is_valid) return i;
    }
    return -1;
}

mesh_error_t node_manager_update(const mesh_node_t *node) {
    if (!node || !node->node_id[0]) return MESH_ERROR_INVALID_PARAM;

    int idx = find_idx(node->node_id);
    bool is_new = (idx < 0);

    if (is_new) {
        idx = free_slot();
        if (idx < 0) return MESH_ERROR_NODE_DB_FULL;
        g_nm.count++;
    }

    uint64_t first_seen = is_new ? mesh_time_ms() : g_nm.nodes[idx].first_seen_ms;
    memcpy(&g_nm.nodes[idx], node, sizeof(mesh_node_t));
    g_nm.nodes[idx].first_seen_ms = first_seen;
    g_nm.nodes[idx].last_heard_ms = mesh_time_ms();
    g_nm.nodes[idx].is_valid = true;

    if (g_nm.callback)
        g_nm.callback(&g_nm.nodes[idx], is_new, g_nm.userdata);

    return MESH_OK;
}

const mesh_node_t *node_manager_get(const char *node_id) {
    int idx = find_idx(node_id);
    return (idx >= 0) ? &g_nm.nodes[idx] : NULL;
}

const mesh_node_t *node_manager_get_by_num(uint32_t node_num) {
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (g_nm.nodes[i].is_valid && g_nm.nodes[i].node_num == node_num)
            return &g_nm.nodes[i];
    }
    return NULL;
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
