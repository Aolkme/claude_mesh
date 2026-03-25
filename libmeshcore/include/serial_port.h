/**
 * serial_port.h - 跨平台串口接口
 *
 * 支持：Linux/macOS（POSIX termios）、Windows（Win32 API）
 */
#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 不透明句柄 */
typedef struct serial_port serial_port_t;

/**
 * 打开串口
 * @param path      设备路径（Linux: "/dev/ttyUSB0"，Windows: "COM3" 或 "\\\\.\\COM10"）
 * @param baudrate  波特率（115200, 9600, 57600, 230400 等）
 * @return          句柄，失败返回 NULL
 */
serial_port_t *serial_port_open(const char *path, uint32_t baudrate);

/**
 * 关闭并释放串口
 */
void serial_port_close(serial_port_t *port);

/**
 * 读取数据（阻塞，直到读到至少1字节或超时）
 * @param port     串口句柄
 * @param buf      接收缓冲区
 * @param len      缓冲区大小
 * @param timeout_ms  超时毫秒数（0 = 不超时）
 * @return  读取到的字节数，0 = 超时，-1 = 错误
 */
int serial_port_read(serial_port_t *port, uint8_t *buf, size_t len, uint32_t timeout_ms);

/**
 * 写入数据（阻塞）
 * @param port  串口句柄
 * @param buf   数据缓冲区
 * @param len   数据长度
 * @return  写入字节数，-1 = 错误
 */
int serial_port_write(serial_port_t *port, const uint8_t *buf, size_t len);

/**
 * 判断串口是否已打开
 */
bool serial_port_is_open(const serial_port_t *port);

/**
 * 获取最后一次错误描述
 */
const char *serial_port_error(const serial_port_t *port);

/**
 * 获取底层文件描述符（POSIX: int fd，用于 select()）
 * Windows 返回 -1（串口使用超时读取，不参与 select）
 */
int serial_port_fd(const serial_port_t *port);

#endif /* SERIAL_PORT_H */
