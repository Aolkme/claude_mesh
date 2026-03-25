/**
 * output_format.c - 表格/JSON/CSV 格式化输出实现
 */
#include "output_format.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

output_format_t output_format_parse(const char *s) {
    if (!s) return FMT_TABLE;
    if (strcmp(s, "json") == 0) return FMT_JSON;
    if (strcmp(s, "csv")  == 0) return FMT_CSV;
    return FMT_TABLE;
}

/* ─── 极简 JSON 字段提取（与 daemon 端相同） ─── */
static const char *jstr(const char *json, const char *key, char *out, size_t olen) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) { out[0]='\0'; return out; }
    p += strlen(search);
    while (*p==' '||*p==':') p++;
    if (*p=='"') {
        p++;
        size_t i=0;
        while (*p&&*p!='"'&&i<olen-1) out[i++]=*p++;
        out[i]='\0';
    } else { out[0]='\0'; }
    return out;
}

static int jint(const char *json, const char *key, int def) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return def;
    p += strlen(search);
    while (*p==' '||*p==':') p++;
    if (*p>='0'&&*p<='9') return atoi(p);
    if (*p=='-') return atoi(p);
    return def;
}

/* ─── status ─── */
void output_status(const char *json, output_format_t fmt) {
    if (fmt == FMT_JSON) { puts(json); return; }

    char connected[8], node_id[16], node_count[8], rx_count[16], uptime[16];
    jstr(json, "connected",   connected,  sizeof(connected));
    jstr(json, "my_node_id",  node_id,    sizeof(node_id));
    jstr(json, "node_count",  node_count, sizeof(node_count));
    jstr(json, "rx_count",    rx_count,   sizeof(rx_count));
    jstr(json, "uptime_s",    uptime,     sizeof(uptime));
    /* JSON 中数字不带引号，jstr 不起作用，直接 jint */
    int nc = jint(json, "node_count", 0);
    int rx = jint(json, "rx_count",   0);
    int up = jint(json, "uptime_s",   0);

    if (fmt == FMT_CSV) {
        printf("connected,node_id,node_count,rx_count,uptime_s\n");
        printf("%s,%s,%d,%d,%d\n",
               strcmp(connected,"true")==0?"yes":"no",
               node_id, nc, rx, up);
        return;
    }
    /* TABLE */
    printf("┌─────────────────────────────────────┐\n");
    printf("│  meshgateway status                 │\n");
    printf("├─────────────────┬───────────────────┤\n");
    printf("│ Connected       │ %-17s │\n", strcmp(connected,"true")==0 ? "yes" : "no");
    printf("│ My Node ID      │ %-17s │\n", node_id);
    printf("│ Nodes in DB     │ %-17d │\n", nc);
    printf("│ RX frames       │ %-17d │\n", rx);
    printf("│ Uptime (s)      │ %-17d │\n", up);
    printf("└─────────────────┴───────────────────┘\n");
}

/* ─── nodes ─── */
void output_nodes(const char *json, output_format_t fmt) {
    if (fmt == FMT_JSON) { puts(json); return; }

    /* CSV header */
    if (fmt == FMT_CSV) {
        printf("node_id,node_num,long_name,short_name,snr,battery,last_heard_ms\n");
    } else {
        printf("%-12s  %-10s  %-20s  %-6s  %5s  %7s  %s\n",
               "Node ID", "Num", "Long Name", "Short", "SNR", "Bat%", "Last Heard (ms)");
        printf("%-12s  %-10s  %-20s  %-6s  %5s  %7s  %s\n",
               "------------", "----------", "--------------------",
               "------", "-----", "-------", "---------------");
    }

    /* 遍历 JSON 中的 nodes 数组（手工解析） */
    const char *p = strstr(json, "\"nodes\":[");
    if (!p) return;
    p += 9; /* 跳过 "nodes":[ */

    while (*p && *p != ']') {
        if (*p != '{') { p++; continue; }
        /* 找到结束 } */
        const char *obj_start = p;
        int depth = 0;
        const char *q = p;
        while (*q) {
            if (*q=='{') depth++;
            else if (*q=='}') { depth--; if (depth==0) { q++; break; } }
            q++;
        }
        /* 复制对象字符串 */
        size_t obj_len = (size_t)(q - obj_start);
        char *obj = (char *)malloc(obj_len + 1);
        if (!obj) break;
        memcpy(obj, obj_start, obj_len);
        obj[obj_len] = '\0';

        char node_id[16], long_name[32], short_name[8];
        int  node_num = jint(obj, "node_num", 0);
        jstr(obj, "node_id",    node_id,    sizeof(node_id));
        jstr(obj, "long_name",  long_name,  sizeof(long_name));
        jstr(obj, "short_name", short_name, sizeof(short_name));
        /* snr / battery / last_heard 是数值，用 jint */
        int snr_x10 = (int)(jint(obj, "snr", 0));   /* float 精度有限 */
        int battery  = jint(obj, "battery",  0);
        long long lh = (long long)jint(obj, "last_heard", 0);

        /* 简单浮点 snr 提取 */
        char snr_str[16];
        {
            char search[] = "\"snr\":";
            const char *sp = strstr(obj, search);
            if (sp) {
                sp += strlen(search);
                snprintf(snr_str, sizeof(snr_str), "%.1f", atof(sp));
            } else {
                snprintf(snr_str, sizeof(snr_str), "0.0");
            }
        }

        if (fmt == FMT_CSV) {
            printf("%s,%d,%s,%s,%s,%d,%lld\n",
                   node_id, node_num, long_name, short_name,
                   snr_str, battery, lh);
        } else {
            printf("%-12s  %-10u  %-20s  %-6s  %5s  %6d%%  %lld\n",
                   node_id, (unsigned)node_num,
                   long_name[0] ? long_name : "-",
                   short_name[0] ? short_name : "-",
                   snr_str, battery, lh);
        }
        (void)snr_x10;
        free(obj);
        p = q;
        while (*p == ',' || *p == ' ') p++;
    }
}

/* ─── 实时事件 ─── */
void output_event(const char *json) {
    char event[32], from[16], to[16], text[128], node_id[16];
    jstr(json, "event",    event,   sizeof(event));
    jstr(json, "from",     from,    sizeof(from));
    jstr(json, "to",       to,      sizeof(to));
    jstr(json, "text",     text,    sizeof(text));
    jstr(json, "node_id",  node_id, sizeof(node_id));

    if (strcmp(event, "packet") == 0) {
        if (text[0])
            printf("[MSG] %s → %s: %s\n", from, to, text);
        else
            printf("[PKT] %s → %s (portnum=%d)\n",
                   from, to, jint(json, "portnum", 0));
    } else if (strcmp(event, "node_update") == 0) {
        char long_name[32], snr_str[16];
        jstr(json, "long_name", long_name, sizeof(long_name));
        const char *sp = strstr(json, "\"snr\":");
        if (sp) snprintf(snr_str, sizeof(snr_str), "%.1f", atof(sp + 6));
        else    snprintf(snr_str, sizeof(snr_str), "?");
        printf("[NODE] %s (%s) snr=%s\n", node_id, long_name, snr_str);
    } else {
        printf("[EVENT] %s\n", json);
    }
    fflush(stdout);
}
