#pragma once

#include <mutex>
#include "actuator/actuator.hpp"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"

namespace actuator {

/**
 * @brief RMT执行器实现类
 *
 * 使用RMT外设实现精确的脉冲输出，将-1到1的输入映射到500-2500us的高电平持续时间
 * 适合控制舵机、编码器等需要精确时序的设备
 */
class RMTActuator : public Actuator {
   public:
    /**
     * @brief 构造函数
     * @param gpio_num 输出GPIO引脚号
     * @param channel RMT通道号
     * @param tick_us rmt一个tick对应时间，单位微秒
     */
    RMTActuator(int gpio_num, uint32_t tick_us = 1);

    /**
     * @brief 析构函数
     */
    virtual ~RMTActuator();

    void setTarget(float target) override;
    /**
     * @brief 执行器输出实现
     * @param wait 等待时间，单位ms，0表示不等待
     * @return true 输出成功，false 输出失败
     */
    bool actuate(int wait = 0) override;

   private:
    int m_gpio_num;                     // GPIO引脚号
    int m_channel;                      // RMT通道号
    rmt_channel_handle_t m_tx_channel;  // RMT通道句柄
    rmt_encoder_handle_t m_encoder;     // RMT编码器句柄
    uint32_t m_tick_us;                 // RMT一个tick对应时间，单位微秒
    rmt_transmit_config_t m_tx_config;  // 传输配置
    std::mutex m_mutex;                 // 互斥锁
    bool m_initialized = false;         // 初始化标志

    /**
     * @brief 初始化RMT
     * @return true 初始化成功，false 初始化失败
     */
    bool initRMT();

    /**
     * @brief 将目标值(-1到1)转换为RMT脉冲宽度
     * @param target 目标值，范围[-1, 1]
     * @return 脉冲宽度(微秒)
     */
    uint32_t targetToPulseWidth(float target);

    /**
     * @brief 编码器函数，将脉冲宽度转换为RMT符号
     */
    static size_t rmt_encoder_cb(const void* data,
                                 size_t data_size,
                                 size_t symbols_written,
                                 size_t symbols_free,
                                 rmt_symbol_word_t* symbols,
                                 bool* done,
                                 void* arg);
};

/**
 * @brief RMT执行器测试函数
 * @param gpio_num 测试使用的GPIO引脚号
 * @param channel RMT通道号
 * @param tick_us RMT一个tick对应时间，单位微秒
 */
static void testRMTActuator(int gpio_num, uint32_t channel, uint32_t tick_us);

}  // namespace actuator