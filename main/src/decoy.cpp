#include "decoy.hpp"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "stdexcept"


static const char *TAG = "Decoy";

// 静态单例实例指针
static Decoy *s_singleton_instance = nullptr;
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t decoy_init(void) {
  try {
    // ESP_LOGI(TAG, "decoy_init() called...");
    Decoy::getInstance();
    ESP_LOGI(TAG, "Decoy module initialized successfully");
    return ESP_OK;
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Decoy module初始化失败: %s", e.what());
    return ESP_FAIL;
  }
}
#ifdef __cplusplus
}
#endif

Decoy *Decoy::getInstance() {
  if (s_singleton_instance == nullptr) {
    s_singleton_instance = new Decoy();
  }
  return s_singleton_instance;
}

Decoy::Decoy() {
  try {
    ESP_LOGI(TAG, "Decoy() constructing...");

    // 初始化 GPIO
    if (!initGPIO()) {
      ESP_LOGE(TAG, "Failed to initialize GPIO");
      throw std::runtime_error("Failed to initialize GPIO");
    }

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
    ESP_LOGI(TAG, "Decoy initialized successfully");
  } catch (...) {
    ESP_LOGE(TAG, "Failed to construct Decoy");
    // 清理已分配的资源
    if (m_timer != nullptr) {
      xTimerDelete(m_timer, 0);
      m_timer = nullptr;
    }
    if (m_adc_handle != nullptr) {
      adc_oneshot_del_unit(m_adc_handle);
      m_adc_handle = nullptr;
    }
    gpio_reset_pin(PIN_MOD1);
    gpio_reset_pin(PIN_MOD2);
    throw;
  }
}

Decoy::~Decoy() {
  ESP_LOGI(TAG, "~Decoy() deconstructing...");

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

  // 清理 GPIO
  gpio_reset_pin(PIN_MOD1);
  gpio_reset_pin(PIN_MOD2);
  ESP_LOGI(TAG, "Decoy destroyed");
}

bool Decoy::initGPIO() {
  esp_err_t ret;

  // 配置 MOD1 引脚
  ret = gpio_set_direction(static_cast<gpio_num_t>(PIN_MOD1), GPIO_MODE_OUTPUT);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MOD1 direction: %s", esp_err_to_name(ret));
    return false;
  }

  // 配置 MOD2 引脚
  ret = gpio_set_direction(static_cast<gpio_num_t>(PIN_MOD2), GPIO_MODE_OUTPUT);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MOD2 direction: %s", esp_err_to_name(ret));
    return false;
  }

  // 初始化为低电平（0V）
  gpio_set_level(PIN_MOD1, 0);
  gpio_set_level(PIN_MOD2, 0);

  ESP_LOGI(TAG, "GPIO initialized: MOD1=GPIO%d, MOD2=GPIO%d", PIN_MOD1,
           PIN_MOD2);
  return true;
}

bool Decoy::initADC() {
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

bool Decoy::initTimer() {
  // 创建软件定时器
  m_timer = xTimerCreate("decoy_adc_timer",                // 定时器名称
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

void Decoy::timerCallback(TimerHandle_t timer) {
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
}

bool Decoy::setVoltage(VoltageLevel level) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // 使用枚举值作为数组下标访问配置
  int level_index = static_cast<int>(level);

  // 验证枚举值是否在有效范围内
  if (level_index < 0 || level_index >= 4) {
    ESP_LOGE(TAG, "Invalid voltage level: %d (supported: 0-3)", level_index);
    return false;
  }

  // 获取对应的MOD配置 [MOD1, MOD2, MOD3]
  const bool *config = VOLTAGE_CONFIGS[level_index];

  // 设置 MOD1 和 MOD2（MOD3通过电路上拉，无需控制）
  esp_err_t ret = gpio_set_level(PIN_MOD1, config[0]);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MOD1 level: %s", esp_err_to_name(ret));
    return false;
  }

  ret = gpio_set_level(PIN_MOD2, config[1]);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MOD2 level: %s", esp_err_to_name(ret));
    return false;
  }

  // 计算实际电压值（非线性）
  float target_vol;
  switch (level) {
  case VoltageLevel::V9:
    target_vol = 9.0f;
    break;
  case VoltageLevel::V12:
    target_vol = 12.0f;
    break;
  case VoltageLevel::V15:
    target_vol = 15.0f;
    break;
  default:
    target_vol = 0.0f;
    break;
  }

  ESP_LOGI(TAG, "Voltage set to %.1fV (MOD1=%d, MOD2=%d, MOD3=%d)", target_vol,
           config[0], config[1], config[2]);

  return true;
}

float Decoy::getVoltage() {
  std::lock_guard<std::mutex> lock(m_mutex);

  // 直接返回定时器回调中计算好的电压值
  ESP_LOGD(TAG, "Get voltage: %.2fV (ADC raw=%d)", m_voltage_value,
           m_adc_raw_value);

  return m_voltage_value;
}
