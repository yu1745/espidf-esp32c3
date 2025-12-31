#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <esp_http_server.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// WebSocket服务器配置
#define WEBSOCKET_SERVER_PORT 80
#define WEBSOCKET_BUFFER_SIZE 1024


// WebSocket连接结构
typedef struct {
    int fd;             // 套接字文件描述符
    bool is_connected;  // 连接状态
} websocket_client_t;

// 初始化WebSocket服务器，实际是注册URI处理器，并不改变http服务器的开关状态
esp_err_t websocket_server_init(httpd_handle_t server);

// 停止WebSocket服务器,实际是移除注册的URI处理器，并不停止HTTP服务器
esp_err_t websocket_server_stop(httpd_handle_t server);

// 发送消息到所有连接的客户端
esp_err_t websocket_broadcast(const char* message, size_t len);

// 发送消息到特定客户端
esp_err_t websocket_send_to_client(httpd_handle_t server,
                                   int client_fd,
                                   const char* message,
                                   size_t len);

#ifdef __cplusplus
}
#endif

#endif  // WEBSOCKET_SERVER_H