#include "executor/executor_factory.hpp"
#include "esp_log.h"
#include "executor/executor.hpp"
#include "executor/osr_executor.hpp"
#include "executor/sr6_executor.hpp"
#include "executor/sr6can_executor.hpp"
#include "executor/trrmax_executor.hpp"

static const char *TAG = "ExecutorFactory";

std::unique_ptr<Executor>
ExecutorFactory::createExecutor(const SettingWrapper &setting) {
  try {
    // 获取 servo mode 值
    int32_t mode = static_cast<int32_t>(setting->servo.MODE);

    ESP_LOGI(TAG, "创建 Executor, MODE: %d (%s)", mode, modeToString(mode));

    // 根据 mode 值创建对应的 executor
    switch (mode) {
    case 0:
      ESP_LOGI(TAG, "创建 OSR Executor (Multi-Axis Motion)");
      return std::make_unique<OSRExecutor>(setting);

    case 3:
      ESP_LOGI(TAG, "创建 SR6 Executor");
      return std::make_unique<SR6Executor>(setting);

    case 6:
      ESP_LOGI(TAG, "创建 TrRMax Executor");
      return std::make_unique<TrRMaxExecutor>(setting);

    case 8:
      ESP_LOGI(TAG, "创建 SR6CAN Executor");
      return std::make_unique<SR6CANExecutor>(setting);

    default:
      ESP_LOGE(TAG,
               "未知的 servo mode: %d, 支持的值为: 0(OSR), 3(SR6), 6(TrRMax), "
               "8(SR6CAN)",
               mode);
      return nullptr;
    }
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "创建 Executor 失败: %s", e.what());
    throw;
  }
}

const char *ExecutorFactory::modeToString(int32_t mode) {
  switch (mode) {
  case 0:
    return "OSR (Multi-Axis Motion)";
  case 3:
    return "SR6";
  case 6:
    return "TrRMax";
  case 8:
    return "SR6CAN";
  default:
    return "Unknown";
  }
}

std::vector<int32_t> ExecutorFactory::getSupportedModes() {
  return {0, 3, 6, 8};
}
