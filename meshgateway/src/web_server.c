/**
 * web_server.c - 嵌入式 HTTP + WebSocket 服务器实现（基于 Mongoose）
 *
 * 设计要点：
 *   - 单线程，非阻塞：在主 select() 循环末尾调用 mg_mgr_poll(&ws->mgr, 0)
 *   - WebSocket 客户端建立连接后自动接收所有广播事件
 *   - 命令处理复用 command_handler（与 TCP IPC 共享同一套逻辑）
 *   - 静态文件从 static_dir 目录服务（Vue 构建产物 dist/ 放入此目录）
 */
#include "web_server.h"
#include "command_handler.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* CORS 头：允许开发模式下 Vite (localhost:5173) 访问 */
#define CORS_HEADERS \
    "Access-Control-Allow-Origin: *\r\n" \
    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n" \
    "Access-Control-Allow-Headers: Content-Type\r\n"

/* 内嵌极简仪表盘（供 static/ 目录为空时使用）*/
static const char *s_fallback_html =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<title>MeshGateway</title>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>"
    "body{font-family:monospace;background:#1a1a2e;color:#eee;margin:0;padding:16px}"
    "h1{color:#e94560;margin:0 0 12px}h2{color:#0f3460;background:#e94560;padding:4px 8px;margin:0 0 8px}"
    "#status,#nodes,#send,#log{background:#16213e;border-radius:6px;padding:12px;margin-bottom:12px}"
    "table{width:100%;border-collapse:collapse;font-size:12px}"
    "th{text-align:left;border-bottom:1px solid #0f3460;padding:4px;color:#e94560}"
    "td{padding:3px 4px;border-bottom:1px solid #0f3460}"
    "input,select{background:#0f3460;color:#eee;border:1px solid #e94560;padding:4px 8px;border-radius:3px}"
    "button{background:#e94560;color:#fff;border:none;padding:6px 16px;border-radius:3px;cursor:pointer}"
    "button:hover{background:#c73652}"
    "#log-area{height:200px;overflow-y:auto;font-size:11px;background:#0d1b2a;padding:8px;border-radius:3px}"
    ".ok{color:#4CAF50}.err{color:#e94560}.ev{color:#64b5f6}"
    ".badge{display:inline-block;padding:2px 6px;border-radius:3px;font-size:11px}"
    ".on{background:#4CAF50;color:#000}.off{background:#e94560;color:#fff}"
    "</style></head><body>"
    "<h1>&#128225; MeshGateway</h1>"
    "<div id='status'><h2>STATUS</h2>"
    "<span id='ws-badge' class='badge off'>WS: disconnected</span> "
    "<span id='serial-badge' class='badge off'>Serial: disconnected</span><br><br>"
    "<span id='status-text'>Connecting...</span></div>"
    "<div id='nodes'><h2>NODES</h2>"
    "<table><thead><tr><th>ID</th><th>Name</th><th>Short</th><th>Bat%</th><th>SNR</th><th>RSSI</th><th>Hops</th></tr></thead>"
    "<tbody id='nodes-tbody'><tr><td colspan=7>No nodes</td></tr></tbody></table></div>"
    "<div id='send'><h2>SEND MESSAGE</h2>"
    "<input id='send-to' placeholder='to: !aabbccdd or broadcast' style='width:200px'> "
    "<input id='send-ch' type='number' value='0' style='width:50px' placeholder='ch'> "
    "<input id='ack-cb' type='checkbox'> ACK "
    "<input id='send-text' placeholder='Message text...' style='width:280px'> "
    "<button onclick='sendMsg()'>Send</button></div>"
    "<div id='log'><h2>EVENT LOG</h2><div id='log-area'></div></div>"
    "<script>"
    "var ws=null,logArea=document.getElementById('log-area');"
    "function log(msg,cls){"
    "  var d=new Date().toISOString().substr(11,8);"
    "  var div=document.createElement('div');"
    "  div.className=cls||'ev';"
    "  div.textContent=d+' '+msg;"
    "  logArea.appendChild(div);"
    "  logArea.scrollTop=logArea.scrollHeight;"
    "}"
    "function connect(){"
    "  var proto=location.protocol==='https:'?'wss:':'ws:';"
    "  ws=new WebSocket(proto+'//'+location.host+'/ws');"
    "  ws.onopen=function(){"
    "    document.getElementById('ws-badge').className='badge on';"
    "    document.getElementById('ws-badge').textContent='WS: connected';"
    "    log('WebSocket connected','ok');"
    "    send('{\"cmd\":\"get_status\"}');"
    "    send('{\"cmd\":\"get_nodes\"}');"
    "  };"
    "  ws.onclose=function(){"
    "    document.getElementById('ws-badge').className='badge off';"
    "    document.getElementById('ws-badge').textContent='WS: disconnected';"
    "    log('WebSocket disconnected — reconnecting in 3s...','err');"
    "    setTimeout(connect,3000);"
    "  };"
    "  ws.onerror=function(){log('WebSocket error','err');};"
    "  ws.onmessage=function(e){"
    "    try{var d=JSON.parse(e.data);}catch(ex){log(e.data);return;}"
    "    if(d.event){handleEvent(d);}else if(d.status){handleResp(d);}"
    "  };"
    "}"
    "function send(j){if(ws&&ws.readyState===1)ws.send(j);}"
    "function handleResp(d){"
    "  if(d.connected!==undefined){"
    "    document.getElementById('status-text').innerHTML="
    "      'Node: <b>'+((d.my_node_id||'?'))+\n"
    "      '</b> | Nodes: <b>'+(d.node_count||0)+'</b> | RX: <b>'+(d.rx_count||0)+'</b> | Uptime: <b>'+(d.uptime_s||0)+'s</b>';"
    "    var sb=document.getElementById('serial-badge');"
    "    sb.className='badge '+(d.connected?'on':'off');"
    "    sb.textContent='Serial: '+(d.connected?'connected':'disconnected');"
    "  }"
    "  if(d.nodes){buildNodes(d.nodes);}"
    "  log(JSON.stringify(d),'ok');"
    "}"
    "function handleEvent(d){"
    "  log(JSON.stringify(d),'ev');"
    "  if(d.event==='node_new'||d.event==='node_updated'){"
    "    send('{\"cmd\":\"get_nodes\"}');"
    "  }"
    "  if(d.event==='config_complete'||d.event==='serial_connected'){"
    "    send('{\"cmd\":\"get_status\"}');"
    "    send('{\"cmd\":\"get_nodes\"}');"
    "  }"
    "}"
    "function buildNodes(nodes){"
    "  var tb=document.getElementById('nodes-tbody');"
    "  if(!nodes.length){tb.innerHTML='<tr><td colspan=7>No nodes</td></tr>';return;}"
    "  tb.innerHTML=nodes.map(function(n){"
    "    return '<tr><td>'+n.node_id+'</td><td>'+n.long_name+'</td><td>'+n.short_name+'</td>'+"
    "      '<td>'+n.battery+'%</td><td>'+n.snr+'</td><td>'+n.rssi+'</td><td>'+n.hops_away+'</td></tr>';"
    "  }).join('');"
    "}"
    "function sendMsg(){"
    "  var to=document.getElementById('send-to').value.trim();"
    "  var text=document.getElementById('send-text').value.trim();"
    "  var ch=document.getElementById('send-ch').value||'0';"
    "  var ack=document.getElementById('ack-cb').checked;"
    "  if(!to||!text){alert('Fill in \"to\" and message');return;}"
    "  var cmd=JSON.stringify({cmd:'send_text',to:to,text:text,channel:parseInt(ch),want_ack:ack});"
    "  send(cmd);"
    "  document.getElementById('send-text').value='';"
    "}"
    "connect();"
    "setInterval(function(){send('{\"cmd\":\"get_status\"}');},10000);"
    "</script></body></html>";

/* ─── Mongoose 事件处理器 ─── */

static void http_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    web_server_t *ws = (web_server_t *)c->fn_data;

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        /* OPTIONS 预检（CORS）*/
        if (mg_strcasecmp(hm->method, mg_str("OPTIONS")) == 0) {
            mg_http_reply(c, 204,
                CORS_HEADERS
                "Content-Length: 0\r\n", "");
            return;
        }

        /* WebSocket 升级 */
        if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
            LOG_DEBUG("web", "WebSocket client connected");
            return;
        }

        /* POST /api/cmd — 复用 command_handle */
        if (mg_match(hm->uri, mg_str("/api/cmd"), NULL) &&
            mg_strcasecmp(hm->method, mg_str("POST")) == 0) {
            /* 提取 body 为 C 字符串 */
            size_t blen = hm->body.len;
            char  *json = (char *)malloc(blen + 1);
            if (!json) {
                mg_http_reply(c, 500, CORS_HEADERS, "{\"status\":\"error\",\"error\":\"OOM\"}");
                return;
            }
            memcpy(json, hm->body.buf, blen);
            json[blen] = '\0';

            char *resp = command_handle((sock_t)0, json, ws->gstate);
            free(json);

            if (resp) {
                mg_http_reply(c, 200,
                    "Content-Type: application/json\r\n" CORS_HEADERS,
                    "%s", resp);
                free(resp);
            } else {
                mg_http_reply(c, 200,
                    "Content-Type: application/json\r\n" CORS_HEADERS,
                    "{\"status\":\"error\",\"error\":\"null response\"}");
            }
            return;
        }

        /* GET /api/status — 快捷接口 */
        if (mg_match(hm->uri, mg_str("/api/status"), NULL)) {
            char *resp = command_handle((sock_t)0, "{\"cmd\":\"get_status\"}", ws->gstate);
            if (resp) {
                mg_http_reply(c, 200,
                    "Content-Type: application/json\r\n" CORS_HEADERS,
                    "%s", resp);
                free(resp);
            } else {
                mg_http_reply(c, 500, CORS_HEADERS, "{}");
            }
            return;
        }

        /* GET /api/nodes — 快捷接口 */
        if (mg_match(hm->uri, mg_str("/api/nodes"), NULL)) {
            char *resp = command_handle((sock_t)0, "{\"cmd\":\"get_nodes\"}", ws->gstate);
            if (resp) {
                mg_http_reply(c, 200,
                    "Content-Type: application/json\r\n" CORS_HEADERS,
                    "%s", resp);
                free(resp);
            } else {
                mg_http_reply(c, 500, CORS_HEADERS, "{}");
            }
            return;
        }

        /* 服务静态文件；若根目录无 index.html，返回内嵌 fallback */
        char index_path[300];
        snprintf(index_path, sizeof(index_path), "%s/index.html", ws->static_dir);
        FILE *f = fopen(index_path, "r");
        if (f) {
            fclose(f);
            struct mg_http_serve_opts opts = {
                .root_dir = ws->static_dir,
                .extra_headers = CORS_HEADERS
            };
            mg_http_serve_dir(c, hm, &opts);
        } else {
            /* fallback: 内嵌仪表盘 */
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                          "%s", s_fallback_html);
        }

    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        if (wm->data.len == 0) return;

        char *json = (char *)malloc(wm->data.len + 1);
        if (!json) return;
        memcpy(json, wm->data.buf, wm->data.len);
        json[wm->data.len] = '\0';

        char *resp = command_handle((sock_t)0, json, ws->gstate);
        free(json);

        if (resp) {
            mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
            free(resp);
        }

    } else if (ev == MG_EV_WS_OPEN) {
        LOG_INFO("web", "WebSocket handshake complete");
    } else if (ev == MG_EV_CLOSE) {
        if (c->is_websocket)
            LOG_DEBUG("web", "WebSocket client disconnected");
    }
}

/* ─── 公开接口实现 ─── */

int web_server_init(web_server_t *ws, const char *bind_addr,
                    const char *static_dir, void *gstate)
{
    if (!ws) return -1;
    memset(ws, 0, sizeof(*ws));

    if (!bind_addr || !bind_addr[0]) {
        LOG_INFO("web", "Web server disabled (no bind address)");
        return 0;
    }

    ws->gstate = gstate;
    strncpy(ws->static_dir,
            (static_dir && static_dir[0]) ? static_dir : "static",
            sizeof(ws->static_dir) - 1);

    mg_mgr_init(&ws->mgr);

    /* 构造 Mongoose URL: http://host:port */
    char url[80];
    if (strncmp(bind_addr, "http", 4) == 0)
        snprintf(url, sizeof(url), "%s", bind_addr);
    else
        snprintf(url, sizeof(url), "http://%s", bind_addr);

    struct mg_connection *conn =
        mg_http_listen(&ws->mgr, url, http_ev_handler, ws);
    if (!conn) {
        LOG_ERROR("web", "Cannot start HTTP server on %s", url);
        mg_mgr_free(&ws->mgr);
        return -1;
    }

    ws->initialized = true;
    LOG_INFO("web", "HTTP+WebSocket server listening on %s (static: %s)",
             url, ws->static_dir);
    return 0;
}

void web_server_poll(web_server_t *ws) {
    if (ws && ws->initialized)
        mg_mgr_poll(&ws->mgr, 0);
}

void web_server_broadcast(web_server_t *ws, const char *json) {
    if (!ws || !ws->initialized || !json) return;
    size_t len = strlen(json);
    for (struct mg_connection *c = ws->mgr.conns; c != NULL; c = c->next) {
        if (c->is_websocket)
            mg_ws_send(c, json, len, WEBSOCKET_OP_TEXT);
    }
}

void web_server_close(web_server_t *ws) {
    if (ws && ws->initialized) {
        mg_mgr_free(&ws->mgr);
        ws->initialized = false;
        LOG_INFO("web", "Web server closed");
    }
}
