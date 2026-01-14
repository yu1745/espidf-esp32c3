#include "pid.hpp"

void pid_init(pid_context_t* pid, float kp, float ki, float kd, float integral_max) {
    if (pid == nullptr) {
        return;
    }
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->integral_max = integral_max;
}

float pid_update(pid_context_t* pid, float setpoint, float feedback) {
    if (pid == nullptr) {
        return 0.0f;
    }

    // 计算误差
    float error = setpoint - feedback;

    // 比例项
    float p_term = pid->kp * error;

    // 积分项
    pid->integral += error;
    // 积分限幅
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < -pid->integral_max) {
        pid->integral = -pid->integral_max;
    }
    float i_term = pid->ki * pid->integral;

    // 微分项
    float d_term = pid->kd * (error - pid->last_error);
    pid->last_error = error;

    // 计算输出
    float output = p_term + i_term + d_term;

    return output;
}

