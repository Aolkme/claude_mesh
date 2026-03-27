/**
 * node_manager.h - 节点表管理接口
 *
 * 维护两张表：
 *   1. 网关节点表（g_gateway）：自身节点，单条，握手期填充
 *   2. 远程节点表（g_nodes[]）：其他节点，最多 MESH_MAX_NODES 条
 */
#ifndef NODE_MANAGER_H
#define NODE_MANAGER_H

#include "mesh_types.h"

/* ─── 回调 ─── */
typedef void (*node_update_callback_t)(const mesh_node_t *node,
                                        bool is_new, void *userdata);

/* ─────────── 初始化 / 销毁 ─────────── */

int  node_manager_init(void);
void node_manager_close(void);

/* ─────────── 网关节点（自身）─────────── */

/**
 * 获取网关节点（可写），返回内部指针
 * 初始化后始终非 NULL
 */
mesh_gateway_node_t *node_manager_gateway(void);

/** 设置网关 node_num（来自 MY_INFO 事件）*/
void node_manager_gateway_set_num(uint32_t node_num);

/** 设置网关连接状态 */
void node_manager_gateway_set_connected(bool connected,
                                         const char *device,
                                         uint32_t baudrate);

/** 设置握手完成标志 */
void node_manager_gateway_set_config_complete(bool done);

/* ─────────── 远程节点表 ─────────── */

/**
 * 更新/添加远程节点（全量更新，保留 first_seen_ms）
 * @return MESH_OK 或 MESH_ERROR_NODE_DB_FULL
 */
mesh_error_t node_manager_update(const mesh_node_t *node);

/**
 * 字段级更新：仅更新私有配置字段（不覆盖遥测/位置等数据）
 * 若节点不存在则静默忽略，返回 MESH_ERROR_NODE_NOT_FOUND
 */
mesh_error_t node_manager_set_enrolled(const char *node_id,
                                        bool is_enrolled,
                                        const uint8_t *company_pub_key,
                                        uint64_t change_ts);

mesh_error_t node_manager_set_private_config(const char *node_id,
                                              bool is_admin_key_set,
                                              uint32_t version,
                                              const char *device_name);

/**
 * 从 PACKET 事件中更新节点字段（位置/遥测/用户信息/信号）
 * 自动识别 packet.portnum 更新对应字段，更新 last_heard_ms
 */
mesh_error_t node_manager_update_from_packet(const mesh_packet_t *pkt);

/* ─────────── 查询 ─────────── */

const mesh_node_t *node_manager_get(const char *node_id);
const mesh_node_t *node_manager_get_by_num(uint32_t node_num);
int  node_manager_get_all(mesh_node_t *out, int max_count);
int  node_manager_get_count(void);

/* ─────────── 维护 ─────────── */

/** 删除 expire_ms 毫秒内未听到的节点 */
void node_manager_expire(uint64_t expire_ms);

/** 注册节点更新回调（新节点/节点变更时触发）*/
void node_manager_set_callback(node_update_callback_t cb, void *userdata);

#endif /* NODE_MANAGER_H */
