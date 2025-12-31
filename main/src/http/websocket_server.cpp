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
#include <cstring>
#include <vector>
#include <mutex>
#include <algorithm>
#include "freertos/queue.h"
#include "globals.hpp"
#include "select_thread.hpp"

static const char* TAG = "websocket_server";

// WebSocket客户端列表
static std::vector<websocket_client_t> clients_list;

// 客户端列表互斥锁
static std::mutex clients_mutex;

// 查找客户端迭代器
static std::vector<websocket_client_t>::iterator find_client_iterator(int fd) {
    return std::find_if(clients_list.begin(), clients_list.end(),
        [fd](const websocket_client_t& client) {
            return client.fd == fd && client.is_connected;
        });
}

// WebSocket帧发送函数
static esp_err_t websocket_send_frame(httpd_req_t* req,
                                      httpd_ws_type_t opcode,
                                      const char* payload,
                                      size_t payload_len) {
    httpd_ws_frame_t ws_pkt;

    // 清零结构体
    std::memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = opcode;
    ws_pkt.payload = (uint8_t*)payload;
    ws_pkt.len = payload_len;
    return httpd_ws_send_frame(req, &ws_pkt);
}

// WebSocket处理器
static esp_err_t websocket_handler(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket握手请求");
        // ESP-IDF会自动处理握手
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
    std::memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = buf;

    // 接收WebSocket帧
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, WEBSOCKET_BUFFER_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "接收WebSocket帧失败");
        return ret;
    }

    // 查找或创建客户端
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = find_client_iterator(client_fd);
        if (it == clients_list.end()) {
            // 创建新客户端
            websocket_client_t new_client;
            new_client.fd = client_fd;
            new_client.is_connected = true;
            clients_list.push_back(new_client);
            ESP_LOGI(TAG, "新客户端 %d 连接已建立", client_fd);
        }
    }

    // 处理接收到的消息
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        // ESP_LOGI(TAG, "收到来自客户端 %d 的消息: %.*s", client_fd, ws_pkt.len,
        //          ws_pkt.payload);

        // 创建数据包并发送到全局队列
        if (global_rx_queue != NULL && ws_pkt.len > 0) {
            data_packet_t* packet = (data_packet_t*)malloc(sizeof(data_packet_t));
            if (packet) {
                packet->source = DATA_SOURCE_WEBSOCKET;
                packet->client_fd = client_fd;
                packet->data = (uint8_t*)malloc(ws_pkt.len);
                if (packet->data) {
                    std::memcpy(packet->data, ws_pkt.payload, ws_pkt.len);
                    packet->length = ws_pkt.len;

                    packet->user_data = NULL;  // WebSocket不需要user_data
                    if (xQueueSend(global_rx_queue, &packet, pdMS_TO_TICKS(100)) !=
                        pdTRUE) {
                        ESP_LOGW(TAG, "Failed to send WebSocket data to global queue");
                        free(packet->data);
                        free(packet);
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to allocate memory for WebSocket data");
                    free(packet);
                }
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory for WebSocket packet");
            }
        }
    } else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
        ESP_LOGI(TAG, "收到来自客户端 %d 的PING帧", client_fd);
        // 自动回复PONG帧
        websocket_send_frame(req, HTTPD_WS_TYPE_PONG, NULL, 0);
    } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "收到来自客户端 %d 的CLOSE帧", client_fd);
        // 移除客户端
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = find_client_iterator(client_fd);
        if (it != clients_list.end()) {
            it->is_connected = false;
            clients_list.erase(it);
        }
    }

    return ESP_OK;
}

// HTTP服务器配置
static httpd_uri_t websocket_uri = {0};
static void init_websocket_uri(void) {
    websocket_uri.uri = "/ws";
    websocket_uri.method = HTTP_GET;
    websocket_uri.handler = websocket_handler;
    websocket_uri.user_ctx = NULL;
    websocket_uri.is_websocket = true;
}

// 初始化WebSocket服务器
extern "C" esp_err_t websocket_server_init(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "无效的HTTP服务器句柄");
        return ESP_FAIL;
    }
    // 初始化URI配置
    init_websocket_uri();
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
extern "C" esp_err_t websocket_server_stop(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "HTTP服务器句柄为空");
        return ESP_FAIL;
    }

    // 断开所有客户端
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto& client : clients_list) {
            if (client.is_connected) {
                client.is_connected = false;
                close(client.fd);
                ESP_LOGI(TAG, "客户端 %d 已断开连接", client.fd);
            }
        }
        clients_list.clear();
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
extern "C" esp_err_t websocket_broadcast(const char* message, size_t len) {
    esp_err_t ret = ESP_OK;

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients_list) {
        if (client.is_connected) {
            // 注意：在ESP-IDF中，广播需要使用不同的方法
            // 这里我们使用一个简化的实现，实际应用中可能需要更复杂的处理
            ESP_LOGI(TAG, "广播消息到客户端 %d", client.fd);

            // 由于ESP-IDF的WebSocket API需要httpd_req_t结构，
            // 我们需要在实际应用中维护一个更复杂的客户端管理
            // 这里暂时只记录日志
        }
    }

    return ret;
}

// 发送消息到特定客户端
extern "C" esp_err_t websocket_send_to_client(httpd_handle_t server,
                                   int client_fd,
                                   const char* message,
                                   size_t len) {
    ESP_LOGI(TAG, "发送消息到客户端 %d: %.*s", client_fd, len, message);
    httpd_ws_frame_t ws_pkt;
    std::memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)message;
    ws_pkt.len = len;
    esp_err_t ret =
        httpd_ws_send_frame_async(g_http_server, client_fd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送消息到客户端 %d 失败: %s", client_fd,
                 esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

