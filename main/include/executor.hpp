#pragma once

#include <string>
#include "tcode.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/**
 * @brief Executor抽象类
 * 用于处理TCode字符串的抽象执行器
 */
class Executor {
   public:
    Executor();
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


    TCode tcode;  // 持有TCode类实例，用于处理TCode字符串

    TaskHandle_t taskHandle;         // 执行任务句柄
    TaskHandle_t parserTaskHandle;  // 解析任务句柄
    SemaphoreHandle_t semaphore;     // 信号量，用于控制任务执行节拍
    esp_timer_handle_t timer;        // 定时器句柄
    bool taskRunning;                // 执行任务运行标志
    bool parserTaskRunning;          // 解析任务运行标志
    bool taskExecuting;              // 任务正在执行标志
    const char* TAG;                 // 日志标签
};
