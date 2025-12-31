#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

// Handy队列类型定义
extern QueueHandle_t handy_queue;

/**
 * @brief 初始化Handy处理模块
 * @return ESP_OK成功，其他值失败
 */
esp_err_t handy_handler_init(void);

/**
 * @brief 启动Handy处理任务
 * @return ESP_OK成功，其他值失败
 */
esp_err_t handy_handler_start(void);

/**
 * @brief Handy任务处理函数
 * @param arg 任务参数
 */
void handy_task(void* arg);

#ifdef __cplusplus
}
#endif
