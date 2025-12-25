#include "mock_executor.hpp"
#include "esp_log.h"

static const char* TAG = "MockExecutor";

/**
 * @brief MockExecutor构造函数
 */
MockExecutor::MockExecutor() : Executor() {
    // 调用父类构造函数，父类构造函数会初始化所有资源
    ESP_LOGI(TAG, "MockExecutor initialized");
}

/**
 * @brief 计算TCode命令
 */
void MockExecutor::compute() {
    // 这里可以实现具体的TCode计算逻辑
    // 可以通过访问 tcode 成员来获取解析后的数据
    // 例如：tcode.L0_current, tcode.R0_current 等
    ESP_LOGD(TAG, "Computing TCode");
}

/**
 * @brief 执行TCode命令
 */
void MockExecutor::execute() {
    // 这里可以实现具体的TCode执行逻辑
    // 可以通过访问 tcode 成员来获取解析后的数据
    // 例如：tcode.L0_current, tcode.R0_current 等
    ESP_LOGD(TAG, "Executing TCode");
}
