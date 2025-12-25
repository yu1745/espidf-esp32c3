#include "select_thread.hpp"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "globals.h"
#include "tcp_server.hpp"
#include "udp_server.hpp"
#include "uart/uart.h"

namespace {
const char* TAG = "select_thread";
const int TCP_PORT = 8080;
const int UDP_PORT = 8081;
const int BUFFER_SIZE = 1024;
const int SELECT_TIMEOUT_MS = 1000;
const int MAX_CLIENTS = 5;

// 全局变量
static TaskHandle_t select_task_handle = NULL;
static bool select_thread_running = false;
}  // namespace

// Select线程主函数
static void select_thread_task(void* arg) {
    fd_set read_fds;
    int max_fd = -1;
    struct timeval timeout;

    ESP_LOGI(TAG, "Select thread started");

    while (select_thread_running) {
        // 清空文件描述符集合
        FD_ZERO(&read_fds);
        max_fd = -1;

        // 添加TCP服务器socket
        int tcp_fd = tcp_server_get_fd();
        if (tcp_fd >= 0) {
            FD_SET(tcp_fd, &read_fds);
            max_fd = (tcp_fd > max_fd) ? tcp_fd : max_fd;
        }

        // 添加UDP服务器socket
        int udp_fd = udp_server_get_fd();
        if (udp_fd >= 0) {
            FD_SET(udp_fd, &read_fds);
            max_fd = (udp_fd > max_fd) ? udp_fd : max_fd;
        }

        // 添加UART文件描述符
        int uart_fd = get_uart_fd();
        if (uart_fd >= 0) {
            FD_SET(uart_fd, &read_fds);
            max_fd = (uart_fd > max_fd) ? uart_fd : max_fd;
        }

        // 添加所有TCP客户端socket
        const int* client_fds = tcp_server_get_client_fds();
        int client_count = tcp_server_get_client_count();
        for (int i = 0; i < client_count; i++) {
            FD_SET(client_fds[i], &read_fds);
            max_fd = (client_fds[i] > max_fd) ? client_fds[i] : max_fd;
        }

        // 设置超时
        timeout.tv_sec = SELECT_TIMEOUT_MS / 1000;
        timeout.tv_usec = (SELECT_TIMEOUT_MS % 1000) * 1000;

        // 等待数据
        int result = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (result < 0) {
            if (errno != EINTR) {
                ESP_LOGE(TAG, "Select error: %s", strerror(errno));
            }
            // 延时1tick，避免饿死看门狗
            vTaskDelay(1);
            continue;
        } else if (result == 0) {
            // 超时，继续循环
            // 延时1tick，避免饿死看门狗
            vTaskDelay(1);
            continue;
        }

        // 检查TCP服务器是否有新连接
        if (tcp_fd >= 0 && FD_ISSET(tcp_fd, &read_fds)) {
            tcp_server_handle_new_client();
        }

        // 检查UDP服务器是否有数据
        if (udp_fd >= 0 && FD_ISSET(udp_fd, &read_fds)) {
            udp_server_handle_data();
        }

        // 检查UART是否有数据
        if (uart_fd >= 0 && FD_ISSET(uart_fd, &read_fds)) {
            // 调用UART模块处理数据
            uart_handle_data();
        }

        // 检查TCP客户端是否有数据
        for (int i = 0; i < client_count; i++) {
            if (FD_ISSET(client_fds[i], &read_fds)) {
                tcp_server_handle_client_data(client_fds[i]);
                // 由于tcp_server_handle_client_data可能会修改客户端数组，
                // 需要重新开始循环
                break;
            }
        }
        // 延时1tick，避免饿死看门狗
        // 如果读取积压，就会一直运行到这，不让出cpu会导致其他任务饿死
        vTaskDelay(1);
    }

    ESP_LOGI(TAG, "Select thread stopped");
    vTaskDelete(NULL);
}

// 实现公共接口函数
esp_err_t select_init(void) {
    // 创建全局接收队列
    if (global_rx_queue == NULL) {
        global_rx_queue = xQueueCreate(20, sizeof(data_packet_t*));
        if (global_rx_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create global RX queue");
            return ESP_FAIL;
        }
    }

    // 设置LwIP已初始化状态
    tcp_server_set_lwip_initialized(true);
    udp_server_set_lwip_initialized(true);

    // 初始化TCP服务器
    if (tcp_server_init() != ESP_OK) {
        return ESP_FAIL;
    }

    // 初始化UDP服务器
    if (udp_server_init() != ESP_OK) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Select thread initialized successfully");
    return ESP_OK;
}

esp_err_t select_start(void) {
    if (select_thread_running) {
        ESP_LOGW(TAG, "Select thread is already running");
        return ESP_ERR_INVALID_STATE;
    }

    select_thread_running = true;

    BaseType_t result = xTaskCreate(select_thread_task, "select_thread", 4096,
                                    NULL, 5, &select_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create select thread");
        select_thread_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Select thread started successfully");
    return ESP_OK;
}

esp_err_t select_stop(void) {
    if (!select_thread_running) {
        ESP_LOGW(TAG, "Select thread is not running");
        return ESP_ERR_INVALID_STATE;
    }

    select_thread_running = false;

    // 等待线程结束
    if (select_task_handle != NULL) {
        vTaskDelete(select_task_handle);
        select_task_handle = NULL;
    }

    // 停止TCP服务器
    tcp_server_stop();

    // 停止UDP服务器
    udp_server_stop();

    ESP_LOGI(TAG, "Select thread stopped successfully");
    return ESP_OK;
}

int get_tcp_server_fd(void) {
    return tcp_server_get_fd();
}

int get_udp_server_fd(void) {
    return udp_server_get_fd();
}