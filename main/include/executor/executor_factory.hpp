#pragma once

#include <memory>
#include <vector>
#include "setting.hpp"

// 前向声明
class Executor;

/**
 * @brief Executor 工厂类
 * @details 提供静态工厂方法，根据 servo mode 创建对应的 Executor 实例
 */
class ExecutorFactory {
public:
    /**
     * @brief 根据 servo mode 创建对应的 Executor 实例
     * @details 根据 setting.servo.MODE 的值创建对应的执行器：
     *          - 0: OSR (Multi-Axis Motion)
     *          - 3: SR6
     *          - 6: TrRMax
     *          - 8: SR6CAN
     *          - 9: O6 (6-Axis Parallel Robot)
     * @param setting 配置对象
     * @return Executor 智能指针，如果 mode 无效返回 nullptr
     * @throws std::runtime_error 如果创建 executor 失败
     */
    static std::unique_ptr<Executor> createExecutor(const SettingWrapper &setting);

    /**
     * @brief 将 mode 值转换为字符串描述
     * @param mode servo mode 值
     * @return mode 的字符串描述
     */
    static const char* modeToString(int32_t mode);

    /**
     * @brief 获取所有支持的 executor mode 列表
     * @return 包含所有支持的 mode 值的数组
     */
    static std::vector<int32_t> getSupportedModes();
};

