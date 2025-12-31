#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "lwip/sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化UDP服务器
esp_err_t udp_server_init(void);

// 停止UDP服务器
esp_err_t udp_server_stop(void);

// 获取UDP服务器文件描述符
int udp_server_get_fd(void);

// 处理UDP数据
void udp_server_handle_data(void);

// 设置LwIP初始化状态
void udp_server_set_lwip_initialized(bool initialized);

// 向UDP客户端发送响应
esp_err_t udp_server_send_response(int server_fd, const struct sockaddr_in* client_addr, const char* data, size_t len);

#ifdef __cplusplus
}
#endif