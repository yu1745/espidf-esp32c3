#pragma once

#include <mutex>
#include "tcode.hpp"
#include "setting.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

ESP_EVENT_DECLARE_BASE(EXECUTOR_EVENT);

// 统计窗口大小（秒）
#ifndef EXECUTOR_STATS_WINDOW_SECONDS
#define EXECUTOR_STATS_WINDOW_SECONDS 1
#endif

// 事件ID定义
typedef enum {
    EXECUTOR_EVENT_COMPUTE = 0,
    EXECUTOR_EVENT_EXECUTE = 1,
} executor_event_id_t;

// 事件数据结构
typedef struct {
    bool is_start;      // true表示开始，false表示结束
    int64_t timestamp;  // 时间戳（微秒）
} executor_event_data_t;

/**
 * @brief Executor抽象类
 * 用于处理TCode字符串的抽象执行器
 */
class Executor {
   public:
    /**
     * @brief 构造函数
     * @param setting 设置配置
     */
    explicit Executor(const SettingWrapper& setting);
    virtual ~Executor();

    /**
     * @brief 计算
     */
    virtual void compute() = 0;

    /**
     * @brief 执行
     */
    virtual void execute() = 0;

   protected:
    /**
     * @brief 执行器任务函数
     * @param arg 任务参数
     */
    static void taskFunc(void* arg);

    /**
     * @brief 解析器任务函数
     * @param arg 任务参数
     */
    static void parserTaskFunc(void* arg);

    /**
     * @brief 定时器回调函数
     * @param arg 回调参数
     */
    static void timerCallback(void* arg);

    /**
     * @brief 事件处理器
     * @param handler_arg 处理器参数
     * @param event_base 事件基
     * @param event_id 事件ID
     * @param event_data 事件数据
     */
    static void eventHandler(void* handler_arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data);

    /**
     * @brief 发送compute事件
     * @param is_start true表示开始，false表示结束
     */
    void sendComputeEvent(bool is_start);

    /**
     * @brief 发送execute事件
     * @param is_start true表示开始，false表示结束
     */
    void sendExecuteEvent(bool is_start);

    TCode tcode;  // 持有TCode类实例，用于处理TCode字符串
    SettingWrapper m_setting;  // 设置配置

    TaskHandle_t taskHandle;         // 执行任务句柄
    TaskHandle_t parserTaskHandle;  // 解析任务句柄
    SemaphoreHandle_t semaphore;     // 信号量，用于控制任务执行节拍
    esp_timer_handle_t timer;        // 定时器句柄
    bool taskRunning;                // 执行任务运行标志
    bool parserTaskRunning;          // 解析任务运行标志
    bool taskExecuting;              // 任务正在执行标志
    const char* TAG;                 // 日志标签
    std::mutex m_compute_mutex;      // 计算互斥锁
};
