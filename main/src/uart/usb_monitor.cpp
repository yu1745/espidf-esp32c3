#include "uart/usb_monitor.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "globals.hpp"
#include "led.hpp"
#include "uart/uart.h"
#include "utils.hpp"
#include <exception>

// USB事件基定义
ESP_EVENT_DEFINE_BASE(USB_MONITOR_EVENT);

// 内置USB事件处理器
// 根据USB连接状态控制LED：
// - USB未连接：绿灯闪烁（亮-灭）
// - USB已连接：绿灯常亮
// 注意：只有在系统初始化完成后（g_system_initialized == true）才控制LED
//       初始化期间，LED由main.cpp和其他模块控制
static void usb_event_handler_internal(void *handler_arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id, void *event_data) {
  if (event_base != USB_MONITOR_EVENT) {
    return;
  }

  // 只有在系统初始化完成后才控制LED
  if (!g_system_initialized) {
    ESP_LOGI("UsbMonitor", "System not fully initialized, USB monitor not controlling LED yet");
    return;
  }

  if (event_id == USB_MONITOR_EVENT_CONNECTED) {
    ESP_LOGI("UsbMonitor", "USB connected - LED green solid");
    Led::getInstance()->setSuccess(); // 绿灯常亮
  } else if (event_id == USB_MONITOR_EVENT_DISCONNECTED) {
    ESP_LOGI("UsbMonitor", "USB disconnected - LED green blinking");
    Led::getInstance()->setBlink(1000); // 绿灯闪烁（1秒周期）
  }
}

// C接口实现
extern "C" {

esp_err_t usb_monitor_init(void) {
  try {
    if (!UsbMonitor::getInstance().init()) {
      return ESP_FAIL;
    }
    ESP_LOGI("UsbMonitor", "USB monitor initialized successfully (timer not started)");
    return ESP_OK;
  } catch (const std::exception &e) {
    ESP_LOGE("UsbMonitor", "Failed to initialize USB monitor: %s", e.what());
    return ESP_FAIL;
  }
}

esp_err_t usb_monitor_start(void) {
  try {
    if (!UsbMonitor::getInstance().start()) {
      return ESP_FAIL;
    }
    ESP_LOGI("UsbMonitor", "USB monitor started successfully");
    return ESP_OK;
  } catch (const std::exception &e) {
    ESP_LOGE("UsbMonitor", "Failed to start USB monitor: %s", e.what());
    return ESP_FAIL;
  }
}

esp_err_t usb_monitor_register_handler(void) {
  // 确保默认事件循环已创建
  esp_err_t ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE("UsbMonitor", "Failed to create default event loop: %s",
             esp_err_to_name(ret));
    return ret;
  }

  // 注册内置事件处理器
  ret = esp_event_handler_register(USB_MONITOR_EVENT, ESP_EVENT_ANY_ID,
                                   &usb_event_handler_internal, nullptr);

  if (ret != ESP_OK) {
    ESP_LOGE("UsbMonitor", "Failed to register USB event handler: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI("UsbMonitor", "USB event handler registered successfully");
  return ESP_OK;
}

esp_err_t usb_monitor_deinit(void) {
  UsbMonitor::getInstance().deinit();
  return ESP_OK;
}

} // extern "C"

// C++类实现
UsbMonitor &UsbMonitor::getInstance() {
  static UsbMonitor instance;
  return instance;
}

UsbMonitor::~UsbMonitor() { deinit(); }

bool UsbMonitor::init() {
  if (m_initialized) {
    ESP_LOGW(TAG, "USB monitor already initialized");
    return true;
  }

  ESP_LOGI(TAG, "Initializing USB monitor...");

  // 确保默认事件循环已创建
  esp_err_t ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to create default event loop: %s",
             esp_err_to_name(ret));
    return false;
  }

  // 获取初始连接状态（不启动定时器）
  m_last_connected = uart_is_usb_connected();
  ESP_LOGI(TAG, "Initial USB connection state: %s",
           m_last_connected ? "connected" : "disconnected");

  m_initialized = true;
  ESP_LOGI(TAG, "USB monitor initialized (call start() to begin monitoring)");
  return true;
}

bool UsbMonitor::start() {
  if (!m_initialized) {
    ESP_LOGE(TAG, "USB monitor not initialized");
    return false;
  }

  if (m_timer != nullptr) {
    ESP_LOGW(TAG, "USB monitor timer already started");
    return true;
  }

  // 初始化并启动定时器
  if (!initTimer()) {
    ESP_LOGE(TAG, "Failed to initialize timer");
    return false;
  }

  ESP_LOGI(TAG, "USB monitor timer started");
  return true;
}

void UsbMonitor::deinit() {
  if (!m_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing USB monitor...");

  // 停止并删除定时器
  if (m_timer != nullptr) {
    if (xTimerStop(m_timer, pdMS_TO_TICKS(100)) == pdFAIL) {
      ESP_LOGW(TAG, "Failed to stop timer");
    }
    if (xTimerDelete(m_timer, pdMS_TO_TICKS(100)) == pdFAIL) {
      ESP_LOGW(TAG, "Failed to delete timer");
    }
    m_timer = nullptr;
  }

  m_initialized = false;
  ESP_LOGI(TAG, "USB monitor deinitialized");
}

bool UsbMonitor::initTimer() {
  // 创建软件定时器
  m_timer = xTimerCreate("usb_monitor_timer",              // 定时器名称
                         pdMS_TO_TICKS(CHECK_INTERVAL_MS), // 定时器周期（毫秒）
                         pdTRUE,                           // 自动重载
                         nullptr,                          // 定时器ID
                         timerCallback                     // 回调函数
  );

  if (m_timer == nullptr) {
    ESP_LOGE(TAG, "Failed to create timer");
    return false;
  }

  // 启动定时器
  if (xTimerStart(m_timer, 0) != pdPASS) {
    ESP_LOGE(TAG, "Failed to start timer");
    xTimerDelete(m_timer, 0);
    m_timer = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "Timer initialized: interval=%lu ms", CHECK_INTERVAL_MS);
  return true;
}

void UsbMonitor::timerCallback(TimerHandle_t timer) {
  UsbMonitor &instance = getInstance();
  instance.checkUsbConnection();
}

void UsbMonitor::checkUsbConnection() {
  // 检查当前USB连接状态
  bool current_connected = uart_is_usb_connected();

  // 状态未变化，直接返回
  if (current_connected == m_last_connected) {
    return;
  }

  // 状态发生变化
  m_last_connected = current_connected;

  // 准备事件数据
  usb_monitor_event_data_t event_data = {.connected = current_connected,
                                         .timestamp = esp_timer_get_time()};

  if (current_connected) {
    ESP_LOGI(TAG, "USB connected - posting event");

    // 发布连接事件到默认事件循环
    esp_err_t ret =
        esp_event_post(USB_MONITOR_EVENT, USB_MONITOR_EVENT_CONNECTED,
                       &event_data, sizeof(event_data), pdMS_TO_TICKS(100));

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to post USB connected event: %s",
               esp_err_to_name(ret));
    }
  } else {
    ESP_LOGI(TAG, "USB disconnected - posting event");

    // 发布断开事件到默认事件循环
    esp_err_t ret =
        esp_event_post(USB_MONITOR_EVENT, USB_MONITOR_EVENT_DISCONNECTED,
                       &event_data, sizeof(event_data), pdMS_TO_TICKS(100));

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to post USB disconnected event: %s",
               esp_err_to_name(ret));
    }
  }
}
