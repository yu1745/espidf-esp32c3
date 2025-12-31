#include "queue_reader.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "globals.hpp"
#include "select_thread.hpp"

static const char* TAG = "queue_reader";

// 全局变量
static TaskHandle_t queue_reader_task_handle = NULL;
static bool queue_reader_running = false;

// 队列读取任务主函数
static void queue_reader_task(void* arg) {
    data_packet_t* packet;

    ESP_LOGI(TAG, "Queue reader task started");

    while (queue_reader_running) {
        // 尝试从全局接收队列中获取数据包
        if (xQueueReceive(global_rx_queue, &packet, pdMS_TO_TICKS(100)) ==
            pdTRUE) {
            // 丢弃数据包，释放内存
            if (packet != NULL) {
                if (packet->data != NULL) {
                    free(packet->data);
                }
                // 释放UDP客户端地址（如果存在）
                if (packet->user_data != NULL) {
                    free(packet->user_data);
                }
                free(packet);

                // 可选：打印日志记录丢弃的数据包
                // ESP_LOGD(TAG, "Dropped packet from source %d",
                // packet->source);
            }
        }

        // 延时1tick，避免饿死看门狗
        vTaskDelay(1);
    }

    ESP_LOGI(TAG, "Queue reader task stopped");
    vTaskDelete(NULL);
}

// 实现公共接口函数
esp_err_t queue_reader_init(void) {
    // 检查全局队列是否已初始化
    if (global_rx_queue == NULL) {
        ESP_LOGE(TAG, "Global RX queue is not initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Queue reader initialized successfully");
    return ESP_OK;
}

esp_err_t queue_reader_start(void) {
    if (queue_reader_running) {
        ESP_LOGW(TAG, "Queue reader task is already running");
        return ESP_ERR_INVALID_STATE;
    }

    queue_reader_running = true;

    BaseType_t result = xTaskCreate(queue_reader_task, "queue_reader", 2048,
                                    NULL, 5, &queue_reader_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create queue reader task");
        queue_reader_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Queue reader task started successfully");
    return ESP_OK;
}

esp_err_t queue_reader_stop(void) {
    if (!queue_reader_running) {
        ESP_LOGW(TAG, "Queue reader task is not running");
        return ESP_ERR_INVALID_STATE;
    }

    queue_reader_running = false;

    // 等待任务结束
    if (queue_reader_task_handle != NULL) {
        vTaskDelete(queue_reader_task_handle);
        queue_reader_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Queue reader task stopped successfully");
    return ESP_OK;
}