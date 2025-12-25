#include "actuator/actuator.hpp"
#include "actuator/ledc_actuator.hpp"
#include "actuator/rmt_actuator.hpp"
#include "analysis.h"
#include "ble/ble.h"
#include "ble/def.hpp"
#include "dirent.h"
#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "globals.h"
#include "http/def.hpp"
#include "led.hpp"
#include "main.pb.h"
#include "memory"
#include "pb_encode.h"
#include "queue_reader.h"
#include "select_thread.hpp"
#include "spiffs.h"
#include "stdio.h"
#include "uart/uart.h"
#include "wifi.hpp"
#include "mock_executor.hpp"

namespace {
const char* TAG = "main";

// Executor实例
std::unique_ptr<MockExecutor> g_executor;

// 使用VFS API实现ls功能
void list_root_directory() {
    ESP_LOGI(TAG, "Listing root directory (/):");

    DIR* dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open root directory");
        return;
    }

    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s", entry->d_name);
        count++;
    }

    closedir(dir);
    ESP_LOGI(TAG, "Total entries: %d", count);
}
}  // namespace

extern "C" void app_main(void) {
    // vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "app_main()\n");
    esp_log_level_set("tcp_server", ESP_LOG_DEBUG);
    esp_log_level_set("udp_server", ESP_LOG_DEBUG);

    // 初始化UART
    uart_init();

    // 初始化SPIFFS
    ESP_ERROR_CHECK(spiffs_init());

    // 初始化LED
    led_init();

    // 创建LED颜色测试任务
    xTaskCreate(
        [](void* arg) {
            // 运行LED颜色测试，持续30秒
            led_color_test(30);
            // 测试结束后删除任务
            vTaskDelete(NULL);
        },
        "led_test", 2048, NULL, 5, NULL);

    // 初始化WiFi（这会初始化LwIP栈）
    wifi_init();

    // 等待一小段时间确保LwIP完全初始化
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 初始化select线程
    ESP_ERROR_CHECK(select_init());
    ESP_ERROR_CHECK(select_start());

    // 初始化队列读取任务
    // ESP_ERROR_CHECK(queue_reader_init());
    // ESP_ERROR_CHECK(queue_reader_start());

    // 初始化CPU分析模块
    ESP_ERROR_CHECK(analysis_init());

    // 创建并初始化Executor
    try {
        g_executor = std::make_unique<MockExecutor>();
        ESP_LOGI(TAG, "MockExecutor created successfully");
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to create MockExecutor: %s", e.what());
        return;
    }

    ble_init();

    // 测试RMT执行器
    // actuator::testRMTActuator(8, 1,
    //                           1);  // GPIO 8, 通道 1, 1us per tick

    // 打印注册的VFS路径
    ESP_LOGI(TAG, "Registered VFS paths:");
    esp_vfs_dump_registered_paths(stdout);

    // 使用VFS API列出根目录内容
    list_root_directory();

    ESP_LOGI(TAG, "app_main() completed successfully");
    return;
}