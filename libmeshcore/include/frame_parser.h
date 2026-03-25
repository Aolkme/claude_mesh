/**
 * frame_parser.h - 串口帧同步解析器
 *
 * Meshtastic 串口帧格式：
 *   [0x94][0xC3][len_MSB][len_LSB][FromRadio protobuf payload]
 *
 * 状态机：WAIT_START1 → WAIT_START2 → WAIT_LEN_H → WAIT_LEN_L → WAIT_PAYLOAD
 */
#ifndef FRAME_PARSER_H
#define FRAME_PARSER_H

#include <stdint.h>
#include <stddef.h>

/* ─────────────────── 常量 ─────────────────── */

#define FRAME_MAGIC1        0x94
#define FRAME_MAGIC2        0xC3
#define FRAME_MAX_PAYLOAD   65535           /* 64KB 上限（保护）*/
#define FRAME_TYPICAL_MAX   4096            /* 典型最大帧大小 */

/* ─────────────────── 状态机 ─────────────────── */

typedef enum {
    FRAME_STATE_WAIT_START1,    /* 等待 0x94 */
    FRAME_STATE_WAIT_START2,    /* 等待 0xC3 */
    FRAME_STATE_WAIT_LEN_H,     /* 等待长度高字节 */
    FRAME_STATE_WAIT_LEN_L,     /* 等待长度低字节 */
    FRAME_STATE_WAIT_PAYLOAD,   /* 接收 payload */
} frame_state_t;

/* ─────────────────── 返回值 ─────────────────── */

typedef enum {
    FRAME_RESULT_PENDING   = 0,     /* 帧未完整，继续等待 */
    FRAME_RESULT_COMPLETE  = 1,     /* 帧完整，payload 已就绪 */
    FRAME_RESULT_TEXT_BYTE = 2,     /* 非帧字节，可能是设备日志 */
    FRAME_RESULT_ERROR     = -1,    /* 错误（长度非法等） */
} frame_result_t;

/* ─────────────────── 解析器结构体 ─────────────────── */

typedef struct {
    frame_state_t state;

    /* 帧接收缓冲区 */
    uint8_t *payload_buf;
    size_t   payload_capacity;
    uint16_t payload_expected;   /* 期望接收的字节数 */
    uint16_t payload_received;   /* 已接收的字节数 */

    /* 文本字节缓冲（非帧字节，来自设备 Serial.println） */
    uint8_t *text_buf;
    size_t   text_capacity;
    size_t   text_len;

    /* 统计 */
    uint32_t frames_received;
    uint32_t frames_error;
} frame_parser_t;

/* ─────────────────── 接口 ─────────────────── */

/**
 * 初始化帧解析器
 * @param parser       解析器实例
 * @param payload_buf  payload 缓冲区（建议 4096 字节）
 * @param payload_cap  缓冲区大小
 * @param text_buf     文本缓冲区（建议 256 字节）
 * @param text_cap     文本缓冲区大小
 */
void frame_parser_init(frame_parser_t *parser,
                       uint8_t *payload_buf, size_t payload_cap,
                       uint8_t *text_buf, size_t text_cap);

/**
 * 重置解析器状态（保留缓冲区）
 */
void frame_parser_reset(frame_parser_t *parser);

/**
 * 喂入一个字节，更新状态机
 *
 * @param parser  解析器实例
 * @param byte    输入字节
 * @return FRAME_RESULT_COMPLETE  -> payload 已就绪，可通过 frame_parser_get_payload() 获取
 *         FRAME_RESULT_PENDING   -> 继续等待
 *         FRAME_RESULT_TEXT_BYTE -> 非帧字节已追加到 text_buf
 *         FRAME_RESULT_ERROR     -> 长度非法，状态机已重置
 */
frame_result_t frame_parser_feed(frame_parser_t *parser, uint8_t byte);

/**
 * 获取最近一个完整帧的 payload（调用 feed 返回 COMPLETE 后有效）
 * @param len  输出：payload 长度
 * @return     payload 缓冲区指针（只读，下次 feed 前有效）
 */
const uint8_t *frame_parser_get_payload(const frame_parser_t *parser, uint16_t *len);

/**
 * 获取文本缓冲区内容
 * @param len  输出：文本长度
 * @return     文本缓冲区指针
 */
const uint8_t *frame_parser_get_text(const frame_parser_t *parser, size_t *len);

/**
 * 清空文本缓冲区
 */
void frame_parser_clear_text(frame_parser_t *parser);

#endif /* FRAME_PARSER_H */
