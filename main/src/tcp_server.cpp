#include "tcp_server.hpp"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <mutex>
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "globals.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "select_thread.hpp"

namespace {
const char* TAG = "tcp_server";
const int TCP_PORT = 8080;
const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 5;

// 全局变量
static int tcp_server_fd = -1;
static int tcp_client_fds[MAX_CLIENTS];
static int tcp_client_count = 0;

// LwIP API互斥锁
static std::mutex lwip_mutex;
static bool lwip_initialized = false;
}  // namespace

// 初始化TCP服务器
esp_err_t tcp_server_init(void) {
    struct sockaddr_in server_addr;

    // 确保LwIP已初始化
    if (!lwip_initialized) {
        ESP_LOGE(TAG, "LwIP not initialized yet");
        return ESP_FAIL;
    }

    // 获取LwIP互斥锁
    std::lock_guard<std::mutex> lock(lwip_mutex);

    // 创建TCP socket
    tcp_server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_server_fd < 0) {
        ESP_LOGE(TAG, "Failed to create TCP socket: %s", strerror(errno));
        return ESP_FAIL;
    }

    // 设置socket选项，允许地址重用
    int opt = 1;
    setsockopt(tcp_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(TCP_PORT);

    if (bind(tcp_server_fd, (struct sockaddr*)&server_addr,
             sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind TCP socket: %s", strerror(errno));
        close(tcp_server_fd);
        tcp_server_fd = -1;
        return ESP_FAIL;
    }

    // 开始监听
    if (listen(tcp_server_fd, MAX_CLIENTS) < 0) {
        ESP_LOGE(TAG, "Failed to listen on TCP socket: %s", strerror(errno));
        close(tcp_server_fd);
        tcp_server_fd = -1;
        return ESP_FAIL;
    }

    // 初始化TCP客户端数组
    memset(tcp_client_fds, 0, sizeof(tcp_client_fds));
    tcp_client_count = 0;

    ESP_LOGI(TAG, "TCP server initialized on port %d", TCP_PORT);
    return ESP_OK;
}

// 停止TCP服务器
esp_err_t tcp_server_stop(void) {
    // 获取LwIP互斥锁
    std::lock_guard<std::mutex> lock(lwip_mutex);

    // 关闭所有TCP客户端连接
    for (int i = 0; i < tcp_client_count; i++) {
        close(tcp_client_fds[i]);
    }
    tcp_client_count = 0;

    // 关闭服务器socket
    if (tcp_server_fd >= 0) {
        close(tcp_server_fd);
        tcp_server_fd = -1;
    }

    ESP_LOGI(TAG, "TCP server stopped");
    return ESP_OK;
}

// 获取TCP服务器文件描述符
int tcp_server_get_fd(void) {
    return tcp_server_fd;
}

// 处理新的TCP客户端连接
void tcp_server_handle_new_client(void) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 获取LwIP互斥锁
    std::lock_guard<std::mutex> lock(lwip_mutex);

    int client_fd =
        accept(tcp_server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        ESP_LOGE(TAG, "Failed to accept TCP connection: %s", strerror(errno));
        return;
    }
    ESP_LOGI(TAG, "New TCP client connected: fd=%d", client_fd);

    if (tcp_client_count >= MAX_CLIENTS) {
        ESP_LOGW(TAG, "Maximum TCP clients reached, rejecting connection");
        close(client_fd);
        return;
    }

    tcp_client_fds[tcp_client_count++] = client_fd;
    ESP_LOGI(TAG, "New TCP client connected: fd=%d, count=%d", client_fd,
             tcp_client_count);
}

// 处理TCP客户端数据
void tcp_server_handle_client_data(int client_fd) {
    uint8_t buffer[BUFFER_SIZE];
    // ESP_LOGD(TAG, "TCP server handle client data: fd=%d", client_fd);
    // 获取LwIP互斥锁
    lwip_mutex.lock();

    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        // 连接关闭或错误
        ESP_LOGI(TAG, "TCP client disconnected: fd=%d", client_fd);
        close(client_fd);
        lwip_mutex.unlock();

        // 从客户端数组中移除
        for (int i = 0; i < tcp_client_count; i++) {
            if (tcp_client_fds[i] == client_fd) {
                // 将后面的元素前移
                for (int j = i; j < tcp_client_count - 1; j++) {
                    tcp_client_fds[j] = tcp_client_fds[j + 1];
                }
                tcp_client_count--;
                break;
            }
        }
        return;
    }

    lwip_mutex.unlock();

    // 创建数据包并发送到全局队列
    data_packet_t* packet = (data_packet_t*)malloc(sizeof(data_packet_t));
    if (packet) {
        packet->source = DATA_SOURCE_TCP;
        packet->client_fd = client_fd;
        packet->data = (uint8_t*)malloc(bytes_read);
        if (packet->data) {
            memcpy(packet->data, buffer, bytes_read);
            packet->length = bytes_read;

            if (xQueueSend(global_rx_queue, &packet, pdMS_TO_TICKS(100)) !=
                pdTRUE) {
                ESP_LOGW(TAG, "Failed to send TCP data to global queue");
                free(packet->data);
                free(packet);
            } else {
                // ESP_LOGI(TAG, "TCP data received: fd=%d, bytes=%d",
                // client_fd,
                //          bytes_read);
            }
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for TCP data");
            free(packet);
        }
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for TCP packet");
    }
}

// 关闭TCP客户端连接
void tcp_server_close_client(int client_fd) {
    // 获取LwIP互斥锁
    std::lock_guard<std::mutex> lock(lwip_mutex);

    close(client_fd);

    // 从客户端数组中移除
    for (int i = 0; i < tcp_client_count; i++) {
        if (tcp_client_fds[i] == client_fd) {
            // 将后面的元素前移
            for (int j = i; j < tcp_client_count - 1; j++) {
                tcp_client_fds[j] = tcp_client_fds[j + 1];
            }
            tcp_client_count--;
            break;
        }
    }
}

// 获取TCP客户端数量
int tcp_server_get_client_count(void) {
    return tcp_client_count;
}

// 获取TCP客户端文件描述符数组
const int* tcp_server_get_client_fds(void) {
    return tcp_client_fds;
}

// 设置LwIP初始化状态
void tcp_server_set_lwip_initialized(bool initialized) {
    lwip_initialized = initialized;
}