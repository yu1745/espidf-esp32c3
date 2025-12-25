#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void uart_init(void);

// 处理UART数据读取和echo
void uart_handle_data(void);

#ifdef __cplusplus
}
#endif