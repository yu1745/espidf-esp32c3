#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "lwip/sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化TCP服务器
esp_err_t tcp_server_init(void);

// 停止TCP服务器
esp_err_t tcp_server_stop(void);

// 获取TCP服务器文件描述符
int tcp_server_get_fd(void);

// 处理新的TCP客户端连接
void tcp_server_handle_new_client(void);

// 处理TCP客户端数据
void tcp_server_handle_client_data(int client_fd);

// 关闭TCP客户端连接
void tcp_server_close_client(int client_fd);

// 获取TCP客户端数量
int tcp_server_get_client_count(void);

// 获取TCP客户端文件描述符数组
const int* tcp_server_get_client_fds(void);

// 设置LwIP初始化状态
void tcp_server_set_lwip_initialized(bool initialized);

// 向TCP客户端发送响应
esp_err_t tcp_server_send_response(int client_fd, const char* data, size_t len);

#ifdef __cplusplus
}
#endif