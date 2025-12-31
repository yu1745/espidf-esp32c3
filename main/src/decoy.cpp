#include "decoy.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "setting.hpp"
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

    // 从Setting加载引脚配置
    if (!loadPinConfig()) {
      ESP_LOGE(TAG, "Failed to load pin configuration");
      throw std::runtime_error("Failed to load pin configuration");
    }

    // 初始化 GPIO
    if (!initGPIO()) {
      ESP_LOGE(TAG, "Failed to initialize GPIO");
      throw std::runtime_error("Failed to initialize GPIO");
    }

    m_initialized = true;
    ESP_LOGI(TAG, "Decoy initialized successfully");
  } catch (...) {
    ESP_LOGE(TAG, "Failed to construct Decoy");
    // 清理已分配的资源
    gpio_reset_pin(m_pin_mod1);
    gpio_reset_pin(m_pin_mod2);
    throw;
  }
}

Decoy::~Decoy() {
  ESP_LOGI(TAG, "~Decoy() deconstructing...");

  // 清理 GPIO
  gpio_reset_pin(m_pin_mod1);
  gpio_reset_pin(m_pin_mod2);
  ESP_LOGI(TAG, "Decoy destroyed");
}

bool Decoy::initGPIO() {
  esp_err_t ret;

  // 配置 MOD1 引脚
  ret = gpio_set_direction(m_pin_mod1, GPIO_MODE_OUTPUT);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MOD1 direction: %s", esp_err_to_name(ret));
    return false;
  }

  // 配置 MOD2 引脚
  ret = gpio_set_direction(m_pin_mod2, GPIO_MODE_OUTPUT);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MOD2 direction: %s", esp_err_to_name(ret));
    return false;
  }

  // 初始化为低电平（0V）
  gpio_set_level(m_pin_mod1, 0);
  gpio_set_level(m_pin_mod2, 0);

  ESP_LOGI(TAG, "GPIO initialized: MOD1=GPIO%d, MOD2=GPIO%d", m_pin_mod1,
           m_pin_mod2);
  return true;
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
  esp_err_t ret = gpio_set_level(m_pin_mod1, config[0]);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MOD1 level: %s", esp_err_to_name(ret));
    return false;
  }

  ret = gpio_set_level(m_pin_mod2, config[1]);
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

bool Decoy::loadPinConfig() {
  try {
    // 加载Setting配置文件
    SettingWrapper setting;
    setting.loadFromFile(SETTING_FILE_PATH);

    // 获取decoy引脚配置
    const Setting_Decoy &decoy_config = setting.get()->decoy;

    // 验证引脚配置
    if (decoy_config.MOD1_PIN < 0 || decoy_config.MOD2_PIN < 0) {
      ESP_LOGE(TAG, "Invalid pin configuration: MOD1=%d, MOD2=%d",
               decoy_config.MOD1_PIN, decoy_config.MOD2_PIN);
      return false;
    }

    // 设置成员变量
    m_pin_mod1 = static_cast<gpio_num_t>(decoy_config.MOD1_PIN);
    m_pin_mod2 = static_cast<gpio_num_t>(decoy_config.MOD2_PIN);

    ESP_LOGI(TAG, "Pin configuration loaded: MOD1=GPIO%d, MOD2=GPIO%d",
             m_pin_mod1, m_pin_mod2);

    return true;
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Failed to load pin configuration: %s", e.what());
    return false;
  }
}
