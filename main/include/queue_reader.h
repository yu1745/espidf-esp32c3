#pragma once

#include <esp_err.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化队列读取任务
esp_err_t queue_reader_init(void);

// 启动队列读取任务
esp_err_t queue_reader_start(void);

// 停止队列读取任务
esp_err_t queue_reader_stop(void);

#ifdef __cplusplus
}
#endif