#pragma once

#include "executor.hpp"
#include "pid.hpp"
#include <mutex>

// SR6 CAN伺服电机数量
#define SR6CANServoNum 6
#define SR6CANArrLen 6

/**
 * @brief SR6CAN执行器类
 *
 * 继承自Executor，用于通过CAN总线控制6个MIT协议电机实现SR6运动
 */
class SR6CANExecutor : public Executor {
public:
    /**
     * @brief 构造函数
     * @param setting 设置配置
     */
    explicit SR6CANExecutor(const SettingWrapper& setting);

    /**
     * @brief 析构函数
     */
    ~SR6CANExecutor() override;


    /**
     * @brief 获取执行频率
     * @return 执行频率
     */
    int getExecuteFrequency() const;

protected:
    /**
     * @brief 计算电机目标位置
     */
    void compute() override;

    /**
     * @brief 执行电机控制
     */
    void execute() override;

private:
    /**
     * @brief 初始化电机
     */
    void initMotors();

    /**
     * @brief 计算主舵机角度
     * @param x 目标x坐标（1/100 mm）
     * @param y 目标y坐标（1/100 mm）
     * @return 舵机输出值(弧度)
     */
    float SetMainServo(float x, float y);

    /**
     * @brief 计算俯仰舵机角度
     * @param x 目标x坐标（1/100 mm）
     * @param y 目标y坐标（1/100 mm）
     * @param z 目标z坐标（1/100 mm）
     * @param pitch 俯仰角度（1/100 度）
     * @return 舵机输出值(弧度)
     */
    float SetPitchServo(float x, float y, float z, float pitch);

    /**
     * @brief CAN接收任务
     * @param param 任务参数
     */
    static void canReceiveTask(void* param);

    // 电机位置目标值（弧度）
    float motor_position[SR6CANArrLen];
    
    // 电机位置反馈值（弧度）
    float motor_position_feedback[SR6CANArrLen];
    
    // PID控制器数组
    pid_controller_t motor_pid[SR6CANArrLen];
    
    // MIT控制参数
    float motor_kp[SR6CANArrLen];
    float motor_ki[SR6CANArrLen];
    float motor_kd[SR6CANArrLen];
    float motor_offset[SR6CANArrLen];  // 角度，不是弧度
    
    // CAN接收任务句柄
    TaskHandle_t can_receive_task_handle_;
    
    // 互斥锁
    std::mutex compute_mutex_;
    std::mutex feedback_mutex_;
    
    // 初始化完成标志
    bool init_done;
    
    // 静态成员变量
    static const char* TAG;
    static bool mit_initialized_;
    static std::mutex init_mutex_;
};

