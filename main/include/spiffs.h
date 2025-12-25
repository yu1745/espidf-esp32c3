#pragma once

#include "esp_err.h"

/**
 * @brief 初始化 SPIFFS 文件系统
 *
 * 该函数会注册并挂载 SPIFFS 分区到 VFS
 *
 * @return ESP_OK 成功
 * @return 其他 错误码
 */
esp_err_t spiffs_init(void);