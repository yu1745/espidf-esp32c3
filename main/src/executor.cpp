#include "executor/executor.hpp"
#include "esp_event.h"
#include "globals.hpp"
#include "http/websocket_server.h"
#include "select_thread.hpp"
#include "tcp_server.hpp"
#include "uart/uart.h"
#include "udp_server.hpp"
#include "utils.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

// 定义事件基
ESP_EVENT_DEFINE_BASE(EXECUTOR_EVENT);

/**
 * @brief Executor构造函数
 * @param setting 设置配置
 */
Executor::Executor(const SettingWrapper &setting)
    : m_setting(setting), taskHandle(nullptr), parserTaskHandle(nullptr),
      semaphore(nullptr), timer(nullptr), taskRunning(false),
      parserTaskRunning(false), taskExecuting(false), TAG("Executor") {
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
        .dispatch_method = ESP_TIMER_TASK,
        .name = "executor_timer",
        .skip_unhandled_events = false};

    // 创建定时器
    esp_err_t ret = esp_timer_create(&timer_args, &timer);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(ret));
      throw std::runtime_error("Failed to create timer");
    }

    // 设置任务运行标志
    taskRunning = true;
    parserTaskRunning = true;

    // 确保默认事件循环已创建
    esp_err_t event_ret = esp_event_loop_create_default();
    if (event_ret != ESP_OK && event_ret != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "Failed to create default event loop: %s",
               esp_err_to_name(event_ret));
      throw std::runtime_error("Failed to create default event loop");
    }

    // 注册事件处理器
    event_ret = esp_event_handler_register(EXECUTOR_EVENT, ESP_EVENT_ANY_ID,
                                           &Executor::eventHandler, this);
    if (event_ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register event handler: %s",
               esp_err_to_name(event_ret));
      throw std::runtime_error("Failed to register event handler");
    }

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

    // 启动定时器，频率为setting.servo.A_SERVO_PWM_FREQ
    ret = esp_timer_start_periodic(timer,
                                   1000000 / m_setting->servo.A_SERVO_PWM_FREQ);
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
void Executor::taskFunc(void *arg) {
  Executor *executor = static_cast<Executor *>(arg);
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

      // 发送compute开始事件并执行
      executor->sendComputeEvent(true);
      executor->compute();
      executor->sendComputeEvent(false);

      // 发送execute开始事件并执行
      executor->sendExecuteEvent(true);
      executor->execute();
      executor->sendExecuteEvent(false);

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
void Executor::parserTaskFunc(void *arg) {
  Executor *self = static_cast<Executor *>(arg);
  if (self == nullptr) {
    return;
  }

  ESP_LOGI(self->TAG, "Parser task started, parserTaskRunning=%d",
           self->parserTaskRunning);

  while (self->parserTaskRunning) {
    // 不断从全局接收队列读取数据包，有命令就解析
    if (global_rx_queue != nullptr) {
      data_packet_t *packet = nullptr;
      if (xQueueReceive(global_rx_queue, &packet, portMAX_DELAY) == pdTRUE) {
        if (packet != nullptr && packet->data != nullptr &&
            packet->length > 0) {
          // 检查是否是 'D1' 命令
          bool is_d1_command = false;
          if (packet->length >= 2 && packet->data[0] == 'D' &&
              packet->data[1] == '1') {
            is_d1_command = true;
            // 检查是否只有 'D1'（可能后面有换行符或回车符）
            // bool only_d1 = true;
            // for (size_t i = 2; i < packet->length; i++) {
            //   if (packet->data[i] != '\n' && packet->data[i] != '\r') {
            //     only_d1 = false;
            //     break;
            //   }
            // }
            // if (only_d1) {
            //   is_d1_command = true;
            // }
          }

          // 如果是 'D1' 命令，发送响应
          if (is_d1_command) {
            const char *response = "TCode v0.3\n";
            size_t response_len = strlen(response);

            if (packet->source == DATA_SOURCE_TCP && packet->client_fd >= 0) {
              // TCP 响应
              esp_err_t ret = tcp_server_send_response(packet->client_fd,
                                                       response, response_len);
              if (ret != ESP_OK) {
                ESP_LOGE(self->TAG,
                         "Failed to send TCP response for D1 command");
              }
            } else if (packet->source == DATA_SOURCE_WEBSOCKET &&
                       packet->client_fd >= 0) {
              // WebSocket 响应
              esp_err_t ret = websocket_send_to_client(
                  g_http_server, packet->client_fd, response, response_len);
              if (ret != ESP_OK) {
                ESP_LOGE(self->TAG,
                         "Failed to send WebSocket response for D1 command");
              }
            } else if (packet->source == DATA_SOURCE_UDP &&
                       packet->user_data != nullptr) {
              // UDP 响应，需要客户端地址
              struct sockaddr_in *client_addr =
                  static_cast<struct sockaddr_in *>(packet->user_data);
              esp_err_t ret = udp_server_send_response(
                  packet->client_fd, client_addr, response, response_len);
              if (ret != ESP_OK) {
                ESP_LOGE(self->TAG,
                         "Failed to send UDP response for D1 command");
              }
            } else if (packet->source == DATA_SOURCE_UART) {
              // UART 响应，直接通过UART发送
              esp_err_t ret = uart_send_response(response, response_len);
              if (ret != ESP_OK) {
                ESP_LOGE(self->TAG,
                         "Failed to send UART response for D1 command");
              }
            }
            // BLE 等没有有效 client_fd 的，直接忽略（不发送响应）
          }

          // 如果不是 'D1' 命令，或者没有有效 client_fd，继续正常处理
          if (!is_d1_command) {
            // 将数据转换为字符串
            std::string tcodeStr(reinterpret_cast<const char *>(packet->data),
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
        }

        // 释放数据包内存
        if (packet != nullptr) {
          if (packet->data != nullptr) {
            free(packet->data);
          }
          // 释放UDP客户端地址（如果存在）
          if (packet->user_data != nullptr) {
            free(packet->user_data);
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
void Executor::timerCallback(void *arg) {
  Executor *executor = static_cast<Executor *>(arg);
  if (executor != nullptr) {
    // 检查任务是否正在执行
    if (executor->taskExecuting) {
      ESP_LOGW(executor->TAG, "Task execution exceeded one cycle!");
    }

    // 释放信号量，触发任务执行
    xSemaphoreGiveFromISR(executor->semaphore, nullptr);
  }
}

// 统计信息结构
struct ExecutorStats {
  // Compute统计
  int64_t compute_total_duration; // 累计耗时（微秒）
  int64_t compute_sum_squares;    // 耗时平方和（用于计算方差）
  int64_t compute_max_duration;   // 最大耗时（微秒）
  uint32_t compute_count;         // 执行次数

  // Execute统计
  int64_t execute_total_duration; // 累计耗时（微秒）
  int64_t execute_sum_squares;    // 耗时平方和（用于计算方差）
  int64_t execute_max_duration;   // 最大耗时（微秒）
  uint32_t execute_count;         // 执行次数

  // 时间窗口管理
  int64_t window_start_time; // 窗口开始时间（微秒）
  int64_t last_print_time;   // 上次打印时间（微秒）

  // 当前执行开始时间
  int64_t compute_start_time;
  int64_t execute_start_time;
};

/**
 * @brief 事件处理器
 * 用于计时compute和execute的执行时间，统计平均耗时和执行频率
 * @param handler_arg 处理器参数
 * @param event_base 事件基
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
void Executor::eventHandler(void *handler_arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
  Executor *executor = static_cast<Executor *>(handler_arg);
  if (executor == nullptr || event_data == nullptr) {
    return;
  }

  executor_event_data_t *data =
      static_cast<executor_event_data_t *>(event_data);

  // 使用静态变量存储统计信息
  static ExecutorStats stats = {.compute_total_duration = 0,
                                .compute_sum_squares = 0,
                                .compute_max_duration = 0,
                                .compute_count = 0,
                                .execute_total_duration = 0,
                                .execute_sum_squares = 0,
                                .execute_max_duration = 0,
                                .execute_count = 0,
                                .window_start_time = 0,
                                .last_print_time = 0,
                                .compute_start_time = 0,
                                .execute_start_time = 0};

  if (event_base == EXECUTOR_EVENT) {
    int64_t current_time = data->timestamp;

    // 初始化窗口开始时间
    if (stats.window_start_time == 0) {
      stats.window_start_time = current_time;
      stats.last_print_time = current_time;
    }

    if (event_id == EXECUTOR_EVENT_COMPUTE) {
      if (data->is_start) {
        stats.compute_start_time = current_time;
      } else {
        // 计算耗时并累加
        int64_t duration = current_time - stats.compute_start_time;
        stats.compute_total_duration += duration;
        stats.compute_sum_squares += duration * duration; // 累加平方
        if (duration > stats.compute_max_duration) {
          stats.compute_max_duration = duration; // 更新最大值
        }
        stats.compute_count++;
      }
    } else if (event_id == EXECUTOR_EVENT_EXECUTE) {
      if (data->is_start) {
        stats.execute_start_time = current_time;
      } else {
        // 计算耗时并累加
        int64_t duration = current_time - stats.execute_start_time;
        stats.execute_total_duration += duration;
        stats.execute_sum_squares += duration * duration; // 累加平方
        if (duration > stats.execute_max_duration) {
          stats.execute_max_duration = duration; // 更新最大值
        }
        stats.execute_count++;
      }
    }

    // 检查是否需要打印统计信息（每秒一次）
    int64_t time_since_last_print = current_time - stats.last_print_time;
    if (time_since_last_print >= EXECUTOR_STATS_WINDOW_SECONDS * 1000000LL) {
      // 计算窗口时间（秒）
      int64_t window_duration = current_time - stats.window_start_time;
      float window_seconds = window_duration / 1000000.0f;

      // 计算平均值和执行频率
      float compute_avg_us =
          stats.compute_count > 0
              ? stats.compute_total_duration / (float)stats.compute_count
              : 0.0f;
      float compute_avg_ms = compute_avg_us / 1000.0f;
      float compute_freq = stats.compute_count / window_seconds;

      // 计算compute方差：Var = E(X²) - [E(X)]²
      float compute_variance = 0.0f;
      if (stats.compute_count > 0) {
        float compute_mean_squared = compute_avg_us * compute_avg_us;
        float compute_mean_of_squares =
            stats.compute_sum_squares / (float)stats.compute_count;
        compute_variance = compute_mean_of_squares - compute_mean_squared;
        if (compute_variance < 0.0f)
          compute_variance = 0.0f; // 防止浮点误差
      }
      float compute_stddev_us = sqrtf(compute_variance); // 标准差
      float compute_stddev_ms = compute_stddev_us / 1000.0f;
      float compute_max_ms = stats.compute_max_duration / 1000.0f;

      float execute_avg_us =
          stats.execute_count > 0
              ? stats.execute_total_duration / (float)stats.execute_count
              : 0.0f;
      float execute_avg_ms = execute_avg_us / 1000.0f;
      float execute_freq = stats.execute_count / window_seconds;

      // 计算execute方差：Var = E(X²) - [E(X)]²
      float execute_variance = 0.0f;
      if (stats.execute_count > 0) {
        float execute_mean_squared = execute_avg_us * execute_avg_us;
        float execute_mean_of_squares =
            stats.execute_sum_squares / (float)stats.execute_count;
        execute_variance = execute_mean_of_squares - execute_mean_squared;
        if (execute_variance < 0.0f)
          execute_variance = 0.0f; // 防止浮点误差
      }
      float execute_stddev_us = sqrtf(execute_variance); // 标准差
      float execute_stddev_ms = execute_stddev_us / 1000.0f;
      float execute_max_ms = stats.execute_max_duration / 1000.0f;

      // 打印统计信息
      ESP_LOGI(executor->TAG,
               "Stats [%.1fs window] - Compute: avg=%.3f ms, "
               "stddev=%.3f ms, max=%.3f ms, freq=%.2f Hz",
               window_seconds, compute_avg_ms, compute_stddev_ms,
               compute_max_ms, compute_freq);
      ESP_LOGI(executor->TAG,
               "Stats [%.1fs window] - Execute: avg=%.3f ms, "
               "stddev=%.3f ms, max=%.3f ms, freq=%.2f Hz",
               window_seconds, execute_avg_ms, execute_stddev_ms,
               execute_max_ms, execute_freq);

      // 重置统计信息，开始新的窗口
      stats.compute_total_duration = 0;
      stats.compute_sum_squares = 0;
      stats.compute_max_duration = 0;
      stats.compute_count = 0;
      stats.execute_total_duration = 0;
      stats.execute_sum_squares = 0;
      stats.execute_max_duration = 0;
      stats.execute_count = 0;
      stats.window_start_time = current_time;
      stats.last_print_time = current_time;
    }
  }
}

/**
 * @brief 发送compute事件
 * @param is_start true表示开始，false表示结束
 */
void Executor::sendComputeEvent(bool is_start) {
  executor_event_data_t event_data = {.is_start = is_start,
                                      .timestamp = esp_timer_get_time()};
  esp_event_post(EXECUTOR_EVENT, EXECUTOR_EVENT_COMPUTE, &event_data,
                 sizeof(event_data), portMAX_DELAY);
}

/**
 * @brief 发送execute事件
 * @param is_start true表示开始，false表示结束
 */
void Executor::sendExecuteEvent(bool is_start) {
  executor_event_data_t event_data = {.is_start = is_start,
                                      .timestamp = esp_timer_get_time()};
  esp_event_post(EXECUTOR_EVENT, EXECUTOR_EVENT_EXECUTE, &event_data,
                 sizeof(event_data), portMAX_DELAY);
}
