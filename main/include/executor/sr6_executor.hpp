#pragma once

#include "actuator/actuator.hpp"
#include "actuator/ledc_actuator.hpp"
#include "actuator/rmt_actuator.hpp"
#include "executor.hpp"
#include <cmath>
#include <memory>

/**
 * @brief SR6执行器类
 *
 * 继承自Executor，用于控制六个舵机实现SR6运动
 * 使用LEDC外设控制PWM输出，实现A-F六个舵机的控制
 * 使用RMT外设控制G舵机
 */
class SR6Executor : public Executor {
public:
  /**
   * @brief 构造函数
   * @param setting 设置配置
   */
  explicit SR6Executor(const SettingWrapper &setting);

  /**
   * @brief 析构函数
   */
  ~SR6Executor() override;

protected:
  /**
   * @brief 计算舵机目标位置
   * 从tcode中解析L0, L1, L2, R0, R1, R2的值，并计算各个舵机的目标占空比
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

  /**
   * @brief 计算主舵机角度
   * @param x 目标x坐标（1/100 mm）
   * @param y 目标y坐标（1/100 mm）
   * @return 舵机输出值(弧度)
   */
  float setMainServo(float x, float y);

  /**
   * @brief 计算俯仰舵机角度
   * @param x 目标x坐标（1/100 mm）
   * @param y 目标y坐标（1/100 mm）
   * @param z 目标z坐标（1/100 mm）
   * @param pitch 俯仰角度（1/100 度）
   * @return 舵机输出值(弧度)
   */
  float setPitchServo(float x, float y, float z, float pitch);

  // 舵机执行器
  std::unique_ptr<actuator::Actuator> m_servo_a; // 舵机A(LEDC)
  std::unique_ptr<actuator::Actuator> m_servo_b; // 舵机B(LEDC)
  std::unique_ptr<actuator::Actuator> m_servo_c; // 舵机C(LEDC)
  std::unique_ptr<actuator::Actuator> m_servo_d; // 舵机D(LEDC)
  std::unique_ptr<actuator::Actuator> m_servo_e; // 舵机E(LEDC)
  std::unique_ptr<actuator::Actuator> m_servo_f; // 舵机F(LEDC)
  std::unique_ptr<actuator::Actuator> m_servo_g; // 舵机G(RMT)

  // 计算结果占空比（-1到1范围）
  float m_servo_a_duty;
  float m_servo_b_duty;
  float m_servo_c_duty;
  float m_servo_d_duty;
  float m_servo_e_duty;
  float m_servo_f_duty;
  float m_servo_g_duty;

  static const char *TAG;
  static constexpr int COMPUTE_TIMEOUT = 1000; // 计算超时阈值（微秒）
  static constexpr float MS_PER_RAD = 636.62f; // 每弧度对应的毫秒数
  static constexpr float US_PER_RAD = 1000.0f * MS_PER_RAD;
};
