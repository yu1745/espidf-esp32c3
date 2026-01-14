#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <vector>
// #include "Arduino.h"  // Removed for ESP-IDF
#include "ctw/ctw.hpp"
#include "executor.hpp"
#include "pid.hpp"
#include "setting.hpp"

#ifndef SR6CANServoNum
#define SR6CANServoNum 6
#endif

#if SR6CANServoNum <= 6
#define SR6CANArrLen 6
#else
#define SR6CANArrLen SR6CANServoNum
#endif

class SR6CANExecutor : public Executor {
   public:
    SR6CANExecutor(const SettingWrapper& setting);
    ~SR6CANExecutor() override;

   protected:
    void compute() override;
    void execute() override;

   private:
    // 初始化状态枚举
    enum class InitState {
        WAITING_STABILITY,  // 等待位置稳定
        HOMING,             // 回原点中
        RUNNING             // 正常运行
    };

    // 静态成员变量
    static const char* TAG;
    static bool ctw_initialized_;
    static std::mutex init_mutex_;

    // 初始化状态
    InitState init_state_ = InitState::WAITING_STABILITY;
    unsigned long init_start_time_ = 0;
    bool homing_completed_ = false;
    unsigned long homing_start_time_ = 0;
    float homing_target_positions[SR6CANArrLen] = {};

    // 电机位置（弧度）
    double motor_position[SR6CANArrLen];

    float motor_position_feedback[SR6CANArrLen] = {};
    std::mutex compute_mutex_;
    std::mutex feedback_mutex_;

    // // MIT控制参数数组（每个电机的Kp, Ki, Kd, offset）
    // float motor_kp[SR6CANArrLen];
    // float motor_ki[SR6CANArrLen];
    // float motor_kd[SR6CANArrLen];
    float motor_offset[SR6CANArrLen]; /* 角度，不是弧度 */
    float motor_feedback_offset[SR6CANArrLen] = {};
    int stable[6] = {};
    uint32_t last_feedback_update_times[SR6CANArrLen] = {};  // 记录上次反馈更新时间

    // 初始化电机
    void initMotors();

    // 初始化电机PID参数
    void initMotorParams();

    /**
     * @brief 设置主伺服角度
     *
     * @param x
     * @param y
     * @return float 弧度
     */
    float SetMainServo(float x, float y);

    /**
     * @brief 设置俯仰伺服角度
     *
     * @param x
     * @param y
     * @param z
     * @param pitch
     * @return float 弧度
     */
    float SetPitchServo(float x, float y, float z, float pitch);

    /**
     * @brief 从CTW更新反馈数据，包括处理稳定计数和偏移
     */
    void updateFeedbackFromCTW();

    /**
     * @brief 检查是否所有电机都已稳定
     * @return true 所有电机都已稳定
     * @return false 至少有一个电机未稳定
     */
    bool isAllMotorsStable() const;

    /**
     * @brief 打印未稳定的电机信息
     */
    void printUnstableMotors() const;

    /**
     * @brief 执行回原点操作
     */
    void performHoming();

    /**
     * @brief 检查回原点是否完成
     * @return true 回原点完成
     * @return false 回原点未完成
     */
    bool isHomingComplete();
    int getExecuteFrequency();
};
