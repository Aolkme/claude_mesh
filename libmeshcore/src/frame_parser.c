/**
 * frame_parser.c - 串口帧同步解析器实现
 *
 * 参考：meshdebug/serial_worker.py SerialWorker._read_one_frame()
 */
#include "frame_parser.h"
#include <string.h>

void frame_parser_init(frame_parser_t *parser,
                       uint8_t *payload_buf, size_t payload_cap,
                       uint8_t *text_buf, size_t text_cap)
{
    memset(parser, 0, sizeof(frame_parser_t));
    parser->payload_buf      = payload_buf;
    parser->payload_capacity = payload_cap;
    parser->text_buf         = text_buf;
    parser->text_capacity    = text_cap;
    parser->state            = FRAME_STATE_WAIT_START1;
}

void frame_parser_reset(frame_parser_t *parser)
{
    parser->state            = FRAME_STATE_WAIT_START1;
    parser->payload_expected = 0;
    parser->payload_received = 0;
    /* 不清空 text_buf —— 未处理的文本保留 */
}

frame_result_t frame_parser_feed(frame_parser_t *parser, uint8_t byte)
{
    switch (parser->state) {

    case FRAME_STATE_WAIT_START1:
        if (byte == FRAME_MAGIC1) {
            parser->state = FRAME_STATE_WAIT_START2;
        } else {
            /* 非帧字节，追加到文本缓冲 */
            goto append_text;
        }
        return FRAME_RESULT_PENDING;

    case FRAME_STATE_WAIT_START2:
        if (byte == FRAME_MAGIC2) {
            parser->state = FRAME_STATE_WAIT_LEN_H;
        } else {
            /* 0x94 后面不是 0xC3，把 0x94 和当前字节都当文本处理 */
            uint8_t magic1 = FRAME_MAGIC1;
            if (parser->text_len < parser->text_capacity - 1) {
                parser->text_buf[parser->text_len++] = magic1;
            }
            parser->state = FRAME_STATE_WAIT_START1;
            /* 当前字节也当文本 */
            goto append_text;
        }
        return FRAME_RESULT_PENDING;

    case FRAME_STATE_WAIT_LEN_H:
        parser->payload_expected = (uint16_t)byte << 8;
        parser->state = FRAME_STATE_WAIT_LEN_L;
        return FRAME_RESULT_PENDING;

    case FRAME_STATE_WAIT_LEN_L:
        parser->payload_expected |= byte;
        parser->payload_received  = 0;

        /* 检查合法性 */
        if (parser->payload_expected == 0 ||
            parser->payload_expected > FRAME_MAX_PAYLOAD) {
            parser->frames_error++;
            frame_parser_reset(parser);
            return FRAME_RESULT_ERROR;
        }
        if (parser->payload_expected > parser->payload_capacity) {
            parser->frames_error++;
            frame_parser_reset(parser);
            return FRAME_RESULT_ERROR;
        }

        parser->state = FRAME_STATE_WAIT_PAYLOAD;
        return FRAME_RESULT_PENDING;

    case FRAME_STATE_WAIT_PAYLOAD:
        parser->payload_buf[parser->payload_received++] = byte;

        if (parser->payload_received == parser->payload_expected) {
            /* 帧完成 */
            parser->frames_received++;
            parser->state = FRAME_STATE_WAIT_START1;
            return FRAME_RESULT_COMPLETE;
        }
        return FRAME_RESULT_PENDING;
    }

    return FRAME_RESULT_PENDING;

append_text:
    if (parser->text_len < parser->text_capacity - 1) {
        parser->text_buf[parser->text_len++] = byte;
        parser->text_buf[parser->text_len]   = '\0';
    }
    return FRAME_RESULT_TEXT_BYTE;
}

const uint8_t *frame_parser_get_payload(const frame_parser_t *parser, uint16_t *len)
{
    if (len) *len = parser->payload_expected;
    return parser->payload_buf;
}

const uint8_t *frame_parser_get_text(const frame_parser_t *parser, size_t *len)
{
    if (len) *len = parser->text_len;
    return parser->text_buf;
}

void frame_parser_clear_text(frame_parser_t *parser)
{
    parser->text_len = 0;
    if (parser->text_buf && parser->text_capacity > 0) {
        parser->text_buf[0] = '\0';
    }
}
