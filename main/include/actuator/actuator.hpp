#pragma once

#include <esp_log.h>
#include <string>

namespace actuator {

/**
 * @brief 执行器抽象类
 *
 * 线性执行器的抽象基类，输入是以0为中点的对称区间，输入类型为float
 * 输出由各具体执行器自行实现
 */
class Actuator {
   public:
    /**
     * @brief 构造函数
     * @param offset 偏移量，默认为0.0f
     * @throws std::exception
     * 某些派生类的构造函数可能会抛出异常,在外设初始化失败时
     */
    Actuator(float offset = 0.0f) : m_offset(offset) {}

    /**
     * @brief 虚析构函数
     */
    virtual ~Actuator() = default;

    /**
     * @brief 设置执行器目标值
     * @param target 目标值,范围[-1, 1]
     * @return true 设置成功，false 设置失败
     */
    virtual void setTarget(float target) = 0;

    /**
     * @brief 获取当前目标值
     * @return 当前目标值
     */
    float getTarget() const { return m_target; }

   protected:
    /**
     * @brief 目标值
     */
    float m_target;

    /**
     * @brief 偏移量
     */
    float m_offset;

    /**
     * @brief 执行器输出实现（纯虚函数）
     * @param wait 等待时间，单位ms，0表示不等待 -1表示等待直到完成
     * @return true 输出成功，false 输出失败
     */
    virtual bool actuate(int wait = 0) = 0;

    /**
     * @brief 检查执行器是否有反馈
     * @return true 有反馈，false 无反馈
     */
    virtual bool hasFeedback() const { return false; };

    virtual float getFeedback() const { return 0.0f; };
};

}  // namespace actuator