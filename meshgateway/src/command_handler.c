/**
 * command_handler.c - JSON 命令路由实现
 *
 * 使用 cJSON 解析请求和构造响应，覆盖所有核心命令。
 * 帧头编码统一使用 frame_builder_encode()。
 */
#include "command_handler.h"
#include "config.h"
#include "log.h"
#include "../libmeshcore/include/node_manager.h"
#include "../libmeshcore/include/proto_parser.h"
#include "../libmeshcore/include/proto_builder.h"
#include "../libmeshcore/include/admin_builder.h"
#include "../libmeshcore/include/frame_builder.h"
#include "../libmeshcore/include/serial_port.h"
#include "../libmeshcore/include/mesh_types.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── 工具：序列化 cJSON 对象为字符串（调用者负责 free）─── */
static char *json_to_str(cJSON *obj) {
    char *s = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return s;
}

/* ─── 工具：错误响应 ─── */
static char *err_resp(const char *msg) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "error");
    cJSON_AddStringToObject(obj, "error", msg);
    return json_to_str(obj);
}

/* ─── 工具：向串口发送 proto buf（含 frame_builder 帧封装）─── */
static bool send_proto(gateway_state_t *gs, const uint8_t *proto_buf, size_t proto_len) {
    if (!gs->serial) return false;

    uint8_t frame[4 + 512];
    size_t  frame_len = 0;
    if (frame_builder_encode(proto_buf, proto_len,
                             frame, sizeof(frame), &frame_len) != MESH_OK)
        return false;

    return serial_port_write((serial_port_t *)gs->serial, frame, frame_len) >= 0;
}

/* ─────────────────── 命令实现 ─────────────────── */

static char *cmd_get_status(gateway_state_t *gs) {
    time_t now    = time(NULL);
    long   uptime = (long)(now - gs->start_time);

    const mesh_gateway_node_t *gw = node_manager_gateway();

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status",          "ok");
    cJSON_AddBoolToObject  (obj, "connected",        gs->connected);
    cJSON_AddStringToObject(obj, "my_node_id",       gw->node_id[0] ? gw->node_id : "unknown");
    cJSON_AddBoolToObject  (obj, "config_complete",  gw->config_complete);
    cJSON_AddNumberToObject(obj, "node_count",       node_manager_get_count());
    cJSON_AddNumberToObject(obj, "rx_count",         (double)gs->rx_count);
    cJSON_AddNumberToObject(obj, "uptime_s",         uptime);
    if (gw->serial_connected)
        cJSON_AddStringToObject(obj, "serial_device", gw->serial_device);
    return json_to_str(obj);
}

static char *cmd_get_gateway_info(void) {
    const mesh_gateway_node_t *gw = node_manager_gateway();

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status",           "ok");
    cJSON_AddStringToObject(obj, "node_id",          gw->node_id);
    cJSON_AddNumberToObject(obj, "node_num",         (double)gw->node_num);
    cJSON_AddStringToObject(obj, "long_name",        gw->long_name);
    cJSON_AddStringToObject(obj, "short_name",       gw->short_name);
    cJSON_AddBoolToObject  (obj, "serial_connected", gw->serial_connected);
    cJSON_AddBoolToObject  (obj, "config_complete",  gw->config_complete);
    cJSON_AddStringToObject(obj, "serial_device",    gw->serial_device);
    cJSON_AddBoolToObject  (obj, "has_keypair",      gw->has_keypair);
    return json_to_str(obj);
}

static char *cmd_get_nodes(void) {
    mesh_node_t nodes[MESH_MAX_NODES];
    int count = node_manager_get_all(nodes, MESH_MAX_NODES);

    cJSON *obj = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddStringToObject(obj, "status", "ok");
    cJSON_AddNumberToObject(obj, "count",  count);

    for (int i = 0; i < count; i++) {
        const mesh_node_t *nd = &nodes[i];
        cJSON *n = cJSON_CreateObject();
        cJSON_AddStringToObject(n, "node_id",    nd->node_id);
        cJSON_AddNumberToObject(n, "node_num",   (double)nd->node_num);
        cJSON_AddStringToObject(n, "long_name",  nd->long_name);
        cJSON_AddStringToObject(n, "short_name", nd->short_name);
        cJSON_AddNumberToObject(n, "battery",    nd->battery_level);
        cJSON_AddNumberToObject(n, "voltage",    nd->voltage);
        cJSON_AddNumberToObject(n, "snr",        nd->snr);
        cJSON_AddNumberToObject(n, "rssi",       nd->rssi);
        cJSON_AddNumberToObject(n, "hops_away",  nd->hops_away);
        cJSON_AddNumberToObject(n, "last_heard", (double)nd->last_heard_ms);
        cJSON_AddNumberToObject(n, "latitude",   nd->latitude);
        cJSON_AddNumberToObject(n, "longitude",  nd->longitude);
        cJSON_AddNumberToObject(n, "altitude",   nd->altitude);
        cJSON_AddBoolToObject  (n, "is_enrolled",nd->is_enrolled);
        cJSON_AddItemToArray(arr, n);
    }
    cJSON_AddItemToObject(obj, "nodes", arr);
    return json_to_str(obj);
}

static char *cmd_get_node(cJSON *req) {
    cJSON *jid = cJSON_GetObjectItemCaseSensitive(req, "node_id");
    if (!cJSON_IsString(jid))
        return err_resp("missing 'node_id'");

    const mesh_node_t *nd = node_manager_get(jid->valuestring);
    if (!nd)
        return err_resp("node not found");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status",                   "ok");
    cJSON_AddStringToObject(obj, "node_id",                  nd->node_id);
    cJSON_AddNumberToObject(obj, "node_num",                 (double)nd->node_num);
    cJSON_AddStringToObject(obj, "long_name",                nd->long_name);
    cJSON_AddStringToObject(obj, "short_name",               nd->short_name);
    cJSON_AddNumberToObject(obj, "hw_model",                 nd->hw_model);
    cJSON_AddNumberToObject(obj, "role",                     nd->role);
    cJSON_AddNumberToObject(obj, "battery",                  nd->battery_level);
    cJSON_AddNumberToObject(obj, "voltage",                  nd->voltage);
    cJSON_AddNumberToObject(obj, "snr",                      nd->snr);
    cJSON_AddNumberToObject(obj, "rssi",                     nd->rssi);
    cJSON_AddNumberToObject(obj, "hops_away",                nd->hops_away);
    cJSON_AddNumberToObject(obj, "uptime_s",                 (double)nd->uptime_seconds);
    cJSON_AddNumberToObject(obj, "latitude",                 nd->latitude);
    cJSON_AddNumberToObject(obj, "longitude",                nd->longitude);
    cJSON_AddNumberToObject(obj, "altitude",                 nd->altitude);
    cJSON_AddNumberToObject(obj, "last_heard",               (double)nd->last_heard_ms);
    cJSON_AddNumberToObject(obj, "first_seen",               (double)nd->first_seen_ms);
    cJSON_AddBoolToObject  (obj, "is_enrolled",              nd->is_enrolled);
    cJSON_AddBoolToObject  (obj, "is_admin_key_set",         nd->is_admin_key_set);
    cJSON_AddNumberToObject(obj, "private_config_version",   (double)nd->private_config_version);
    cJSON_AddStringToObject(obj, "device_name",              nd->device_name);
    return json_to_str(obj);
}

static char *cmd_connect_serial(cJSON *req, gateway_state_t *gs) {
    if (gs->serial)
        return err_resp("already connected, disconnect first");

    cJSON *jdev  = cJSON_GetObjectItemCaseSensitive(req, "device");
    cJSON *jbaud = cJSON_GetObjectItemCaseSensitive(req, "baudrate");

    if (!cJSON_IsString(jdev))
        return err_resp("missing 'device'");

    const char *device = jdev->valuestring;
    uint32_t baud = cJSON_IsNumber(jbaud) ? (uint32_t)jbaud->valuedouble : 115200;

    serial_port_t *sp = serial_port_open(device, baud);
    if (!sp) {
        LOG_ERROR("cmd_handler", "Cannot open serial port: %s", device);
        return err_resp("failed to open serial port");
    }

    gs->serial    = sp;
    gs->connected = true;
    node_manager_gateway_set_connected(true, device, baud);

    /* want_config 握手 */
    uint8_t proto_buf[64];
    size_t  proto_len = 0;
    uint32_t config_id = (uint32_t)time(NULL);
    if (proto_build_want_config(config_id, proto_buf, sizeof(proto_buf), &proto_len) == MESH_OK)
        send_proto(gs, proto_buf, proto_len);

    LOG_INFO("cmd_handler", "Serial connected: %s @ %u baud", device, baud);

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status",   "ok");
    cJSON_AddStringToObject(obj, "device",   device);
    cJSON_AddNumberToObject(obj, "baudrate", baud);
    return json_to_str(obj);
}

static char *cmd_disconnect_serial(gateway_state_t *gs) {
    if (!gs->serial)
        return err_resp("not connected");

    serial_port_close(gs->serial);
    gs->serial    = NULL;
    gs->connected = false;
    node_manager_gateway_set_connected(false, "", 0);

    LOG_INFO("cmd_handler", "Serial disconnected");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "ok");
    return json_to_str(obj);
}

static char *cmd_send_text(cJSON *req, gateway_state_t *gs) {
    cJSON *jto   = cJSON_GetObjectItemCaseSensitive(req, "to");
    cJSON *jtext = cJSON_GetObjectItemCaseSensitive(req, "text");
    if (!cJSON_IsString(jto))   return err_resp("missing 'to'");
    if (!cJSON_IsString(jtext)) return err_resp("missing 'text'");

    const char *to_str = jto->valuestring;
    const char *text   = jtext->valuestring;

    cJSON *jch  = cJSON_GetObjectItemCaseSensitive(req, "channel");
    cJSON *jack = cJSON_GetObjectItemCaseSensitive(req, "want_ack");
    uint8_t channel  = cJSON_IsNumber(jch) ? (uint8_t)jch->valuedouble : 0;
    bool    want_ack = cJSON_IsBool(jack)  ? (bool)jack->valueint : false;

    uint32_t to_num;
    if (strcmp(to_str, "^all") == 0 || strcmp(to_str, MESH_BROADCAST_ID) == 0)
        to_num = MESH_BROADCAST_NUM;
    else {
        to_num = mesh_node_id_to_num(to_str);
        if (to_num == 0) return err_resp("invalid 'to' node id");
    }

    if (!gs->connected || !gs->serial)
        return err_resp("not connected");

    uint8_t proto_buf[512];
    size_t  proto_len = 0;
    if (proto_build_text_packet(gs->my_node_num, to_num, text,
                                channel, 3, want_ack, 0,
                                proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return err_resp("proto_build_text failed");

    if (!send_proto(gs, proto_buf, proto_len))
        return err_resp("serial send failed");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "ok");
    cJSON_AddStringToObject(obj, "to",     to_str);
    cJSON_AddStringToObject(obj, "text",   text);
    return json_to_str(obj);
}

/* send_position */
static char *cmd_send_position(cJSON *req, gateway_state_t *gs) {
    cJSON *jto  = cJSON_GetObjectItemCaseSensitive(req, "to");
    cJSON *jlat = cJSON_GetObjectItemCaseSensitive(req, "lat");
    cJSON *jlon = cJSON_GetObjectItemCaseSensitive(req, "lon");
    if (!cJSON_IsString(jto)) return err_resp("missing 'to'");
    if (!cJSON_IsNumber(jlat)) return err_resp("missing 'lat'");
    if (!cJSON_IsNumber(jlon)) return err_resp("missing 'lon'");

    const char *to_str = jto->valuestring;
    double lat = jlat->valuedouble;
    double lon = jlon->valuedouble;
    cJSON *jalt = cJSON_GetObjectItemCaseSensitive(req, "alt");
    cJSON *jch  = cJSON_GetObjectItemCaseSensitive(req, "channel");
    int32_t alt     = cJSON_IsNumber(jalt) ? (int32_t)jalt->valuedouble : 0;
    uint8_t channel = cJSON_IsNumber(jch)  ? (uint8_t)jch->valuedouble  : 0;

    uint32_t to_num;
    if (strcmp(to_str, "^all") == 0 || strcmp(to_str, MESH_BROADCAST_ID) == 0)
        to_num = MESH_BROADCAST_NUM;
    else {
        to_num = mesh_node_id_to_num(to_str);
        if (to_num == 0) return err_resp("invalid 'to' node id");
    }

    if (!gs->connected || !gs->send_raw) return err_resp("not connected");

    uint8_t proto_buf[512];
    size_t  proto_len = 0;
    if (proto_builder_position(gs->my_node_num, to_num, lat, lon, alt, channel,
                                proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return err_resp("proto_build_position failed");

    if (!send_proto(gs, proto_buf, proto_len))
        return err_resp("serial send failed");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "ok");
    cJSON_AddStringToObject(obj, "to",     to_str);
    cJSON_AddNumberToObject(obj, "lat",    lat);
    cJSON_AddNumberToObject(obj, "lon",    lon);
    cJSON_AddNumberToObject(obj, "alt",    alt);
    return json_to_str(obj);
}

/* ─── Admin 命令公共：解析 node_id + passkey ─── */
static bool parse_admin_params(cJSON *req, uint32_t *dest_num_out,
                                uint32_t *passkey_out, char **err_out)
{
    cJSON *jnode = cJSON_GetObjectItemCaseSensitive(req, "node_id");
    if (!cJSON_IsString(jnode)) { *err_out = "missing 'node_id'"; return false; }

    uint32_t dest = mesh_node_id_to_num(jnode->valuestring);
    if (dest == 0) { *err_out = "invalid 'node_id'"; return false; }

    cJSON *jpk = cJSON_GetObjectItemCaseSensitive(req, "passkey");
    *dest_num_out = dest;
    *passkey_out  = cJSON_IsNumber(jpk) ? (uint32_t)jpk->valuedouble : 0;
    return true;
}

/* admin_get_session_passkey */
static char *cmd_admin_get_session_passkey(cJSON *req, gateway_state_t *gs) {
    if (!gs->connected || !gs->send_raw) return err_resp("not connected");
    uint32_t dest, passkey; char *errstr;
    if (!parse_admin_params(req, &dest, &passkey, &errstr)) return err_resp(errstr);

    uint8_t proto_buf[512]; size_t proto_len = 0;
    if (admin_build_get_session_passkey(dest, gs->my_node_num,
                                        proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return err_resp("admin_build failed");
    if (!send_proto(gs, proto_buf, proto_len)) return err_resp("serial send failed");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "ok");
    cJSON_AddStringToObject(obj, "note", "response arrives via monitor event (portnum=6)");
    return json_to_str(obj);
}

/* admin_get_config */
static char *cmd_admin_get_config(cJSON *req, gateway_state_t *gs) {
    if (!gs->connected || !gs->send_raw) return err_resp("not connected");
    uint32_t dest, passkey; char *errstr;
    if (!parse_admin_params(req, &dest, &passkey, &errstr)) return err_resp(errstr);

    cJSON *jtype = cJSON_GetObjectItemCaseSensitive(req, "config_type");
    uint32_t config_type = cJSON_IsNumber(jtype) ? (uint32_t)jtype->valuedouble : 0;

    uint8_t proto_buf[512]; size_t proto_len = 0;
    if (admin_build_get_config(dest, gs->my_node_num, config_type, passkey,
                                proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return err_resp("admin_build failed");
    if (!send_proto(gs, proto_buf, proto_len)) return err_resp("serial send failed");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "ok");
    cJSON_AddStringToObject(obj, "note", "response arrives via monitor event (portnum=6)");
    return json_to_str(obj);
}

/* admin_get_channel */
static char *cmd_admin_get_channel(cJSON *req, gateway_state_t *gs) {
    if (!gs->connected || !gs->send_raw) return err_resp("not connected");
    uint32_t dest, passkey; char *errstr;
    if (!parse_admin_params(req, &dest, &passkey, &errstr)) return err_resp(errstr);

    cJSON *jidx = cJSON_GetObjectItemCaseSensitive(req, "channel_index");
    uint32_t ch_idx = cJSON_IsNumber(jidx) ? (uint32_t)jidx->valuedouble : 0;

    uint8_t proto_buf[512]; size_t proto_len = 0;
    if (admin_build_get_channel(dest, gs->my_node_num, ch_idx, passkey,
                                 proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return err_resp("admin_build failed");
    if (!send_proto(gs, proto_buf, proto_len)) return err_resp("serial send failed");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "ok");
    cJSON_AddStringToObject(obj, "note", "response arrives via monitor event (portnum=6)");
    return json_to_str(obj);
}

/* admin_reboot */
static char *cmd_admin_reboot(cJSON *req, gateway_state_t *gs) {
    if (!gs->connected || !gs->send_raw) return err_resp("not connected");
    uint32_t dest, passkey; char *errstr;
    if (!parse_admin_params(req, &dest, &passkey, &errstr)) return err_resp(errstr);

    cJSON *jdelay = cJSON_GetObjectItemCaseSensitive(req, "delay_s");
    int32_t delay_s = cJSON_IsNumber(jdelay) ? (int32_t)jdelay->valuedouble : 5;

    uint8_t proto_buf[512]; size_t proto_len = 0;
    if (admin_build_reboot(dest, gs->my_node_num, delay_s, passkey,
                            proto_buf, sizeof(proto_buf), &proto_len) != MESH_OK)
        return err_resp("admin_build failed");
    if (!send_proto(gs, proto_buf, proto_len)) return err_resp("serial send failed");

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "status", "ok");
    cJSON_AddNumberToObject(obj, "delay_s", delay_s);
    return json_to_str(obj);
}

/* ─────────────────── 主入口 ─────────────────── */

char *command_handle(sock_t client_fd, const char *json_str, void *userdata) {
    (void)client_fd;
    gateway_state_t *gs = (gateway_state_t *)userdata;

    cJSON *req = cJSON_Parse(json_str);
    if (!req) {
        LOG_WARN("cmd_handler", "JSON parse error: %.80s", json_str);
        return err_resp("invalid JSON");
    }

    cJSON *jcmd = cJSON_GetObjectItemCaseSensitive(req, "cmd");
    const char *cmd = cJSON_IsString(jcmd) ? jcmd->valuestring : "";
    LOG_DEBUG("cmd_handler", "cmd=%s", cmd);

    char *resp = NULL;

    if      (strcmp(cmd, "get_status")              == 0) resp = cmd_get_status(gs);
    else if (strcmp(cmd, "get_gateway_info")         == 0) resp = cmd_get_gateway_info();
    else if (strcmp(cmd, "get_nodes")                == 0) resp = cmd_get_nodes();
    else if (strcmp(cmd, "get_node")                 == 0) resp = cmd_get_node(req);
    else if (strcmp(cmd, "connect_serial")           == 0) resp = cmd_connect_serial(req, gs);
    else if (strcmp(cmd, "disconnect_serial")        == 0) resp = cmd_disconnect_serial(gs);
    else if (strcmp(cmd, "send_text")                == 0) resp = cmd_send_text(req, gs);
    else if (strcmp(cmd, "send_position")            == 0) resp = cmd_send_position(req, gs);
    else if (strcmp(cmd, "admin_get_session_passkey")== 0) resp = cmd_admin_get_session_passkey(req, gs);
    else if (strcmp(cmd, "admin_get_config")         == 0) resp = cmd_admin_get_config(req, gs);
    else if (strcmp(cmd, "admin_get_channel")        == 0) resp = cmd_admin_get_channel(req, gs);
    else if (strcmp(cmd, "admin_reboot")             == 0) resp = cmd_admin_reboot(req, gs);
    else if (strcmp(cmd, "monitor_start")    == 0) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "status", "ok");
        cJSON_AddBoolToObject(o, "monitor", true);
        resp = json_to_str(o);
    }
    else if (strcmp(cmd, "monitor_stop")     == 0) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "status", "ok");
        cJSON_AddBoolToObject(o, "monitor", false);
        resp = json_to_str(o);
    }
    else {
        cJSON *o = cJSON_CreateObject();
        char  msg[128];
        snprintf(msg, sizeof(msg), "unknown cmd: %s", cmd);
        cJSON_AddStringToObject(o, "status", "error");
        cJSON_AddStringToObject(o, "error",  msg);
        resp = json_to_str(o);
    }

    cJSON_Delete(req);
    return resp;
}
