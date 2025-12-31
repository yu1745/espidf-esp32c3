#pragma once

#include <mutex>
#include "actuator/actuator.hpp"
#include "driver/ledc.h"

namespace actuator {

/**
 * @brief LEDC执行器实现类
 *
 * 使用LEDC外设实现PWM输出，将-1到1的输入映射到500-2500us的高电平持续时间
 * 使用14位分辨率实现精确的PWM控制
 */
class LEDCActuator : public Actuator {
   public:
    /**
     * @brief 构造函数
     * @param gpio_num 输出GPIO引脚号
     * @param channel LEDC通道号
     * @param timer LEDC定时器号
     * @param freq_hz PWM频率，默认50Hz
     * @param offset 偏移量，默认为0.0f
     */
    LEDCActuator(int gpio_num, ledc_channel_t channel, ledc_timer_t timer, uint32_t freq_hz = 50, float offset = 0.0f);

    /**
     * @brief 析构函数
     */
    virtual ~LEDCActuator();

    /**
     * @brief 设置执行器目标值
     * @param target 目标值，范围[-1, 1]
     */
    void setTarget(float target) override;

   protected:
    /**
     * @brief 执行器输出实现
     * @param wait 等待时间，单位ms，0表示不等待
     * @return true 输出成功，false 输出失败
     */
    bool actuate(int wait = 0) override;

   private:
    int m_gpio_num;                                          // GPIO引脚号
    ledc_channel_t m_channel;                                // LEDC通道
    ledc_timer_t m_timer;                                    // LEDC定时器
    uint32_t m_freq_hz;                                      // PWM频率
    ledc_timer_bit_t m_duty_resolution = LEDC_TIMER_14_BIT;  // PWM分辨率(14位)
    std::mutex m_mutex;                                      // 互斥锁

    /**
     * @brief 初始化LEDC
     * @return true 初始化成功，false 初始化失败
     */
    bool initLEDC();

    /**
     * @brief 将目标值(-1到1)转换为PWM占空比值
     * @param target 目标值，范围[-1, 1]
     * @return PWM占空比值
     */
    uint32_t targetToDuty(float target);
};

}  // namespace actuator