#pragma once

#include <esp_err.h>
#include <esp_event.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#ifdef __cplusplus
extern "C" {
#endif

// 事件基础声明
ESP_EVENT_DECLARE_BASE(ANALYSIS_EVENT);

// 事件ID定义
typedef enum {
    ANALYSIS_EVENT_CPU_USAGE = 0,
} analysis_event_id_t;

// CPU占用事件数据结构
typedef struct {
    float cpu_usage_percent;    // CPU使用率百分比
    size_t free_heap_size;      // 可用堆内存大小
    size_t total_heap_size;     // 总堆内存大小
} analysis_cpu_usage_event_data_t;

// 全局定时器声明
extern TimerHandle_t cpu_analysis_timer;

// CPU占用率相关变量声明
extern uint32_t idle_ticks_total;
extern uint32_t total_ticks;
extern float cpu_usage_percent;

// 函数声明
esp_err_t analysis_init(void);
void analysis_timer_callback(TimerHandle_t xTimer);
float get_cpu_usage(void);
void reset_cpu_stats(void);

// 事件相关函数声明
esp_err_t analysis_event_init(void);
void analysis_cpu_usage_event_handler(void* handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#ifdef __cplusplus
}
#endif