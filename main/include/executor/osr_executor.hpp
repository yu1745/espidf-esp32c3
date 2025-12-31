#pragma once

#include "executor.hpp"
#include "proto/setting.pb.h"
#include "actuator/ledc_actuator.hpp"
#include <memory>

/**
 * @brief OSR（Multi-Axis Motion）执行器类
 *
 * 继承自Executor，用于控制多个舵机实现多轴运动
 * 使用LEDC外设控制PWM输出，实现L0, R0, R1, R2四个轴的控制
 */
class OSRExecutor : public Executor {
   public:
    /**
     * @brief 构造函数
     * @param setting 设置配置
     */
    explicit OSRExecutor(const SettingWrapper& setting);

    /**
     * @brief 析构函数
     */
    ~OSRExecutor() override;

   protected:
    /**
     * @brief 计算舵机目标位置
     * 从tcode中解析L0, R0, R1, R2的值，并计算各个舵机的目标占空比
     */
    void compute() override;

    /**
     * @brief 执行舵机控制
     * 将计算出的占空比应用到各个舵机
     */
    void execute() override;

   private:
    /**
     * @brief 初始化LEDC定时器
     * @return true 成功，false 失败
     */
    bool initLEDC();

    // 舵机执行器
    std::unique_ptr<actuator::LEDCActuator> m_servo_a;  // 舵机A
    std::unique_ptr<actuator::LEDCActuator> m_servo_b;  // 舵机B
    std::unique_ptr<actuator::LEDCActuator> m_servo_c;  // 舵机C
    std::unique_ptr<actuator::LEDCActuator> m_servo_d;  // 舵机D

    // 计算结果占空比
    float m_servo_a_duty;
    float m_servo_b_duty;
    float m_servo_c_duty;
    float m_servo_d_duty;

    static const char* TAG;
    static constexpr int COMPUTE_TIMEOUT = 1000;  // 计算超时阈值（微秒）
};

