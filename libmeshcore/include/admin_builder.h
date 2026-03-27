/**
 * admin_builder.h - AdminMessage (PortNum 6) 构造接口
 *
 * 直接使用 meshtastic/admin.pb-c.h，不重新实现协议逻辑。
 * Session Passkey 以参数传入；阶段三再实现自动管理。
 */
#ifndef ADMIN_BUILDER_H
#define ADMIN_BUILDER_H

#include "mesh_types.h"

/**
 * 请求目标节点的 Session Passkey（无需 passkey，通常是第一步）
 * 响应通过 PORTNUM_ADMIN 事件返回，携带 get_session_passkey_response。
 */
mesh_error_t admin_build_get_session_passkey(
    uint32_t dest_num, uint32_t from_num,
    uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 读取目标节点的设备配置
 *
 * @param config_type  配置类型（0=DEVICE,1=POSITION,2=POWER,3=NETWORK,
 *                              4=DISPLAY,5=LORA,6=BLUETOOTH）
 * @param passkey      Session Passkey（uint32_t，little-endian 4 字节）
 */
mesh_error_t admin_build_get_config(
    uint32_t dest_num, uint32_t from_num,
    uint32_t config_type, uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 读取目标节点的信道配置
 *
 * @param channel_index  信道索引（0-7）
 * @param passkey        Session Passkey
 */
mesh_error_t admin_build_get_channel(
    uint32_t dest_num, uint32_t from_num,
    uint32_t channel_index, uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 重启目标节点
 *
 * @param delay_s  延迟秒数（0 = 立即重启）
 * @param passkey  Session Passkey
 */
mesh_error_t admin_build_reboot(
    uint32_t dest_num, uint32_t from_num,
    int32_t delay_s, uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len);

/**
 * 恢复目标节点出厂设置（仅配置）
 *
 * @param passkey  Session Passkey
 */
mesh_error_t admin_build_factory_reset_config(
    uint32_t dest_num, uint32_t from_num,
    uint32_t passkey,
    uint8_t *buf, size_t buf_len, size_t *out_len);

#endif /* ADMIN_BUILDER_H */
