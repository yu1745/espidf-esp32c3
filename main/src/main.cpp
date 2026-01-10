#include "analysis.h"
#include "ble/ble.h"
#include "ble/def.hpp" //不要注释掉！！！
#include "decoy.hpp"
#include "def.h"
#include "dirent.h"
#include "err.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "esp_vfs.h"
#include "executor/executor_factory.hpp"
#include "freertos/task.h"
#include "globals.hpp"
#include "handyplug/handy_handler.hpp"
#include "http/def.hpp"
#include "led.hpp"
#include "mdns.hpp"
#include "sdkconfig.h"
#include "select_thread.hpp"
#include "setting.hpp"
#include "spiffs.h"
#include "stdio.h"
#include "uart/uart.h"
#include "uart/uart2.h"
#include "uart/usb_monitor.hpp"
#include "utils.hpp"
#include "voltage.hpp"
#include "wifi.hpp"

namespace {
const char *TAG = "main";

// LED初始化标志，用于ESP_ERROR_CHECK_WITH_LED宏
bool s_led_initialized = false;

} // namespace

extern "C" void app_main(void) {
  // vTaskDelay(pdMS_TO_TICKS(2000));
  ESP_LOGI(TAG, "app_main()\n");
  printBuildConfigOptions();
  esp_log_level_set("tcp_server", ESP_LOG_DEBUG);
  esp_log_level_set("udp_server", ESP_LOG_DEBUG);
  esp_log_level_set("rmt", ESP_LOG_DEBUG);

  // 初始化UART（不使用LED检查，因为LED还未初始化）
  esp_err_t ret = uart_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "UART初始化失败");
  }

  // 初始化UART2（不使用LED检查）
  if (CONFIG_ENABLE_UART2) {
    ret = uart2_init();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "UART2初始化失败");
    }
  } else {
    ESP_LOGI(TAG, "UART2功能未启用");
  }

  // 初始化SPIFFS
  ESP_ERROR_CHECK_WITH_LED(spiffs_init(), SPI_ERR);

  // 初始化LED - 显示初始化中状态（绿灯）
  if (CONFIG_ENABLE_LED) {
    ESP_ERROR_CHECK(led_init());
    s_led_initialized = true;
    Led::getInstance()->setSuccess(); // 绿灯常亮，表示初始化开始
    vTaskDelay(pdMS_TO_TICKS(500));   // 短暂延时让LED显示

    // 注册USB事件处理器（根据USB状态控制LED）
    // 注意：USB监控已在uart_init()中初始化，但定时器未启动
    esp_err_t ret = usb_monitor_register_handler();

    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Failed to register USB event handler: %s",
               esp_err_to_name(ret));
      // 不影响主流程，继续执行
    } else {
      ESP_LOGI(TAG, "USB event handler registered successfully");

      // 根据当前USB状态设置LED（在定时器启动前的初始状态）
      bool usb_connected = uart_is_usb_connected();
      if (usb_connected) {
        ESP_LOGI(TAG, "Initial USB state: connected - LED green solid");
        Led::getInstance()->setSuccess();
      } else {
        ESP_LOGI(TAG, "Initial USB state: disconnected - LED green blinking");
        Led::getInstance()->setBlink(1000); // 绿灯闪烁，1秒周期
      }
    }
  } else {
    ESP_LOGI(TAG, "LED功能未启用");
  }

  // 初始化设置模块（在SPIFFS初始化之后）
  try {
    setting_init();
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "设置模块初始化失败: %s", e.what());
    if (CONFIG_ENABLE_LED) {
      led_init();
      s_led_initialized = true;
      Led::getInstance()->showErrorCode(PIN_ERR);
    }
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  // 初始化Decoy（电压控制模块）
  if (CONFIG_ENABLE_DECOY) {
    ESP_ERROR_CHECK_WITH_LED(decoy_init(), PIN_ERR);
  } else {
    ESP_LOGI(TAG, "电压诱骗功能未启用");
  }

  // 初始化Voltage（电压监测模块）
  if (CONFIG_ENABLE_VOLTAGE) {
    ESP_ERROR_CHECK_WITH_LED(voltage_init(), PIN_ERR);
  } else {
    ESP_LOGI(TAG, "电压监测功能未启用");
  }

  // 初始化WiFi
  if (CONFIG_ENABLE_WIFI) {
    wifi_init();
    // WiFi初始化是异步的，这里暂时不使用错误检查
    // 实际的错误会在WiFi连接失败时通过其他方式显示
  } else {
    ESP_LOGI(TAG, "WiFi功能未启用");
  }

  // 初始化BLE
  if (CONFIG_ENABLE_BLE) {
    ble_init();
  } else {
    ESP_LOGI(TAG, "蓝牙功能未启用");
  }

  // 初始化Handy处理模块
  if (CONFIG_ENABLE_HANDY) {
    ESP_ERROR_CHECK_WITH_LED(handy_handler_init(), TCP_ERR);
    ESP_ERROR_CHECK_WITH_LED(handy_handler_start(), TCP_ERR);
  }

  // 初始化mDNS
  if (CONFIG_ENABLE_MDNS) {
    ESP_ERROR_CHECK_WITH_LED(init_mdns(), WIFI_ERR);
  } else {
    ESP_LOGI(TAG, "mDNS功能未启用");
  }

  // 初始化select线程
  ESP_ERROR_CHECK_WITH_LED(select_init(), TCP_ERR);
  ESP_ERROR_CHECK_WITH_LED(select_start(), TCP_ERR);

  // 初始化CPU分析模块
  ESP_ERROR_CHECK_WITH_LED(analysis_init(), SPI_ERR);

  // 打印注册的VFS路径(别删)
  // ESP_LOGI(TAG, "Registered VFS paths:");
  // esp_vfs_dump_registered_paths(stdout);

  // 使用VFS API列出根目录内容(别删)
  // list_root_directory();

  SettingWrapper setting;
  setting.loadFromFile();
  g_executor = ExecutorFactory::createExecutor(setting);

  // 所有模块初始化完成
  // 启动USB监控定时器（但USB监控要等到g_system_initialized=true后才控制LED）
  if (CONFIG_ENABLE_LED) {
    esp_err_t ret = usb_monitor_start();
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Failed to start USB monitor: %s", esp_err_to_name(ret));
    } else {
      ESP_LOGI(TAG, "USB monitor timer started");
    }
  }

  // 根据当前USB状态设置LED（最后的初始化状态）
  if (CONFIG_ENABLE_LED) {
    bool usb_connected = uart_is_usb_connected();
    if (usb_connected) {
      ESP_LOGI(TAG, "Final USB state: connected - LED green solid");
      Led::getInstance()->setSuccess();
    } else {
      ESP_LOGI(TAG, "Final USB state: disconnected - LED green blinking");
      Led::getInstance()->setBlink(1000);
    }
  }

  // 设置系统初始化完成标志，现在USB监控接管LED控制
  g_system_initialized = true;
  ESP_LOGI(TAG, "System fully initialized - USB monitor now controlling LED");

  ESP_LOGI(TAG, "app_main() completed successfully");
  return;
}