#pragma once

#include "executor.hpp"
#include <string>

/**
 * @brief MockExecutor类
 * Executor的具体实现类，用于执行TCode命令
 */
class MockExecutor : public Executor {
   public:
    MockExecutor();
    virtual ~MockExecutor() = default;

    /**
     * @brief 计算TCode命令
     */
    virtual void compute() override;

    /**
     * @brief 执行TCode命令
     */
    virtual void execute() override;
};
