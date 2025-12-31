#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化UART2
esp_err_t uart2_init(void);

// 处理UART2数据读取和echo
void uart2_handle_data(void);

// 通过UART2发送数据
esp_err_t uart2_send_response(const char* data, size_t len);

// 获取UART2文件描述符供select使用
int get_uart2_fd(void);

#ifdef __cplusplus
}
#endif
