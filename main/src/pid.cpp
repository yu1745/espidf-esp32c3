#include "pid.hpp"

void pid_init(pid_controller_t* pid, float kp, float ki, float kd, float max_integral) {
    if (pid == nullptr) {
        return;
    }
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->max_integral = max_integral;
}

float pid_update(pid_controller_t* pid, float setpoint, float current_value) {
    if (pid == nullptr) {
        return 0.0f;
    }

    // 计算误差
    float error = setpoint - current_value;

    // 比例项
    float p_term = pid->kp * error;

    // 积分项
    pid->integral += error;
    // 积分限幅
    if (pid->integral > pid->max_integral) {
        pid->integral = pid->max_integral;
    } else if (pid->integral < -pid->max_integral) {
        pid->integral = -pid->max_integral;
    }
    float i_term = pid->ki * pid->integral;

    // 微分项
    float d_term = pid->kd * (error - pid->last_error);
    pid->last_error = error;

    // 计算输出
    float output = p_term + i_term + d_term;

    return output;
}

