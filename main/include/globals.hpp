#pragma once

#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#ifdef __cplusplus
#include "executor/executor.hpp"
#include <memory>

extern "C" {
#endif

// 全局服务器实例声明
extern httpd_handle_t g_http_server;

// 全局状态变量
extern bool g_wifi_connected;
extern bool g_http_server_running;
extern bool g_websocket_server_running;

// UART队列声明
extern QueueHandle_t uart_rx_queue;
extern QueueHandle_t uart_tx_queue;

// 全局接收队列声明
extern QueueHandle_t global_rx_queue;

#ifdef __cplusplus

extern std::unique_ptr<Executor> g_executor;

}
#endif
