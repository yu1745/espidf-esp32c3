#include "globals.hpp"

// 全局服务器实例定义
httpd_handle_t g_http_server = NULL;

// 全局状态变量定义
bool g_wifi_connected = false;
bool g_http_server_running = false;
bool g_websocket_server_running = false;

// UART队列定义
QueueHandle_t uart_rx_queue = NULL;
QueueHandle_t uart_tx_queue = NULL;

// 全局接收队列定义
QueueHandle_t global_rx_queue = NULL;

// 全局Executor实例定义
std::unique_ptr<Executor> g_executor = nullptr;