#include "http/websocket_server.h"
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "globals.h"

static const char* TAG = "websocket_server";

// 最大客户端连接数
#define MAX_CLIENTS 4

// WebSocket客户端连接数组
static websocket_client_t clients[MAX_CLIENTS] = {0};

// 查找空闲客户端槽
static int find_free_client_slot(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].is_connected) {
            return i;
        }
    }
    return -1;
}

// 查找客户端槽
static int find_client_slot(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == fd && clients[i].is_connected) {
            return i;
        }
    }
    return -1;
}

// WebSocket帧发送函数
static esp_err_t websocket_send_frame(httpd_req_t* req,
                                      int opcode,
                                      const char* payload,
                                      size_t payload_len) {
    httpd_ws_frame_t ws_pkt;

    // 清零结构体
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = opcode;
    ws_pkt.payload = (uint8_t*)payload;
    ws_pkt.len = payload_len;
    return httpd_ws_send_frame(req, &ws_pkt);
}

// WebSocket处理器
static esp_err_t websocket_handler(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket握手请求");
        // ESP-IDF会自动处理握手，因为is_websocket=true
        return ESP_OK;
    }

    // 获取客户端套接字
    int client_fd = httpd_req_to_sockfd(req);
    if (client_fd < 0) {
        ESP_LOGE(TAG, "获取客户端套接字失败");
        return ESP_FAIL;
    }

    uint8_t buf[WEBSOCKET_BUFFER_SIZE] = {0};
    httpd_ws_frame_t ws_pkt;

    // 清零结构体
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = 1;  // HTTPD_WS_TYPE_TEXT 在ESP-IDF v4.4中可能定义为1
    ws_pkt.payload = buf;

    // 接收WebSocket帧
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, WEBSOCKET_BUFFER_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "接收WebSocket帧失败");
        return ret;
    }

    // 查找或创建客户端槽
    int slot = find_client_slot(client_fd);
    if (slot == -1) {
        printf("client_fd: %d\n", client_fd);
        slot = find_free_client_slot();
        if (slot == -1) {
            ESP_LOGE(TAG, "已达到最大客户端连接数");
            return ESP_FAIL;
        }

        // 注册新客户端
        clients[slot].fd = client_fd;
        clients[slot].is_connected = true;

        ESP_LOGI(TAG, "新客户端 %d 连接已建立", client_fd);

        // 发送欢迎消息
        char welcome_msg[] = "欢迎连接到ESP32 WebSocket服务器!";
        websocket_send_frame(req, 1, welcome_msg,
                             strlen(welcome_msg));  // 1 = HTTPD_WS_TYPE_TEXT
    }

    // 处理接收到的消息
    if (ws_pkt.type == 1) {  // HTTPD_WS_TYPE_TEXT
        ESP_LOGI(TAG, "收到来自客户端 %d 的消息: %.*s", client_fd, ws_pkt.len,
                 ws_pkt.payload);

        // 回显消息
        char echo_msg[WEBSOCKET_BUFFER_SIZE];
        snprintf(echo_msg, sizeof(echo_msg), "回显: %.*s", ws_pkt.len,
                 ws_pkt.payload);
        websocket_send_frame(req, 1, echo_msg,
                             strlen(echo_msg));  // 1 = HTTPD_WS_TYPE_TEXT
    } else if (ws_pkt.type == 0x9) {             // HTTPD_WS_TYPE_PING
        ESP_LOGI(TAG, "收到来自客户端 %d 的PING帧", client_fd);
        // 自动回复PONG帧
        websocket_send_frame(req, 0xA, NULL, 0);  // 0xA = HTTPD_WS_TYPE_PONG
    } else if (ws_pkt.type == 0x8) {              // HTTPD_WS_TYPE_CLOSE
        ESP_LOGI(TAG, "收到来自客户端 %d 的CLOSE帧", client_fd);
        // 标记客户端为断开状态
        if (slot != -1) {
            clients[slot].is_connected = false;
        }
    }

    return ESP_OK;
}

// HTTP服务器配置
static const httpd_uri_t websocket_uri = {.uri = "/ws",
                                          .method = HTTP_GET,
                                          .handler = websocket_handler,
                                          .user_ctx = NULL,
                                          .is_websocket = true};

// 初始化WebSocket服务器
esp_err_t websocket_server_init(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "无效的HTTP服务器句柄");
        return ESP_FAIL;
    }
    // 注册WebSocket处理器
    esp_err_t ret = httpd_register_uri_handler(server, &websocket_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册WebSocket处理器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WebSocket服务器已注册到HTTP服务器");
    return ESP_OK;
}

// 停止WebSocket服务器
esp_err_t websocket_server_stop(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "HTTP服务器句柄为空");
        return ESP_FAIL;
    }

    // 断开所有客户端
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected) {
            clients[i].is_connected = false;
            close(clients[i].fd);
            ESP_LOGI(TAG, "客户端 %d 已断开连接", clients[i].fd);
        }
    }

    // 取消注册WebSocket处理器
    esp_err_t ret = httpd_unregister_uri_handler(server, websocket_uri.uri,
                                                 websocket_uri.method);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "取消注册WebSocket处理器失败: %s", esp_err_to_name(ret));
        // 继续执行
    }

    ESP_LOGI(TAG, "WebSocket服务器已停止");
    return ESP_OK;
}

// 广播消息到所有连接的客户端
esp_err_t websocket_broadcast(const char* message, size_t len) {
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected) {
            // 创建一个临时的请求结构用于发送
            httpd_req_t* req = NULL;
            // 注意：在ESP-IDF中，广播需要使用不同的方法
            // 这里我们使用一个简化的实现，实际应用中可能需要更复杂的处理
            ESP_LOGI(TAG, "广播消息到客户端 %d", clients[i].fd);

            // 由于ESP-IDF的WebSocket API需要httpd_req_t结构，
            // 我们需要在实际应用中维护一个更复杂的客户端管理
            // 这里暂时只记录日志
        }
    }

    return ret;
}

// 发送消息到特定客户端
esp_err_t websocket_send_to_client(httpd_handle_t server,
                                   int client_fd,
                                   const char* message,
                                   size_t len) {
    ESP_LOGI(TAG, "发送消息到客户端 %d: %.*s", client_fd, len, message);
    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t*)message, .len = len};
    esp_err_t ret =
        httpd_ws_send_frame_async(g_http_server, client_fd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送消息到客户端 %d 失败: %s", client_fd,
                 esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}
