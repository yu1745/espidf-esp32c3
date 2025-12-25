#include "udp_server.hpp"
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
const char* TAG = "udp_server";
const int UDP_PORT = 8081;
const int BUFFER_SIZE = 1024;

// 全局变量
static int udp_server_fd = -1;

// LwIP API互斥锁
static std::mutex lwip_mutex;
static bool lwip_initialized = false;
}  // namespace

// 初始化UDP服务器
esp_err_t udp_server_init(void) {
    struct sockaddr_in server_addr;

    // 确保LwIP已初始化
    if (!lwip_initialized) {
        ESP_LOGE(TAG, "LwIP not initialized yet");
        return ESP_FAIL;
    }

    // 获取LwIP互斥锁
    std::lock_guard<std::mutex> lock(lwip_mutex);

    // 创建UDP socket
    udp_server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_server_fd < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: %s", strerror(errno));
        return ESP_FAIL;
    }

    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(UDP_PORT);

    if (bind(udp_server_fd, (struct sockaddr*)&server_addr,
             sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP socket: %s", strerror(errno));
        close(udp_server_fd);
        udp_server_fd = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP server initialized on port %d", UDP_PORT);
    return ESP_OK;
}

// 停止UDP服务器
esp_err_t udp_server_stop(void) {
    // 获取LwIP互斥锁
    std::lock_guard<std::mutex> lock(lwip_mutex);

    // 关闭服务器socket
    if (udp_server_fd >= 0) {
        close(udp_server_fd);
        udp_server_fd = -1;
    }

    ESP_LOGI(TAG, "UDP server stopped");
    return ESP_OK;
}

// 获取UDP服务器文件描述符
int udp_server_get_fd(void) {
    return udp_server_fd;
}

// 处理UDP数据
void udp_server_handle_data(void) {
    uint8_t buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 获取LwIP互斥锁
    lwip_mutex.lock();

    int bytes_read = recvfrom(udp_server_fd, buffer, sizeof(buffer) - 1, 0,
                              (struct sockaddr*)&client_addr, &client_addr_len);

    lwip_mutex.unlock();

    if (bytes_read > 0) {
        // 创建数据包并发送到全局队列
        data_packet_t* packet = (data_packet_t*)malloc(sizeof(data_packet_t));
        if (packet) {
            ESP_LOGD(TAG, "UDP data received: bytes=%d", bytes_read);
            packet->source = DATA_SOURCE_UDP;
            packet->client_fd = udp_server_fd;
            packet->data = (uint8_t*)malloc(bytes_read);
            if (packet->data) {
                memcpy(packet->data, buffer, bytes_read);
                packet->length = bytes_read;

                if (xQueueSend(global_rx_queue, &packet, pdMS_TO_TICKS(100)) !=
                    pdTRUE) {
                    ESP_LOGW(TAG, "Failed to send UDP data to global queue");
                    free(packet->data);
                    free(packet);
                } else {
                    ESP_LOGI(TAG, "UDP data received: bytes=%d", bytes_read);
                }
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory for UDP data");
                free(packet);
            }
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for UDP packet");
        }
    } else {
        ESP_LOGE(TAG, "Failed to receive UDP data: %s", strerror(errno));
    }
}

// 设置LwIP初始化状态
void udp_server_set_lwip_initialized(bool initialized) {
    lwip_initialized = initialized;
}