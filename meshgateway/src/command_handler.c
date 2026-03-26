/**
 * command_handler.c - JSON 命令路由实现
 *
 * 使用极简手写 JSON 解析（避免引入额外依赖），
 * 输出使用 snprintf 构造 JSON 字符串。
 */
#include "command_handler.h"
#include "log.h"
#include "../libmeshcore/include/node_manager.h"
#include "../libmeshcore/include/proto_parser.h"
#include "../libmeshcore/include/mesh_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── 极简 JSON 字段提取 ─── */
/* 提取字符串字段值（返回静态 buf，线程不安全，足够单线程命令处理） */
static const char *json_get_str(const char *json, const char *key, char *out, size_t out_len) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == ' ') p++;
    if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i < out_len - 1) out[i++] = *p++;
        out[i] = '\0';
        return out;
    }
    return NULL;
}

static int json_get_int(const char *json, const char *key, int def) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return def;
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == ' ') p++;
    if (*p >= '0' && *p <= '9') return atoi(p);
    return def;
}

/* ─── 命令处理函数 ─── */

static char *cmd_get_status(gateway_state_t *gs) {
    time_t now = time(NULL);
    long uptime = (long)(now - gs->start_time);
    int node_count = node_manager_get_count();
    char node_id[MESH_NODE_ID_LEN] = "unknown";
    if (gs->my_node_num) mesh_node_num_to_id(gs->my_node_num, node_id, sizeof(node_id));

    char *resp = (char *)malloc(512);
    snprintf(resp, 512,
        "{\"status\":\"ok\","
        "\"connected\":%s,"
        "\"my_node_id\":\"%s\","
        "\"node_count\":%d,"
        "\"rx_count\":%llu,"
        "\"uptime_s\":%ld}",
        gs->connected ? "true" : "false",
        node_id,
        node_count,
        (unsigned long long)gs->rx_count,
        uptime);
    return resp;
}

static char *cmd_get_nodes(void) {
    mesh_node_t nodes[MESH_MAX_NODES];
    int count = node_manager_get_all(nodes, MESH_MAX_NODES);

    /* 估算缓冲区大小：每个节点约 300 字节 */
    size_t bufsz = 64 + count * 320;
    char *resp = (char *)malloc(bufsz);
    if (!resp) return NULL;

    char *p = resp;
    size_t rem = bufsz;
    int n;

    n = snprintf(p, rem, "{\"status\":\"ok\",\"count\":%d,\"nodes\":[", count);
    p += n; rem -= (size_t)n;

    for (int i = 0; i < count; i++) {
        const mesh_node_t *nd = &nodes[i];
        n = snprintf(p, rem,
            "%s{\"node_id\":\"%s\","
            "\"node_num\":%u,"
            "\"long_name\":\"%s\","
            "\"short_name\":\"%s\","
            "\"snr\":%.1f,"
            "\"battery\":%u,"
            "\"last_heard\":%llu}",
            i > 0 ? "," : "",
            nd->node_id,
            nd->node_num,
            nd->long_name,
            nd->short_name,
            nd->snr,
            nd->battery_level,
            (unsigned long long)nd->last_heard_ms);
        if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
    }

    n = snprintf(p, rem, "]}");
    if (n > 0) { p += n; }
    return resp;
}

static char *cmd_send_text(const char *json, gateway_state_t *gs) {
    char to_id[MESH_NODE_ID_LEN] = "";
    char text[256] = "";

    if (!json_get_str(json, "to",   to_id, sizeof(to_id)))
        return strdup("{\"status\":\"error\",\"error\":\"missing 'to' field\"}");
    if (!json_get_str(json, "text", text,  sizeof(text)))
        return strdup("{\"status\":\"error\",\"error\":\"missing 'text' field\"}");

    int channel = json_get_int(json, "channel", 0);

    /* 解析目标节点号 */
    uint32_t to_num;
    if (strcmp(to_id, "^all") == 0 || strcmp(to_id, MESH_BROADCAST_ID) == 0) {
        to_num = MESH_BROADCAST_NUM;
    } else {
        to_num = mesh_node_id_to_num(to_id);
        if (to_num == 0)
            return strdup("{\"status\":\"error\",\"error\":\"invalid 'to' node id\"}");
    }

    if (!gs->connected || !gs->send_raw)
        return strdup("{\"status\":\"error\",\"error\":\"not connected\"}");

    uint8_t buf[512];
    size_t out_len = 0;
    mesh_error_t err = proto_build_text_packet(
        gs->my_node_num, to_num, text, (uint8_t)channel, 0,
        buf, sizeof(buf), &out_len);

    if (err != MESH_OK)
        return strdup("{\"status\":\"error\",\"error\":\"proto_build failed\"}");

    /* 构造串口帧：0x94 0xC3 [len_h][len_l] [payload] */
    uint8_t frame[520];
    frame[0] = 0x94; frame[1] = 0xC3;
    frame[2] = (uint8_t)(out_len >> 8);
    frame[3] = (uint8_t)(out_len & 0xFF);
    memcpy(frame + 4, buf, out_len);

    gs->send_raw(frame, 4 + out_len, gs->send_ctx);

    char *resp = (char *)malloc(128);
    snprintf(resp, 128,
        "{\"status\":\"ok\",\"to\":\"%s\",\"text\":\"%s\"}", to_id, text);
    return resp;
}

/* ─── 主入口 ─── */

char *command_handle(sock_t client_fd, const char *json, void *userdata) {
    (void)client_fd;
    gateway_state_t *gs = (gateway_state_t *)userdata;

    char cmd[64] = "";
    json_get_str(json, "cmd", cmd, sizeof(cmd));
    LOG_DEBUG("cmd_handler", "cmd=%s json=%s", cmd, json);

    if (strcmp(cmd, "get_status") == 0)
        return cmd_get_status(gs);

    if (strcmp(cmd, "get_nodes") == 0)
        return cmd_get_nodes();

    if (strcmp(cmd, "send_text") == 0)
        return cmd_send_text(json, gs);

    if (strcmp(cmd, "monitor_start") == 0)
        return strdup("{\"status\":\"ok\",\"monitor\":true}");

    if (strcmp(cmd, "monitor_stop") == 0)
        return strdup("{\"status\":\"ok\",\"monitor\":false}");

    char *resp = (char *)malloc(128);
    snprintf(resp, 128, "{\"status\":\"error\",\"error\":\"unknown cmd: %s\"}", cmd);
    return resp;
}
