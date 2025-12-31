#pragma once

#include <cstdint>

/**
 * @brief PID控制器结构体
 */
typedef struct {
    float kp;        // 比例系数
    float ki;        // 积分系数
    float kd;        // 微分系数
    float integral;  // 积分累积值
    float last_error; // 上次误差
    float max_integral; // 积分限幅
} pid_controller_t;

/**
 * @brief 初始化PID控制器
 * @param pid PID控制器指针
 * @param kp 比例系数
 * @param ki 积分系数
 * @param kd 微分系数
 * @param max_integral 积分限幅
 */
void pid_init(pid_controller_t* pid, float kp, float ki, float kd, float max_integral);

/**
 * @brief 更新PID控制器
 * @param pid PID控制器指针
 * @param setpoint 目标值
 * @param current_value 当前值
 * @return PID输出值
 */
float pid_update(pid_controller_t* pid, float setpoint, float current_value);

