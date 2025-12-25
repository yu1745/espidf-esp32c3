#pragma once

#include <lwip/sockets.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// 数据包结构，用于标识数据来源
typedef enum {
    DATA_SOURCE_UART = 0,
    DATA_SOURCE_TCP = 1,
    DATA_SOURCE_UDP = 2
} data_source_t;

typedef struct {
    data_source_t source;
    int client_fd;  // 对于TCP/UDP，客户端socket文件描述符；对于UART，为-1
    uint8_t* data;
    size_t length;
} data_packet_t;

// 初始化select线程
esp_err_t select_init(void);

// 启动select线程
esp_err_t select_start(void);

// 停止select线程
esp_err_t select_stop(void);

// 获取UART文件描述符
// uart.cpp实现
int get_uart_fd(void);

// 获取TCP服务器文件描述符
// tcp_server.cpp实现
int get_tcp_server_fd(void);

// 获取UDP服务器文件描述符
// udp_server.cpp实现
int get_udp_server_fd(void);

#ifdef __cplusplus
}
#endif