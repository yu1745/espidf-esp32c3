#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t uart_init(void);

// 处理UART数据读取和echo
void uart_handle_data(void);

// 通过UART发送数据
esp_err_t uart_send_response(const char* data, size_t len);

// 检测USB串口是否连接
bool uart_is_usb_connected(void);

#ifdef __cplusplus
}
#endif