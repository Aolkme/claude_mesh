/**
 * serial_port.c - 跨平台串口实现
 *
 * Linux/macOS: POSIX termios
 * Windows:     Win32 CreateFile/ReadFile/WriteFile
 */
#include "serial_port.h"
#include "mesh_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─────────────────── 平台结构 ─────────────────── */

#ifdef _WIN32
#include <windows.h>

struct serial_port {
    HANDLE  handle;
    char    error[256];
};

static void set_error(serial_port_t *port, const char *msg) {
    strncpy(port->error, msg, sizeof(port->error) - 1);
    port->error[sizeof(port->error) - 1] = '\0';
}

serial_port_t *serial_port_open(const char *path, uint32_t baudrate) {
    serial_port_t *port = calloc(1, sizeof(serial_port_t));
    if (!port) return NULL;

    /* Windows 需要对 COM10+ 添加 \\.\\ 前缀 */
    char full_path[64];
    if (path[0] != '\\') {
        snprintf(full_path, sizeof(full_path), "\\\\.\\%s", path);
    } else {
        strncpy(full_path, path, sizeof(full_path) - 1);
    }

    port->handle = CreateFileA(full_path,
                               GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);

    if (port->handle == INVALID_HANDLE_VALUE) {
        set_error(port, "CreateFile failed");
        free(port);
        return NULL;
    }

    /* 设置波特率和参数 */
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(port->handle, &dcb)) {
        CloseHandle(port->handle);
        free(port);
        return NULL;
    }

    dcb.BaudRate = baudrate;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;
    dcb.fBinary  = TRUE;
    dcb.fParity  = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_DISABLE;
    dcb.fRtsControl  = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX  = FALSE;

    if (!SetCommState(port->handle, &dcb)) {
        set_error(port, "SetCommState failed");
        CloseHandle(port->handle);
        free(port);
        return NULL;
    }

    /* 设置超时（非阻塞读取单字节） */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 500;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 500;
    SetCommTimeouts(port->handle, &timeouts);

    return port;
}

void serial_port_close(serial_port_t *port) {
    if (!port) return;
    if (port->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(port->handle);
    }
    free(port);
}

int serial_port_read(serial_port_t *port, uint8_t *buf, size_t len, uint32_t timeout_ms) {
    if (!port || port->handle == INVALID_HANDLE_VALUE) return -1;

    if (timeout_ms > 0) {
        COMMTIMEOUTS to = {0};
        to.ReadIntervalTimeout = 0;
        to.ReadTotalTimeoutMultiplier = 0;
        to.ReadTotalTimeoutConstant = timeout_ms;
        SetCommTimeouts(port->handle, &to);
    }

    DWORD n = 0;
    if (!ReadFile(port->handle, buf, (DWORD)len, &n, NULL)) {
        set_error(port, "ReadFile failed");
        return -1;
    }
    return (int)n;
}

int serial_port_write(serial_port_t *port, const uint8_t *buf, size_t len) {
    if (!port || port->handle == INVALID_HANDLE_VALUE) return -1;
    DWORD n = 0;
    if (!WriteFile(port->handle, buf, (DWORD)len, &n, NULL)) {
        set_error(port, "WriteFile failed");
        return -1;
    }
    return (int)n;
}

bool serial_port_is_open(const serial_port_t *port) {
    return port && port->handle != INVALID_HANDLE_VALUE;
}

const char *serial_port_error(const serial_port_t *port) {
    return port ? port->error : "NULL port";
}

#else /* Linux / macOS */

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>

struct serial_port {
    int  fd;
    char error[256];
};

static void set_error(serial_port_t *port, const char *msg) {
    snprintf(port->error, sizeof(port->error), "%s: %s", msg, strerror(errno));
}

static speed_t baud_to_speed(uint32_t baudrate) {
    switch (baudrate) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default:     return B115200;
    }
}

serial_port_t *serial_port_open(const char *path, uint32_t baudrate) {
    serial_port_t *port = calloc(1, sizeof(serial_port_t));
    if (!port) return NULL;

    port->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port->fd < 0) {
        set_error(port, "open failed");
        free(port);
        return NULL;
    }

    /* 设置为阻塞模式 */
    int flags = fcntl(port->fd, F_GETFL, 0);
    fcntl(port->fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tty;
    if (tcgetattr(port->fd, &tty) != 0) {
        set_error(port, "tcgetattr failed");
        close(port->fd);
        free(port);
        return NULL;
    }

    speed_t speed = baud_to_speed(baudrate);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    /* 8N1，无流控 */
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CLOCAL | CREAD;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    /* 阻塞读取，直到有数据 */
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(port->fd, TCSANOW, &tty) != 0) {
        set_error(port, "tcsetattr failed");
        close(port->fd);
        free(port);
        return NULL;
    }

    tcflush(port->fd, TCIOFLUSH);
    return port;
}

void serial_port_close(serial_port_t *port) {
    if (!port) return;
    if (port->fd >= 0) close(port->fd);
    free(port);
}

int serial_port_read(serial_port_t *port, uint8_t *buf, size_t len, uint32_t timeout_ms) {
    if (!port || port->fd < 0) return -1;

    if (timeout_ms > 0) {
        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(port->fd, &rfds);
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int ret = select(port->fd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) return ret;  /* 0 = 超时，-1 = 错误 */
    }

    ssize_t n = read(port->fd, buf, len);
    if (n < 0) {
        set_error(port, "read failed");
        return -1;
    }
    return (int)n;
}

int serial_port_write(serial_port_t *port, const uint8_t *buf, size_t len) {
    if (!port || port->fd < 0) return -1;

    ssize_t n = write(port->fd, buf, len);
    if (n < 0) {
        set_error(port, "write failed");
        return -1;
    }
    /* 确保数据已写入设备 */
    tcdrain(port->fd);
    return (int)n;
}

bool serial_port_is_open(const serial_port_t *port) {
    return port && port->fd >= 0;
}

const char *serial_port_error(const serial_port_t *port) {
    return port ? port->error : "NULL port";
}

#endif /* _WIN32 */

/* ─────────────────── serial_port_fd ─────────────────── */

int serial_port_fd(const serial_port_t *port) {
#ifdef _WIN32
    (void)port;
    return -1; /* Windows 串口 HANDLE 不参与 select() */
#else
    return port ? port->fd : -1;
#endif
}

/* ─────────────────── 跨平台时钟 ─────────────────── */

uint64_t mesh_time_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}
