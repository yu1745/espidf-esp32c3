
#include "uart/uart.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "driver/uart_vfs.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "globals.h"
#include "select_thread.hpp"

namespace {
const char* TAG = "uart";
const int UART_BUF_SIZE = 1024;
const int UART_QUEUE_SIZE = 10;
const int UART_LINE_BUF_SIZE = 256;
const int UART_READ_BATCH_SIZE = 64;  // 批量读取大小
static int uart_fd = -1;              // UART文件描述符
}  // namespace

// USB串行JTAG初始化函数
void uart_init() {
    // 创建队列
    uart_rx_queue = xQueueCreate(UART_QUEUE_SIZE, sizeof(uint8_t*));
    uart_tx_queue = xQueueCreate(UART_QUEUE_SIZE, sizeof(uint8_t*));

    if (uart_rx_queue == NULL || uart_tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create UART queues\n");
        return;
    }

    // 配置USB串行JTAG驱动
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .tx_buffer_size = UART_BUF_SIZE * 2,
        .rx_buffer_size = UART_BUF_SIZE * 2,
    };

    // 安装USB串行JTAG驱动
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));

    // 注册VFS驱动以支持文件描述符操作
    usb_serial_jtag_vfs_use_driver();

    // 打开USB串行JTAG设备文件以获取文件描述符
    uart_fd = open("/dev/usbserjtag", O_RDWR | O_NONBLOCK);
    if (uart_fd < 0) {
        ESP_LOGE(TAG, "Failed to open USB serial JTAG device: %s",
                 strerror(errno));
    } else {
        ESP_LOGI(TAG, "USB serial JTAG file descriptor: %d", uart_fd);
    }

    // 创建任务
    // xTaskCreate(uart_rx_task, "usb_serial_jtag_rx_task", 4096, NULL, 5,
    // NULL); xTaskCreate(uart_tx_task, "usb_serial_jtag_tx_task", 4096, NULL,
    // 5, NULL);

    printf("USB serial JTAG initialized successfully\n");
}

// 获取UART文件描述符供select使用
int get_uart_fd(void) {
    return uart_fd;
}

// 处理UART数据读取和echo
void uart_handle_data(void) {
    if (uart_fd < 0) {
        ESP_LOGE(TAG, "UART file descriptor is invalid");
        return;
    }

    uint8_t buffer[UART_BUF_SIZE];
    ssize_t bytes_read = read(uart_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // 确保字符串结束
        ESP_LOGI(TAG, "UART received %zd bytes: %s", bytes_read, buffer);

        // 将数据写回UART (echo)
        ssize_t bytes_written = write(uart_fd, buffer, bytes_read);
        if (bytes_written < 0) {
            ESP_LOGE(TAG, "Failed to write to UART: %s", strerror(errno));
        } else {
            ESP_LOGI(TAG, "UART echoed %zd bytes", bytes_written);
        }

        // 可选：将数据添加到全局接收队列供其他模块使用
        data_packet_t* packet = (data_packet_t*)malloc(sizeof(data_packet_t));
        if (packet != NULL) {
            packet->source = DATA_SOURCE_UART;
            packet->client_fd = -1;  // UART没有客户端文件描述符
            packet->data = (uint8_t*)malloc(bytes_read);
            if (packet->data != NULL) {
                memcpy(packet->data, buffer, bytes_read);
                packet->length = bytes_read;

                if (xQueueSend(global_rx_queue, &packet, pdMS_TO_TICKS(10)) !=
                    pdTRUE) {
                    ESP_LOGW(TAG, "Failed to send UART data to global queue");
                    free(packet->data);
                    free(packet);
                }
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory for UART data");
                free(packet);
            }
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for UART packet");
        }
    } else if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGE(TAG, "UART read error: %s", strerror(errno));
        }
    }
}