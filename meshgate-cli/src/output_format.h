/**
 * output_format.h - 输出格式化接口
 */
#ifndef OUTPUT_FORMAT_H
#define OUTPUT_FORMAT_H

typedef enum {
    FMT_TABLE = 0,
    FMT_JSON,
    FMT_CSV
} output_format_t;

/**
 * 解析命令行格式选项字符串
 * @param s "table"|"json"|"csv"，默认 TABLE
 */
output_format_t output_format_parse(const char *s);

/**
 * 打印 get_status 响应
 */
void output_status(const char *json, output_format_t fmt);

/**
 * 打印 get_nodes 响应
 */
void output_nodes(const char *json, output_format_t fmt);

/**
 * 打印单个实时事件帧
 */
void output_event(const char *json);

#endif /* OUTPUT_FORMAT_H */
