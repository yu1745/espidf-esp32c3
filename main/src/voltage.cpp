#include "voltage.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "stdexcept"

static const char *TAG = "Voltage";

// 定义事件基
ESP_EVENT_DEFINE_BASE(VOLTAGE_EVENT);

// 静态单例实例指针
static Voltage *s_singleton_instance = nullptr;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t voltage_init(void) {
  try {
    // ESP_LOGI(TAG, "voltage_init() called...");
    Voltage::getInstance();
    ESP_LOGI(TAG, "Voltage module initialized successfully");
    return ESP_OK;
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Voltage module初始化失败: %s", e.what());
    return ESP_FAIL;
  }
}

#ifdef __cplusplus
}
#endif

Voltage *Voltage::getInstance() {
  if (s_singleton_instance == nullptr) {
    s_singleton_instance = new Voltage();
  }
  return s_singleton_instance;
}

Voltage::Voltage() {
  try {
    ESP_LOGI(TAG, "Voltage() constructing...");

    // 初始化 ADC
    if (!initADC()) {
      ESP_LOGE(TAG, "Failed to initialize ADC");
      throw std::runtime_error("Failed to initialize ADC");
    }

    // 初始化定时器
    if (!initTimer()) {
      ESP_LOGE(TAG, "Failed to initialize timer");
      throw std::runtime_error("Failed to initialize timer");
    }

    m_initialized = true;
    ESP_LOGI(TAG, "Voltage initialized successfully");
  } catch (...) {
    ESP_LOGE(TAG, "Failed to construct Voltage");
    // 清理已分配的资源
    if (m_timer != nullptr) {
      xTimerDelete(m_timer, 0);
      m_timer = nullptr;
    }
    if (m_adc_handle != nullptr) {
      adc_oneshot_del_unit(m_adc_handle);
      m_adc_handle = nullptr;
    }
    throw;
  }
}

Voltage::~Voltage() {
  ESP_LOGI(TAG, "~Voltage() deconstructing...");

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

  // 清理 ADC 资源
  if (m_adc_handle != nullptr) {
    if (adc_oneshot_del_unit(m_adc_handle) != ESP_OK) {
      ESP_LOGW(TAG, "Failed to delete ADC unit");
    }
    m_adc_handle = nullptr;
  }

  ESP_LOGI(TAG, "Voltage destroyed");
}

bool Voltage::initADC() {
  esp_err_t ret;

  // 创建 ADC oneshot unit
  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = ADC_UNIT_1,
      .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,  // 使用默认时钟源
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

  ret = adc_oneshot_new_unit(&init_config, &m_adc_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create ADC unit: %s", esp_err_to_name(ret));
    return false;
  }

  // 配置 ADC 通道
  adc_oneshot_chan_cfg_t config = {
      .atten = ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH,
  };

  ret = adc_oneshot_config_channel(m_adc_handle, ADC_CHANNEL, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(ret));
    adc_oneshot_del_unit(m_adc_handle);
    m_adc_handle = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "ADC initialized: Channel=%d, Atten=%d, Bitwidth=%d",
           ADC_CHANNEL, ADC_ATTEN, ADC_BITWIDTH);
  return true;
}

bool Voltage::initTimer() {
  // 创建软件定时器
  m_timer = xTimerCreate("voltage_adc_timer",               // 定时器名称
                         pdMS_TO_TICKS(TIMER_INTERVAL_MS), // 定时器周期（毫秒）
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

  ESP_LOGI(TAG, "Timer initialized: interval=%dms", TIMER_INTERVAL_MS);
  return true;
}

void Voltage::timerCallback(TimerHandle_t timer) {
  if (s_singleton_instance == nullptr) {
    return;
  }

  // 多次采样并求平均值以减少噪声
  int sum = 0;
  int raw_value = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    if (adc_oneshot_read(s_singleton_instance->m_adc_handle, ADC_CHANNEL, &raw_value) == ESP_OK) {
      sum += raw_value;
    }
  }
  int sensor_value = sum / ADC_SAMPLES;

  // 更新ADC原始值
  s_singleton_instance->m_adc_raw_value = sensor_value;

  // 计算电压值
  float voltage = sensor_value * (V_REF / ADC_RAW_MAX) * RESISTANCE_RATIO *
                  CALIBRATION_FACTOR;

  // 更新电压值
  s_singleton_instance->m_voltage_value = voltage;

  ESP_LOGD(TAG, "Timer callback: ADC raw=%d, voltage=%.2fV", sensor_value,
           voltage);

  // 静态计数器，每100次调用（1秒）发送一次事件
  static int callback_count = 0;
  callback_count++;

  if (callback_count >= 100) {  // 100 * 10ms = 1秒
    callback_count = 0;

    // 发送电压读数事件
    voltage_reading_event_data_t event_data = {
        .voltage = voltage,
        .adc_raw = sensor_value,
        .timestamp = esp_timer_get_time(),
    };

    esp_err_t ret = esp_event_post(VOLTAGE_EVENT, VOLTAGE_EVENT_READING,
                                   &event_data, sizeof(event_data),
                                   pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Failed to post voltage event: %s", esp_err_to_name(ret));
    }
  }
}

float Voltage::getVoltage() {
  std::lock_guard<std::mutex> lock(m_mutex);

  // 直接返回定时器回调中计算好的电压值
  ESP_LOGD(TAG, "Get voltage: %.2fV (ADC raw=%d)", m_voltage_value,
           m_adc_raw_value);

  return m_voltage_value;
}

int Voltage::getAdcRawValue() {
  std::lock_guard<std::mutex> lock(m_mutex);

  ESP_LOGD(TAG, "Get ADC raw value: %d", m_adc_raw_value);

  return m_adc_raw_value;
}
