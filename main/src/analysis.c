#include "analysis.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "globals.h"

static const char* TAG = "analysis";

// 事件基础定义
ESP_EVENT_DEFINE_BASE(ANALYSIS_EVENT);

// 全局定时器定义
TimerHandle_t cpu_analysis_timer = NULL;

// CPU占用率相关变量定义
uint32_t idle_ticks_total = 0;
uint32_t total_ticks = 0;
float cpu_usage_percent = 0.0f;

// 上一次的计数值
static uint32_t last_idle_ticks = 0;
static uint32_t last_total_ticks = 0;

esp_err_t analysis_init(void) {
    ESP_LOGI(TAG, "初始化CPU分析模块");

    // 初始化事件系统
    esp_err_t ret = analysis_event_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化事件系统失败");
        return ret;
    }

    // 重置统计信息
    reset_cpu_stats();

    // 创建定时器，每1秒执行一次
    cpu_analysis_timer =
        xTimerCreate("analysis_timer",        // 定时器名称
                     pdMS_TO_TICKS(1000),     // 定时器周期：1000ms = 1秒
                     pdTRUE,                  // 自动重载
                     NULL,                    // 定时器ID
                     analysis_timer_callback  // 回调函数
        );

    if (cpu_analysis_timer == NULL) {
        ESP_LOGE(TAG, "创建分析定时器失败");
        return ESP_FAIL;
    }

    // 启动定时器
    if (xTimerStart(cpu_analysis_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "启动分析定时器失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "CPU分析模块初始化完成");
    return ESP_OK;
}

void analysis_timer_callback(TimerHandle_t xTimer) {
    // 获取当前任务状态
    TaskHandle_t idle_task_handle;
    uint32_t current_time = 0;

    // 获取空闲任务句柄
    idle_task_handle = xTaskGetIdleTaskHandle();

    if (idle_task_handle != NULL) {
        // 使用栈内存存储任务状态信息
        TaskStatus_t task_status;

        // 获取空闲任务信息
        vTaskGetInfo(idle_task_handle, &task_status, pdTRUE, eInvalid);

        // 获取当前时间
        current_time = (u_int32_t)esp_timer_get_time();

        // 计算时间差和空闲时间差
        uint32_t time_diff = current_time - last_total_ticks;
        uint32_t idle_ticks_diff =
            task_status.ulRunTimeCounter - last_idle_ticks;
        ESP_LOGI(TAG, "time_diff: %d, idle_ticks_diff: %d", time_diff,
                 idle_ticks_diff);
        if (time_diff > 0) {
            // CPU使用率 = 100% - (空闲时间 / 总时间)
            cpu_usage_percent =
                100.0 * (1.0 - (float)idle_ticks_diff / (float)(time_diff));

            // 限制CPU使用率在0-100%之间
            if (cpu_usage_percent > 100.0) {
                cpu_usage_percent = 100.0;
            } else if (cpu_usage_percent < 0.0) {
                cpu_usage_percent = 0.0;
            }

            // 获取内存信息
            size_t free_heap_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
            size_t total_heap_size =
                heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

            // 创建CPU占用事件数据
            analysis_cpu_usage_event_data_t event_data = {
                .cpu_usage_percent = cpu_usage_percent,
                .free_heap_size = free_heap_size,
                .total_heap_size = total_heap_size};

            // 发送CPU占用事件
            esp_err_t ret = esp_event_post(
                ANALYSIS_EVENT, ANALYSIS_EVENT_CPU_USAGE, &event_data,
                sizeof(event_data), pdMS_TO_TICKS(100));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "发送CPU占用事件失败: %s", esp_err_to_name(ret));
            }
        }

        // 更新上一次的值
        last_idle_ticks = task_status.ulRunTimeCounter;
        last_total_ticks = current_time;
    } else {
        ESP_LOGE(TAG, "无法获取空闲任务句柄");
    }
}

float get_cpu_usage(void) {
    return cpu_usage_percent;
}

void reset_cpu_stats(void) {
    idle_ticks_total = 0;
    total_ticks = 0;
    cpu_usage_percent = 0.0f;
    last_idle_ticks = 0;
    last_total_ticks = 0;

    ESP_LOGI(TAG, "CPU统计信息已重置");
}

// 初始化事件系统
esp_err_t analysis_event_init(void) {
    // 创建默认事件循环
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "创建默认事件循环失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 注册事件处理器到默认事件循环
    ret = esp_event_handler_register(ANALYSIS_EVENT, ANALYSIS_EVENT_CPU_USAGE,
                                     analysis_cpu_usage_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册CPU占用事件处理器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "事件系统初始化完成");
    return ESP_OK;
}

// CPU占用事件处理器
void analysis_cpu_usage_event_handler(void* handler_arg,
                                      esp_event_base_t event_base,
                                      int32_t event_id,
                                      void* event_data) {
    if (event_base == ANALYSIS_EVENT && event_id == ANALYSIS_EVENT_CPU_USAGE) {
        analysis_cpu_usage_event_data_t* data =
            (analysis_cpu_usage_event_data_t*)event_data;

        ESP_LOGI(TAG,
                 "CPU占用事件 - CPU使用率: %.2f%%, 可用内存: %d bytes, 总内存: "
                 "%d bytes",
                 data->cpu_usage_percent, data->free_heap_size,
                 data->total_heap_size);
    }
}