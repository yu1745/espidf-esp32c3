#include "twai/twai.hpp"
#include "esp_err.h"
#include "esp_log.h"

// 事件基础定义
ESP_EVENT_DEFINE_BASE(TWAI_EVENT);

// 静态成员变量定义
bool TWAI::initialized = false;
bool TWAI::started = false;
uint32_t TWAI::current_bitrate = 0;
TaskHandle_t TWAI::bus_load_task_handle = nullptr;
std::mutex TWAI::stats_mutex;

// 总线负载监控统计变量
uint32_t TWAI::rx_message_count = 0;
uint32_t TWAI::tx_message_count = 0;
uint32_t TWAI::rx_bytes_count = 0;
uint32_t TWAI::tx_bytes_count = 0;
uint64_t TWAI::rx_total_transmission_time_us = 0;
uint64_t TWAI::tx_total_transmission_time_us = 0;

static const char *TAG = "TWAI";

esp_err_t TWAI::init(int tx_pin, int rx_pin, uint32_t bitrate) {
  if (initialized) {
    ESP_LOGW(TAG, "TWAI already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing TWAI with TX=%d, RX=%d, bitrate=%lu", tx_pin,
           rx_pin, bitrate);

  // 配置TWAI通用配置
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)tx_pin, (gpio_num_t)rx_pin,
      TWAI_MODE_NORMAL /* TWAI_MODE_NO_ACK */); // TODO 仅测试用
  g_config.tx_queue_len = 15;
  // g_config.rx_queue_len = 20;
  // g_config.mode = TWAI_MODE_NO_ACK;

  // 配置TWAI时序配置
  twai_timing_config_t t_config;
  if (bitrate == 1000000) {
    t_config = TWAI_TIMING_CONFIG_1MBITS();
  } else if (bitrate == 800000) {
    t_config = TWAI_TIMING_CONFIG_800KBITS();
  } else if (bitrate == 500000) {
    t_config = TWAI_TIMING_CONFIG_500KBITS();
  } else if (bitrate == 250000) {
    t_config = TWAI_TIMING_CONFIG_250KBITS();
  } else if (bitrate == 125000) {
    t_config = TWAI_TIMING_CONFIG_125KBITS();
  } else if (bitrate == 100000) {
    t_config = TWAI_TIMING_CONFIG_100KBITS();
  } else if (bitrate == 50000) {
    t_config = TWAI_TIMING_CONFIG_50KBITS();
  } else if (bitrate == 25000) {
    t_config = TWAI_TIMING_CONFIG_25KBITS();
  } else {
    // 自定义波特率
    t_config.brp = 80; // 默认值，需要根据实际计算
    ESP_LOGW(TAG, "Using default timing for custom bitrate %lu", bitrate);
  }

  // 配置TWAI过滤器配置（接收所有消息）
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // 安装TWAI驱动
  esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(ret));
    return ret;
  }

  // 保存波特率供总线负载监控使用
  current_bitrate = bitrate;

  initialized = true;
  ESP_LOGI(TAG, "TWAI initialized successfully");
  return ESP_OK;
}

esp_err_t TWAI::start() {
  if (!initialized) {
    ESP_LOGE(TAG, "TWAI not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (started) {
    ESP_LOGW(TAG, "TWAI already started");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Starting TWAI driver");
  esp_err_t ret = twai_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(ret));
    return ret;
  }

  // 初始化TWAI事件系统（确保默认事件循环已创建）
  ret = initEventSystem();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize TWAI event system: %s", esp_err_to_name(ret));
    return ret;
  }

  started = true;

  // 创建总线负载监控任务
  xTaskCreate(busLoadMonitorTask, "twai_bus_load", 4096, nullptr, 5,
              &bus_load_task_handle);

  ESP_LOGI(TAG, "TWAI started successfully");
  return ESP_OK;
}

esp_err_t TWAI::stop() {
  if (!initialized || !started) {
    ESP_LOGW(TAG, "TWAI not started");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Stopping TWAI driver");
  esp_err_t ret = twai_stop();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to stop TWAI driver: %s", esp_err_to_name(ret));
    return ret;
  }

  // 停止总线负载监控任务
  if (bus_load_task_handle != nullptr) {
    vTaskDelete(bus_load_task_handle);
    bus_load_task_handle = nullptr;
  }

  started = false;
  ESP_LOGI(TAG, "TWAI stopped successfully");
  return ESP_OK;
}

esp_err_t TWAI::deinit() {
  if (!initialized) {
    ESP_LOGW(TAG, "TWAI not initialized");
    return ESP_OK;
  }

  // 先停止驱动
  if (started) {
    stop();
  }

  ESP_LOGI(TAG, "Deinitializing TWAI driver");
  esp_err_t ret = twai_driver_uninstall();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to deinitialize TWAI driver: %s",
             esp_err_to_name(ret));
    return ret;
  }

  initialized = false;
  ESP_LOGI(TAG, "TWAI deinitialized successfully");
  return ESP_OK;
}

esp_err_t TWAI::send(uint32_t id, const uint8_t *data, uint8_t data_len,
                     bool is_extended, int timeout_ms) {
  if (!initialized || !started) {
    ESP_LOGE(TAG, "TWAI not initialized or not started");
    return ESP_ERR_INVALID_STATE;
  }

  if (data_len > 8) {
    ESP_LOGE(TAG, "Data length too long: %d (max 8)", data_len);
    return ESP_ERR_INVALID_ARG;
  }

  // 准备发送帧
  twai_message_t tx_msg;
  tx_msg.identifier = id;
  // tx_msg.extd = is_extended;
  // tx_msg.rtr = false;  // 数据帧
  tx_msg.flags = is_extended ? TWAI_MSG_FLAG_EXTD : TWAI_MSG_FLAG_NONE;
  tx_msg.data_length_code = data_len;

  // 复制数据
  for (int i = 0; i < data_len; i++) {
    tx_msg.data[i] = data[i];
  }

  // 发送消息
  esp_err_t ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(timeout_ms));
  if (ret != ESP_OK) {
    // ESP_LOGE(TAG, "Failed to send TWAI message: %s",
    // esp_err_to_name(ret));
    return ret;
  }

  // 更新发送统计信息
  updateTxStats(data_len, is_extended);

  ESP_LOGD(TAG, "TWAI message sent: ID=0x%lx, Len=%d", id, data_len);
  return ESP_OK;
}

esp_err_t TWAI::receive(uint32_t *id, uint8_t *data, uint8_t *data_len,
                        bool *is_extended, int timeout_ms) {
  if (!initialized || !started) {
    ESP_LOGE(TAG, "TWAI not initialized or not started");
    return ESP_ERR_INVALID_STATE;
  }

  // 接收消息
  twai_message_t rx_msg;
  esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(timeout_ms));
  if (ret != ESP_OK) {
    if (ret != ESP_ERR_TIMEOUT) {
      ESP_LOGE(TAG, "Failed to receive TWAI message: %s", esp_err_to_name(ret));
    }
    return ret;
  }

  // 填充输出参数
  *id = rx_msg.identifier;
  *data_len = rx_msg.data_length_code;
  *is_extended = rx_msg.extd;

  // 复制数据
  memcpy(data, rx_msg.data, *data_len);

  // 更新接收统计信息
  updateRxStats(*data_len, *is_extended);

  ESP_LOGD(TAG, "TWAI message received: ID=0x%lx, Len=%d", *id, *data_len);
  return ESP_OK;
}

bool TWAI::is_initialized() { return initialized; }

bool TWAI::is_started() { return started; }

void TWAI::busLoadMonitorTask(void *arg) {
  ESP_LOGI(TAG, "TWAI总线负载监控任务启动");

  static const uint32_t MONITOR_INTERVAL_MS = 1000; // 每秒计算一次负载
  static uint32_t last_report_time = esp_timer_get_time() / 1000;

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));

    if (!initialized || !started) {
      continue;
    }

    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t time_diff_ms = current_time - last_report_time;
    uint64_t time_diff_us = (uint64_t)time_diff_ms * 1000;

    // 获取统计数据的快照
    std::lock_guard<std::mutex> lock(stats_mutex);

    uint32_t rx_msgs = rx_message_count;
    uint32_t tx_msgs = tx_message_count;
    uint32_t rx_bytes = rx_bytes_count;
    uint32_t tx_bytes = tx_bytes_count;
    uint64_t rx_time_us = rx_total_transmission_time_us;
    uint64_t tx_time_us = tx_total_transmission_time_us;

    // 重置统计计数器
    rx_message_count = 0;
    tx_message_count = 0;
    rx_bytes_count = 0;
    tx_bytes_count = 0;
    rx_total_transmission_time_us = 0;
    tx_total_transmission_time_us = 0;

    // 解锁后计算负载率
    lock.~lock_guard();

    // 计算负载率
    float rx_load = time_diff_us > 0 ? (float)rx_time_us / time_diff_us : 0.0f;
    float tx_load = time_diff_us > 0 ? (float)tx_time_us / time_diff_us : 0.0f;
    float total_load = rx_load + tx_load;

    // 限制在0.0-1.0范围内
    if (rx_load > 1.0f)
      rx_load = 1.0f;
    if (tx_load > 1.0f)
      tx_load = 1.0f;
    if (total_load > 1.0f)
      total_load = 1.0f;

    // 发布总线负载更新事件到ESP-IDF默认事件系统
    twai_bus_load_update_event_data_t event_data;
    event_data.rxLoad = rx_load;
    event_data.txLoad = tx_load;
    event_data.totalLoad = total_load;
    event_data.rxMessageCount = rx_msgs;
    event_data.txMessageCount = tx_msgs;
    event_data.rxBytesCount = rx_bytes;
    event_data.txBytesCount = tx_bytes;
    event_data.bitrate = current_bitrate;
    event_data.timestamp = current_time;
    esp_err_t ret = esp_event_post(TWAI_EVENT, TWAI_EVENT_BUS_LOAD_UPDATE, &event_data,
                                   sizeof(event_data), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "发送总线负载更新事件失败: %s", esp_err_to_name(ret));
    }

    last_report_time = current_time;
  }
}

void TWAI::updateTxStats(uint8_t data_len, bool is_extended) {
  if (!initialized || !started) {
    return;
  }

  std::lock_guard<std::mutex> lock(stats_mutex);

  tx_message_count++;
  tx_bytes_count += data_len;

  // 计算发送这条消息的传输时间
  uint32_t message_time_us = twai_util::calculate_message_transmission_time(
      current_bitrate, data_len, is_extended);

  tx_total_transmission_time_us += message_time_us;
}

void TWAI::updateRxStats(uint8_t data_len, bool is_extended) {
  if (!initialized || !started) {
    return;
  }

  std::lock_guard<std::mutex> lock(stats_mutex);

  rx_message_count++;
  rx_bytes_count += data_len;

  // 计算接收这条消息的传输时间
  uint32_t message_time_us = twai_util::calculate_message_transmission_time(
      current_bitrate, data_len, is_extended);

  rx_total_transmission_time_us += message_time_us;
}

esp_err_t TWAI::initEventSystem() {
  // 确保默认事件循环已创建
  esp_err_t ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    // ESP_ERR_INVALID_STATE表示已经创建过，不算错误
    ESP_LOGE(TAG, "创建默认事件循环失败: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "TWAI事件系统初始化成功（使用默认事件循环）");
  return ESP_OK;
}

esp_err_t TWAI::registerBusLoadHandler(esp_event_handler_t handler,
                                        void *handler_arg) {
  // 注册事件处理器到默认事件循环
  esp_err_t ret = esp_event_handler_register(
      TWAI_EVENT, TWAI_EVENT_BUS_LOAD_UPDATE, handler, handler_arg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "注册总线负载事件处理器失败: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "总线负载事件处理器注册成功");
  return ESP_OK;
}