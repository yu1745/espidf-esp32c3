#pragma once

#include <driver/twai.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <mutex>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

// 事件基础声明
ESP_EVENT_DECLARE_BASE(TWAI_EVENT);

// 事件ID定义
typedef enum {
    TWAI_EVENT_BUS_LOAD_UPDATE = 0,
} twai_event_id_t;

// 总线负载更新事件数据结构
typedef struct {
    float rxLoad;              // 接收负载率 (0.0-1.0)
    float txLoad;              // 发送负载率 (0.0-1.0)
    float totalLoad;           // 总负载率 (0.0-1.0)
    uint32_t rxMessageCount;   // 接收消息数量
    uint32_t txMessageCount;   // 发送消息数量
    uint32_t rxBytesCount;     // 接收字节数
    uint32_t txBytesCount;     // 发送字节数
    uint32_t bitrate;          // 当前波特率
    uint32_t timestamp;        // 时间戳（毫秒）
} twai_bus_load_update_event_data_t;

#ifdef __cplusplus
}
#endif

// TWAI工具函数
namespace twai_util {
/**
 * @brief 计算CAN消息的传输时间（微秒）
 * @param bitrate 波特率（bps）
 * @param data_len 数据长度（0-8）
 * @param is_extended 是否为扩展帧
 * @return 传输时间（微秒）
 */
inline uint32_t calculate_message_transmission_time(uint32_t bitrate,
                                                uint8_t data_len,
                                                bool is_extended) {
  // CAN帧位数计算公式：
  // 帧位数 = 1(起始位) + 11/29(ID位数) + 1(RTR位) + 1(IDE位) + 6(控制位) +
  //           8*data_len(数据位) + 2(CRC分隔位) + 16(CRC位) + 2(ACK位) + 7(EOF)
  
  // 标准帧或扩展帧的位数
  uint32_t header_bits = is_extended ? 58 : 32; // 包含ID、控制等位
  uint32_t data_bits = 8 * data_len;
  uint32_t footer_bits = 25; // CRC、ACK、EOF等
  uint32_t stuff_bits = (header_bits + data_bits + footer_bits) / 5; // 位填充大约20%
  
  uint32_t total_bits = header_bits + data_bits + footer_bits + stuff_bits;
  uint32_t time_us = (total_bits * 1000000) / bitrate;
  
  return time_us;
}
}

class TWAI {
   public:
    /**
     * @brief 初始化TWAI驱动
     * @param tx_pin TX引脚 (默认IO3)
     * @param rx_pin RX引脚 (默认IO2)
     * @param bitrate 波特率 (默认500kbps)
     * @return esp_err_t 错误码
     */
    static esp_err_t init(int tx_pin,
                          int rx_pin,
                          uint32_t bitrate = 500 * 1000);

    /**
     * @brief 启动TWAI驱动
     * @return esp_err_t 错误码
     */
    static esp_err_t start();

    /**
     * @brief 停止TWAI驱动
     * @return esp_err_t 错误码
     */
    static esp_err_t stop();

    /**
     * @brief 卸载TWAI驱动
     * @return esp_err_t 错误码
     */
    static esp_err_t deinit();

    /**
     * @brief 发送TWAI消息
     * @param id 消息ID
     * @param data 数据指针
     * @param data_len 数据长度
     * @param is_extended 是否为扩展帧
     * @param timeout_ms 超时时间(毫秒)
     * @return esp_err_t 错误码
     */
    static esp_err_t send(uint32_t id,
                          const uint8_t* data,
                          uint8_t data_len,
                          bool is_extended = false,
                          int timeout_ms = 20);

    /**
     * @brief 接收TWAI消息
     * @param id 输出参数：接收到的消息ID
     * @param data 输出参数：接收到的数据
     * @param data_len 输出参数：接收到的数据长度
     * @param is_extended 输出参数：是否为扩展帧
     * @param timeout_ms 超时时间(毫秒)
     * @return esp_err_t 错误码
     */
    static esp_err_t receive(uint32_t* id,
                             uint8_t* data,
                             uint8_t* data_len,
                             bool* is_extended,
                             int timeout_ms = 1000);

    /**
     * @brief 检查TWAI是否已初始化
     * @return bool true: 已初始化, false: 未初始化
     */
    static bool is_initialized();

    /**
     * @brief 检查TWAI是否已启动
     * @return bool true: 已启动, false: 未启动
     */
    static bool is_started();

    /**
     * @brief 初始化TWAI事件系统
     * @return esp_err_t 错误码
     */
    static esp_err_t initEventSystem();

    /**
     * @brief 注册总线负载更新事件处理器
     * @param handler 事件处理器函数
     * @param handler_arg 处理器参数
     * @return esp_err_t 错误码
     */
    static esp_err_t registerBusLoadHandler(esp_event_handler_t handler, void* handler_arg);

   private:
    static bool initialized;
    static bool started;
    static uint32_t current_bitrate;
    static TaskHandle_t bus_load_task_handle;
    static std::mutex stats_mutex;

    // 总线负载监控统计变量
    static uint32_t rx_message_count;
    static uint32_t tx_message_count;
    static uint32_t rx_bytes_count;
    static uint32_t tx_bytes_count;
    static uint64_t rx_total_transmission_time_us;
    static uint64_t tx_total_transmission_time_us;

    /**
     * @brief 总线负载监控任务
     * @param arg 任务参数
     */
    static void busLoadMonitorTask(void* arg);

    /**
     * @brief 更新发送统计信息
     * @param data_len 数据长度
     * @param is_extended 是否为扩展帧
     */
    static void updateTxStats(uint8_t data_len, bool is_extended);

    /**
     * @brief 更新接收统计信息
     * @param data_len 数据长度
     * @param is_extended 是否为扩展帧
     */
    static void updateRxStats(uint8_t data_len, bool is_extended);
};