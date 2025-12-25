#include "executor.hpp"
#include "globals.h"
#include "select_thread.hpp"
#include <string>
#include <cstring>
#include <cstdlib>
#include "utils.hpp"
#include <stdexcept>
#include <memory>

/**
 * @brief Executor构造函数
 */
Executor::Executor()
    : taskHandle(nullptr),
      parserTaskHandle(nullptr),
      semaphore(nullptr),
      timer(nullptr),
      taskRunning(false),
      parserTaskRunning(false),
      taskExecuting(false),
      TAG("Executor") {
    try {
        // 创建二值信号量，初始值为0
        semaphore = xSemaphoreCreateBinary();
        if (semaphore == nullptr) {
            ESP_LOGE(TAG, "Failed to create semaphore");
            throw std::runtime_error("Failed to create semaphore");
        }

        // 配置定时器
        const esp_timer_create_args_t timer_args = {
            .callback = &Executor::timerCallback,
            .arg = this,
            .name = "executor_timer"};

        // 创建定时器
        esp_err_t ret = esp_timer_create(&timer_args, &timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(ret));
            throw std::runtime_error("Failed to create timer");
        }

        // 设置任务运行标志
        taskRunning = true;
        parserTaskRunning = true;

        // 创建解析任务（优先级较高，有命令就解析）
        xTaskCreate(&Executor::parserTaskFunc, "parser_task", 4096, this, 6,
                    &parserTaskHandle);

        if (parserTaskHandle == nullptr) {
            ESP_LOGE(TAG, "Failed to create parser task");
            throw std::runtime_error("Failed to create parser task");
        }

        // 创建执行任务（按节拍执行）
        xTaskCreate(&Executor::taskFunc, "executor_task", 4096, this, 5,
                    &taskHandle);

        if (taskHandle == nullptr) {
            ESP_LOGE(TAG, "Failed to create executor task");
            throw std::runtime_error("Failed to create executor task");
        }

        // 启动定时器，周期为20ms（50Hz）
        ret = esp_timer_start_periodic(timer, 20*1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(ret));
            throw std::runtime_error("Failed to start timer");
        }
        ESP_LOGI(TAG, "Executor initialized and started");
    } catch (...) {
        // 清理已经分配的资源
        if (taskHandle != nullptr) {
            vTaskDelete(taskHandle);
            taskHandle = nullptr;
        }
        if (parserTaskHandle != nullptr) {
            vTaskDelete(parserTaskHandle);
            parserTaskHandle = nullptr;
        }
        if (timer != nullptr) {
            esp_timer_delete(timer);
            timer = nullptr;
        }
        if (semaphore != nullptr) {
            vSemaphoreDelete(semaphore);
            semaphore = nullptr;
        }
        // 将捕获的异常再次向上抛出
        throw;
    }
}

/**
 * @brief Executor析构函数
 */
Executor::~Executor() {
    // 停止解析任务
    if (parserTaskRunning) {
        parserTaskRunning = false;
        if (parserTaskHandle != nullptr) {
            vTaskDelete(parserTaskHandle);
            parserTaskHandle = nullptr;
        }
    }

    // 停止执行任务
    if (taskRunning) {
        taskRunning = false;

        // 停止定时器
        if (timer != nullptr) {
            esp_err_t ret = esp_timer_stop(timer);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to stop timer: %s", esp_err_to_name(ret));
            }
        }

        // 释放信号量，以便任务能够退出
        if (semaphore != nullptr) {
            xSemaphoreGive(semaphore);
        }

        // 等待任务结束
        if (taskHandle != nullptr) {
            vTaskDelete(taskHandle);
            taskHandle = nullptr;
        }
    }

    if (timer != nullptr) {
        esp_timer_delete(timer);
        timer = nullptr;
    }

    if (semaphore != nullptr) {
        vSemaphoreDelete(semaphore);
        semaphore = nullptr;
    }

    ESP_LOGI(TAG, "Executor destroyed");
}



/**
 * @brief 执行器任务函数
 * 按定时器节拍执行（由子类实现具体执行逻辑）
 * @param arg 任务参数
 */
void Executor::taskFunc(void* arg) {
    Executor* executor = static_cast<Executor*>(arg);
    if (executor == nullptr) {
        return;
    }

    ESP_LOGI(executor->TAG, "Executor task started");

    while (executor->taskRunning) {
        // 等待信号量
        BaseType_t result = xSemaphoreTake(executor->semaphore, portMAX_DELAY);
        ESP_LOGD(executor->TAG, "Semaphore take result: %d", result);
        
        if (result == pdTRUE) {
            // 设置任务正在执行标志
            executor->taskExecuting = true;
            
            // 按节拍执行（由子类重写compute和execute方法实现具体逻辑）
            // 子类可以通过访问 executor->tcode 来获取解析后的数据
            // 通过虚函数表调用子类实现
            executor->compute();
            executor->execute();
            
            // 清除任务正在执行标志
            executor->taskExecuting = false;
        } else {
            // 超时，继续循环
            ESP_LOGD(executor->TAG, "Semaphore take timeout");
        }
    }

    ESP_LOGI(executor->TAG, "Executor task stopped");
    vTaskDelete(nullptr);
}

/**
 * @brief 解析器任务函数
 * 从global_rx_queue读取数据，解析后存储到tcode对象中
 * @param arg 任务参数
 */
void Executor::parserTaskFunc(void* arg) {
    Executor* self = static_cast<Executor*>(arg);
    if (self == nullptr) {
        return;
    }

    ESP_LOGI(self->TAG, "Parser task started, parserTaskRunning=%d", self->parserTaskRunning);

    while (self->parserTaskRunning) {
        // 不断从全局接收队列读取数据包，有命令就解析
        if (global_rx_queue != nullptr) {
            data_packet_t* packet = nullptr;
            if (xQueueReceive(global_rx_queue, &packet, portMAX_DELAY) == pdTRUE) {
                if (packet != nullptr && packet->data != nullptr && packet->length > 0) {
                    // 将数据转换为字符串
                    std::string tcodeStr(reinterpret_cast<const char*>(packet->data), 
                                         packet->length);
                    
                    // 移除可能的换行符和回车符
                    while (!tcodeStr.empty() && 
                           (tcodeStr.back() == '\n' || tcodeStr.back() == '\r')) {
                        tcodeStr.pop_back();
                    }
                    
                    if (!tcodeStr.empty()) {
                        // 使用tcode类解析TCode字符串
                        self->tcode.preprocess(tcodeStr);
                    }
                }
                
                // 释放数据包内存
                if (packet != nullptr) {
                    if (packet->data != nullptr) {
                        free(packet->data);
                    }
                    free(packet);
                }
            }
        } else {
            // 队列未初始化，短暂延时
            delay1();
        }
    }

    ESP_LOGI(self->TAG, "Parser task stopped");
    vTaskDelete(nullptr);
}

/**
 * @brief 定时器回调函数
 * @param arg 回调参数
 */
void Executor::timerCallback(void* arg) {
    Executor* executor = static_cast<Executor*>(arg);
    if (executor != nullptr) {
        // 检查任务是否正在执行
        if (executor->taskExecuting) {
            ESP_LOGW(executor->TAG, "Task execution exceeded one cycle!");
        }

        // 释放信号量，触发任务执行
        xSemaphoreGiveFromISR(executor->semaphore, nullptr);
    }
}

