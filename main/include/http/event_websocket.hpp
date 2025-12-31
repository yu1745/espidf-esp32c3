#pragma once

#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化事件WebSocket系统
esp_err_t event_websocket_init(httpd_handle_t server);

// 停止事件WebSocket系统
esp_err_t event_websocket_stop(void);

// 发送事件到所有WebSocket客户端
esp_err_t event_websocket_broadcast(const char* message, size_t len);

#ifdef __cplusplus
}
#endif
