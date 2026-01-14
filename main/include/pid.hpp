#pragma once

#include <cstdint>

// 位置模式PID结构体
struct pid_context_t {
    float kp;            // 比例增益
    float ki;            // 积分增益
    float kd;            // 微分增益
    float integral;      // 积分值
    float integral_max;  // 积分限幅
    float last_error;    // 上一次误差
    uint32_t last_time;  // 上一次更新时间(微秒)
    float dt;            // 时间间隔(秒)
    bool compare(void* other) const;
};

/**
 * @brief 初始化 PID 控制器参数
 *
 * @param pid PID 上下文
 * @param kp 比例增益
 * @param ki 积分增益
 * @param kd 微分增益
 * @param integral_max 积分限幅
 */

void pid_init(pid_context_t* pid,
              float kp,
              float ki,
              float kd,
              float integral_max);

/**
 * @brief PID 计算函数
 *
 * @param pid PID 上下文
 * @param setpoint 目标值
 * @param feedback 反馈值
 * @return float 控制输出
 */

float pid_update(pid_context_t* pid, float setpoint, float feedback);