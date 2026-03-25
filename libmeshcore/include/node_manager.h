/**
 * node_manager.h - 节点表管理接口
 */
#ifndef NODE_MANAGER_H
#define NODE_MANAGER_H

#include "mesh_types.h"

/* 回调：节点更新时调用 */
typedef void (*node_update_callback_t)(const mesh_node_t *node, bool is_new, void *userdata);

int  node_manager_init(void);
void node_manager_close(void);

/* 更新/添加节点，返回 MESH_OK 或 MESH_ERROR_NODE_DB_FULL */
mesh_error_t node_manager_update(const mesh_node_t *node);

/* 查询 */
const mesh_node_t *node_manager_get(const char *node_id);
const mesh_node_t *node_manager_get_by_num(uint32_t node_num);
int  node_manager_get_all(mesh_node_t *out, int max_count);
int  node_manager_get_count(void);

/* 删除过期节点（expire_ms 毫秒内未听到）*/
void node_manager_expire(uint64_t expire_ms);

/* 设置更新回调 */
void node_manager_set_callback(node_update_callback_t cb, void *userdata);

#endif /* NODE_MANAGER_H */
