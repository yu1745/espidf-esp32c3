#ifndef CTW_HPP
#define CTW_HPP

// #include <Arduino.h>  // Removed for ESP-IDF
#include <driver/twai.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_event.h>
#include <cstring>
#include <mutex>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ctw_enums.hpp"

// CAN 占用率事件定义
ESP_EVENT_DECLARE_BASE(CTW_CAN_USAGE_EVENT);

// CAN 占用率事件ID
typedef enum {
    CTW_CAN_USAGE_EVENT_BUS_UTILIZATION = 0,
} ctw_can_usage_event_id_t;

// CAN 占用率事件数据结构
typedef struct {
    float bus_utilization_percent;  // CAN总线占用率百分比
    uint64_t total_bits_sent;       // 发送的总位数
    uint64_t total_bits_received;   // 接收的总位数
    uint32_t bitrate;               // 当前波特率
} ctw_can_usage_event_data_t;

/**
 * @brief CTW (Custom TWAI) - ODrive CANsimple 协议库
 *
 * 独立的 ODrive CANsimple 协议实现，用于控制兼容 ODrive 协议的电机。
 * 不依赖 Executor 框架，可作为纯协议库使用。
 */
class CTW {
   public:
    // ========== 使用 C++20 using enum 引入枚举类型 ==========
    using enum AxisState;
    using enum ControllerMode;
    using enum InputMode;
    using enum EndpointID;

    /**
     * @brief 电机状态反馈结构
     */
    struct MotorFeedback {
        float position;        // 位置（弧度）
        float velocity;        // 速度（弧度/秒）
        float torque;          // 力矩（N·m）
        uint8_t axis_state;    // 轴状态
        bool motor_armed;      // 电机是否使能
        uint32_t error;        // 错误代码
        uint32_t last_update;  // 最后更新时间（毫秒）

        MotorFeedback()
            : position(0),
              velocity(0),
              torque(0),
              axis_state(0),
              motor_armed(false),
              error(0),
              last_update(0) {}
    };

    /**
     * @brief 电机配置结构
     */
    struct MotorConfig {
        float pos_gain;             // 位置增益
        float vel_gain;             // 速度增益
        float vel_integrator_gain;  // 速度积分增益
        float vel_limit;            // 速度限制
        float current_limit;        // 电流限制
        uint8_t control_mode;       // 控制模式
        uint8_t input_mode;         // 输入模式

        MotorConfig()
            : pos_gain(0),
              vel_gain(0),
              vel_integrator_gain(0),
              vel_limit(0),
              current_limit(0),
              control_mode(CTRL_MODE_POSITION),
              input_mode(INPUT_MODE_POS_FILTER) {}
    };

    // ========== 初始化函数 ==========

    /**
     * @brief 初始化 CTW 协议库
     * @param tx_pin CAN TX 引脚
     * @param rx_pin CAN RX 引脚
     * @param bitrate CAN 波特率（默认 500kbps）
     * @note 初始化时会自动启动接收任务，无需手动调用 start_receive_task()
     * @return esp_err_t 错误码
     */
    static esp_err_t init(int tx_pin = 2,
                          int rx_pin = 3,
                          uint32_t bitrate = 500000);

    /**
     * @brief 反初始化 CTW 协议库
     * @note 反初始化时会自动停止接收任务
     */
    static esp_err_t deinit();

    // ========== 电机控制函数 ==========

    /**
     * @brief 设置电机目标位置
     * @param node_id 电机节点 ID（1-8）
     * @param position 目标位置（弧度）
     * @return esp_err_t 错误码
     */
    static esp_err_t set_position(uint8_t node_id, float position);

    /**
     * @brief 设置电机目标速度
     * @param node_id 电机节点 ID
     * @param velocity 目标速度（弧度/秒）
     * @return esp_err_t 错误码
     */
    static esp_err_t set_velocity(uint8_t node_id, float velocity);

    /**
     * @brief 设置电机目标力矩
     * @param node_id 电机节点 ID
     * @param torque 目标力矩（N·m）
     * @return esp_err_t 错误码
     */
    static esp_err_t set_torque(uint8_t node_id, float torque);

    // ========== 状态读取函数 ==========

    /**
     * @brief 获取电机位置反馈
     * @param node_id 电机节点 ID
     * @param position 输出参数：位置（弧度）
     * @param timeout_ms 超时时间（毫秒）
     * @return esp_err_t 错误码
     */
    static esp_err_t get_position(uint8_t node_id,
                                  float& position,
                                  int timeout_ms = 1000);

    /**
     * @brief 获取电机速度反馈
     * @param node_id 电机节点 ID
     * @param velocity 输出参数：速度（弧度/秒）
     * @param timeout_ms 超时时间（毫秒）
     * @return esp_err_t 错误码
     */
    static esp_err_t get_velocity(uint8_t node_id,
                                  float& velocity,
                                  int timeout_ms = 1000);

    /**
     * @brief 获取电机当前状态
     * @param node_id 电机节点 ID
     * @param state 输出参数：轴状态
     * @param timeout_ms 超时时间（毫秒）
     * @return esp_err_t 错误码
     */
    static esp_err_t get_current_state(uint8_t node_id,
                                       uint8_t& state,
                                       int timeout_ms = 1000);

    /**
     * @brief 获取完整反馈数据
     * @param node_id 电机节点 ID
     * @param feedback 输出参数：完整反馈结构
     * @param timeout_ms 超时时间（毫秒）
     * @return esp_err_t 错误码
     */
    static esp_err_t get_feedback(uint8_t node_id,
                                  MotorFeedback& feedback,
                                  int timeout_ms = 1000);

    // ========== 参数读写函数 ==========

    /**
     * @brief 写入 Endpoint（float 类型）
     * @param node_id 电机节点 ID
     * @param endpoint_id Endpoint ID
     * @param value 写入值
     * @return esp_err_t 错误码
     */
    static esp_err_t write_endpoint_float(uint8_t node_id,
                                          uint16_t endpoint_id,
                                          float value);

    /**
     * @brief 写入 Endpoint（uint32 类型）
     */
    static esp_err_t write_endpoint_uint32(uint8_t node_id,
                                           uint16_t endpoint_id,
                                           uint32_t value);

    /**
     * @brief 写入 Endpoint（int32 类型）
     */
    static esp_err_t write_endpoint_int32(uint8_t node_id,
                                          uint16_t endpoint_id,
                                          int32_t value);

    /**
     * @brief 写入 Endpoint（uint8 类型）
     */
    static esp_err_t write_endpoint_uint8(uint8_t node_id,
                                          uint16_t endpoint_id,
                                          uint8_t value);

    /**
     * @brief 写入 Endpoint（bool 类型）
     */
    static esp_err_t write_endpoint_bool(uint8_t node_id,
                                         uint16_t endpoint_id,
                                         bool value);

    /**
     * @brief 读取 Endpoint
     * @param node_id 电机节点 ID
     * @param endpoint_id Endpoint ID
     * @param value 输出参数：读取的值
     * @param value_size 值的大小（字节）
     * @param timeout_ms 超时时间（毫秒）
     * @return esp_err_t 错误码
     */
    static esp_err_t read_endpoint(uint8_t node_id,
                                   uint16_t endpoint_id,
                                   void* value,
                                   size_t value_size,
                                   int timeout_ms = 1000);

    /**
     * @brief 读取 Endpoint（bool 类型）
     */
    static esp_err_t read_endpoint_bool(uint8_t node_id,
                                        uint16_t endpoint_id,
                                        bool& value,
                                        int timeout_ms = 1000);

    // ========== 电机配置函数 ==========

    /**
     * @brief 设置轴状态
     * @param node_id 电机节点 ID
     * @param state 目标状态
     * @return esp_err_t 错误码
     */
    static esp_err_t set_axis_state(uint8_t node_id, AxisState state);

    /**
     * @brief 启动电机（进入闭环控制）
     */
    static esp_err_t start_motor(uint8_t node_id);

    /**
     * @brief 停止电机（进入空闲状态）
     */
    static esp_err_t stop_motor(uint8_t node_id);

    /**
     * @brief 设置控制器模式
     * @param node_id 电机节点 ID
     * @param control_mode 控制器模式
     * @param input_mode 输入模式
     * @return esp_err_t 错误码
     */
    static esp_err_t set_controller_mode(uint8_t node_id,
                                         ControllerMode control_mode,
                                         InputMode input_mode);

    /**
     * @brief 设置电机配置
     */
    static esp_err_t set_motor_config(uint8_t node_id,
                                      const MotorConfig& config);

    /**
     * @brief 设置输入滤波器带宽
     * @param node_id 电机节点 ID
     * @param bandwidth 滤波器带宽（Hz）
     * @return esp_err_t 错误码
     */
    static esp_err_t set_filter_bandwidth(uint8_t node_id, float bandwidth);

    /**
     * @brief 清除错误
     */
    static esp_err_t clear_errors(uint8_t node_id);

    // ========== 接收任务管理 ==========

    /**
     * @brief 启动接收任务（后台自动接收反馈数据）
     * @note 接收任务会在 init() 时自动启动，在 deinit() 时自动停止
     * @note 通常不需要手动调用此函数，除非需要重新启动接收任务
     * @param task_priority 任务优先级（默认 5）
     * @return esp_err_t 错误码
     */
    static esp_err_t start_receive_task(uint8_t task_priority = 5);

    /**
     * @brief 停止接收任务
     * @note 接收任务会在 deinit() 时自动停止
     * @note 通常不需要手动调用此函数，除非需要临时停止接收任务
     */
    static esp_err_t stop_receive_task();

    /**
     * @brief 获取缓存的反馈数据（非阻塞）
     * @param node_id 电机节点 ID
     * @param feedback 输出参数：反馈数据
     * @return esp_err_t 错误码
     */
    static esp_err_t get_cached_feedback(uint8_t node_id,
                                         MotorFeedback& feedback);

    // ========== 辅助函数 ==========

    /**
     * @brief 字节序转换（大端序 <-> 小端序）
     */
    static void swap_endian(void* data, size_t length);

    /**
     * @brief 获取状态描述字符串
     */
    static const char* get_axis_state_string(AxisState state);

    /**
     * @brief 检查是否已初始化
     */
    static bool is_initialized();

    /**
     * @brief 检查是否已启动
     */
    static bool is_started();

    /**
     * @brief 启动 CAN 驱动
     */
    static esp_err_t start();

    /**
     * @brief 停止 CAN 驱动
     */
    static esp_err_t stop();

    /**
     * @brief 发送 CAN 消息（底层接口）
     * @param id 消息ID
     * @param data 数据指针
     * @param data_len 数据长度
     * @param is_extended 是否为扩展帧
     * @param timeout_ms 超时时间(毫秒)
     * @return esp_err_t 错误码
     */
    static esp_err_t send(uint32_t id, const uint8_t *data, uint8_t data_len,
                          bool is_extended = false, int timeout_ms = 20);

    /**
     * @brief 接收 CAN 消息（底层接口）
     * @param id 输出参数：接收到的消息ID
     * @param data 输出参数：接收到的数据
     * @param data_len 输出参数：接收到的数据长度
     * @param is_extended 输出参数：是否为扩展帧
     * @param timeout_ms 超时时间(毫秒)
     * @return esp_err_t 错误码
     */
    static esp_err_t receive(uint32_t *id, uint8_t *data, uint8_t *data_len,
                             bool *is_extended, int timeout_ms = 1000);

   private:
    // ========== 静态成员变量 ==========

    static bool initialized_;                  // 是否已初始化
    static bool started_;                      // 是否已启动
    static uint32_t current_bitrate_;          // 当前波特率
    static std::mutex init_mutex_;             // 初始化互斥锁
    static std::mutex send_mutex_;             // 发送互斥锁
    static TaskHandle_t receive_task_handle_;  // 接收任务句柄
    static bool receive_task_running_;         // 接收任务运行标志

    // 反馈数据缓存（支持最多 8 个电机）
    static MotorFeedback feedback_cache_[8];
    static std::mutex feedback_mutex_;  // 反馈数据互斥锁

    // CAN 占用率统计
    static uint64_t total_bits_sent_;          // 发送的总位数
    static uint64_t total_bits_received_;      // 接收的总位数
    static int64_t last_stats_time_;          // 上次统计时间（微秒）
    static std::mutex stats_mutex_;            // 统计互斥锁

    // ========== CAN 底层控制私有函数 ==========

    /**
     * @brief 计算CAN消息的传输时间（微秒）
     * @param bitrate 波特率（bps）
     * @param data_len 数据长度（0-8）
     * @param is_extended 是否为扩展帧
     * @return 传输时间（微秒）
     */
    static uint32_t calculate_message_transmission_time(uint32_t bitrate,
                                                        uint8_t data_len,
                                                        bool is_extended);

    /**
     * @brief 计算CAN帧的总位数
     * @param data_len 数据长度（0-8）
     * @param is_extended 是否为扩展帧
     * @return 总位数（包括位填充）
     */
    static uint32_t calculate_frame_bits(uint8_t data_len, bool is_extended);

    /**
     * @brief 检查并发布CAN占用率事件（如果超过1秒）
     */
    static void check_and_publish_usage();

    // ========== 私有函数 ==========

    /**
     * @brief 接收任务（后台运行）
     */
    static void receive_task(void* param);

    /**
     * @brief 打包 Endpoint 数据（SteadyWin 自定义 SDO 格式）
     * @param opcode 操作码 (0=读, 1=写)
     * @param endpoint_id Endpoint ID
     * @param data 输出的 8 字节数据
     * @param value 要写入的值（读操作时为 nullptr）
     * @param value_size 值的大小（字节）
     *
     * 数据域格式：
     * Byte 0:   opcode (0=读, 1=写)
     * Byte 1-2: Endpoint_ID (uint16, 小端序)
     * Byte 3:   预留
     * Byte 4-7: Value (4字节)
     */
    static void pack_endpoint_data(uint8_t opcode,
                                   uint16_t endpoint_id,
                                   uint8_t data[8],
                                   const void* value,
                                   size_t value_size);

    /**
     * @brief 解包 Endpoint 数据
     */
    static void unpack_endpoint_data(const uint8_t data[8],
                                     void* value,
                                     size_t value_size);

    /**
     * @brief 发送 CAN 消息（带错误处理）
     */
    static esp_err_t send_can_message(uint32_t id,
                                      const uint8_t* data,
                                      uint8_t data_len);

    /**
     * @brief 接收 CAN 消息（带超时）
     */
    static esp_err_t receive_can_message(uint32_t* id,
                                         uint8_t* data,
                                         uint8_t* data_len,
                                         int timeout_ms);
};

#endif  // CTW_HPP
