#include "ble/def.hpp" //不要注释掉！！！
#include "analysis.h"
#include "ble/ble.h"
#include "dirent.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "esp_vfs.h"
#include "executor/executor_factory.hpp"
#include "freertos/task.h"
#include "http/def.hpp"
#include "led.hpp"
#include "mdns.hpp"
#include "select_thread.hpp"
#include "spiffs.h"
#include "stdio.h"
#include "uart/uart.h"
#include "wifi.hpp"
#include "setting.hpp"
#include "decoy.hpp"
#include "globals.hpp"


namespace {
const char* TAG = "main";
} 

extern "C" void app_main(void) {
    // vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "app_main()\n");
    esp_log_level_set("tcp_server", ESP_LOG_DEBUG);
    esp_log_level_set("udp_server", ESP_LOG_DEBUG);
    esp_log_level_set("rmt", ESP_LOG_DEBUG);

    // 初始化UART
    ESP_ERROR_CHECK(uart_init());

    // 初始化SPIFFS
    ESP_ERROR_CHECK(spiffs_init());

    // 初始化设置模块（在SPIFFS初始化之后）
    try {
        setting_init();
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "设置模块初始化失败: %s", e.what());
    }

    // 初始化LED
    ESP_ERROR_CHECK(led_init());

    // 初始化Decoy（电压控制模块）
    ESP_ERROR_CHECK(decoy_init());

    // // 创建LED颜色测试任务
    // xTaskCreate(
    //     [](void*) {
    //         // 运行LED颜色测试，持续30秒
    //         // led_color_test(30);
    //         while (true) {
    //             ESP_LOGI(TAG, "RED");
    //             led0_set_color_and_refresh(LED_COLOR_RED);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "GREEN");
    //             led0_set_color_and_refresh(LED_COLOR_GREEN);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "BLUE");
    //             led0_set_color_and_refresh(LED_COLOR_BLUE);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "YELLOW");
    //             led0_set_color_and_refresh(LED_COLOR_YELLOW);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "CYAN");
    //             led0_set_color_and_refresh(LED_COLOR_CYAN);
    //             vTaskDelay(pdMS_TO_TICKS(1000));    
    //             ESP_LOGI(TAG, "MAGENTA");
    //             led0_set_color_and_refresh(LED_COLOR_MAGENTA);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "WHITE");
    //             led0_set_color_and_refresh(LED_COLOR_WHITE);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "ORANGE");
    //             led0_set_color_and_refresh(LED_COLOR_ORANGE);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "PURPLE");
    //             led0_set_color_and_refresh(LED_COLOR_PURPLE);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //             ESP_LOGI(TAG, "PINK");
    //             led0_set_color_and_refresh(LED_COLOR_PINK);
    //             vTaskDelay(pdMS_TO_TICKS(1000));
    //         }
    //     },
    //     "led_test", 2048, NULL, 5, NULL);

    // 创建Decoy电压测试任务
    // xTaskCreate(
    //     [](void*) {
    //         ESP_LOGI(TAG, "Starting Decoy voltage test...");
    //         const VoltageLevel test_levels[] = {
    //             VoltageLevel::V9,
    //             VoltageLevel::V12,
    //             VoltageLevel::V15
    //         };
    //         const float test_voltages[] = {9.0f, 12.0f, 15.0f};
    //         for (int i = 0; i < 3; i++) {
    //             // 设置电压
    //             Decoy::getInstance()->setVoltage(test_levels[i]);
    //             vTaskDelay(pdMS_TO_TICKS(500));

    //             // 读取并显示实际电压
    //             float actual_voltage = Decoy::getInstance()->getVoltage();
    //             ESP_LOGI(TAG, "Set voltage: %.1fV, Actual voltage: %.2fV",
    //                      test_voltages[i], actual_voltage);

    //             vTaskDelay(pdMS_TO_TICKS(2000));
    //         }

    //         ESP_LOGI(TAG, "Decoy voltage test completed");
    //         vTaskDelete(NULL);
    //     },
    //     "decoy_test", 2048, NULL, 1, NULL);
    // }

    // 初始化WiFi（这会初始化LwIP栈）
    wifi_init();
    ble_init();
    
    // 初始化mDNS
    ESP_ERROR_CHECK(init_mdns());

    // 初始化select线程
    ESP_ERROR_CHECK(select_init());
    ESP_ERROR_CHECK(select_start());

    // 初始化CPU分析模块
    ESP_ERROR_CHECK(analysis_init());

    // 打印注册的VFS路径
    ESP_LOGI(TAG, "Registered VFS paths:");
    esp_vfs_dump_registered_paths(stdout);

    // 使用VFS API列出根目录内容
    // list_root_directory();

    SettingWrapper setting;
    setting.loadFromFile();
    g_executor = ExecutorFactory::createExecutor(setting);

    ESP_LOGI(TAG, "app_main() completed successfully");    
    return;
}