#pragma once

#include "sys/dirent.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <string>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SR6扩展长度常量（单位：mm）
#ifndef EXTENSION_LENGTH
#define EXTENSION_LENGTH 0.0f
#endif

/**
 * @brief 延时1tick
 */
void delay1();

/**
 * @brief 线性映射函数（模板版本）
 * @param x 输入值
 * @param in_min 输入最小值
 * @param in_max 输入最大值
 * @param out_min 输出最小值
 * @param out_max 输出最大值
 * @return 映射后的值
 * 
 * 支持各种数值类型，自动进行类型转换
 */
template <typename T>
T map_(T x, T in_min, T in_max, T out_min, T out_max) {
    if (in_max == in_min) {
        ESP_LOGW("Utils", "map_: input range is zero, returning out_min");
        return out_min;
    }
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief 将7轴坐标转换为6轴坐标
 * @param x7 7轴x坐标
 * @param y7 7轴y坐标
 * @param z7 7轴z坐标
 * @param roll7 7轴滚转角（度）
 * @param pitch7 7轴俯仰角（度）
 * @param twist7 7轴扭转角（度）
 * @param x6 输出6轴x坐标
 * @param y6 输出6轴y坐标
 * @param z6 输出6轴z坐标
 * @param roll6 输出6轴滚转角（度）
 * @param pitch6 输出6轴俯仰角（度）
 * @param twist6 输出6轴扭转角（度）
 */
void axis7_to_axis6(float x7, float y7, float z7, float roll7, float pitch7,
                    float twist7, float& x6, float& y6, float& z6, float& roll6,
                    float& pitch6, float& twist6);

/**
 * @brief 获取构建参数JSON字符串
 * @return 返回包含固件版本、构建时间、硬件信息和编译选项的JSON字符串
 */
std::string getBuildParameters();

// 辅助宏：用于二次展开
#define _UTILS_CONCAT(a, b) a##b
#define UTILS_CONCAT(a, b) _UTILS_CONCAT(a, b)

// 辅助宏：用于生成唯一的标识符
#define _UTILS_GET_UNIQUE_ID() UTILS_CONCAT(utils_exec_unique_id_, __LINE__)

// 辅助宏：用于创建执行器结构体
#define _DEFINE_EXEC_STRUCT(code, id)                   \
    namespace {                                          \
    struct UTILS_CONCAT(utils_exec_struct_, id) {        \
        UTILS_CONCAT(utils_exec_struct_, id)() {         \
            code;                                       \
        }                                                \
    };                                                   \
    }

// 辅助宏：用于创建执行器实例
#define _DEFINE_EXEC_INSTANCE(id) \
    static UTILS_CONCAT(utils_exec_struct_, id) UTILS_CONCAT(utils_exec_instance_, id);

// 宏定义：在文件顶层作用域执行任意代码
#define EXEC(code)                                      \
    _DEFINE_EXEC_STRUCT(code, _UTILS_GET_UNIQUE_ID())   \
    _DEFINE_EXEC_INSTANCE(_UTILS_GET_UNIQUE_ID())

// 辅助宏：用于创建lambda执行器结构体
#define _DEFINE_LAMBDA_EXEC_STRUCT(lambda, id)                         \
    namespace {                                                         \
    struct UTILS_CONCAT(utils_lambda_exec_struct_, id) {               \
        UTILS_CONCAT(utils_lambda_exec_struct_, id)(auto&& fn) {       \
            fn();                                                       \
        }                                                                \
    };                                                                  \
    }

// 辅助宏：用于创建lambda执行器实例
#define _DEFINE_LAMBDA_EXEC_INSTANCE(id, lambda) \
    static UTILS_CONCAT(utils_lambda_exec_struct_, id) UTILS_CONCAT(utils_lambda_exec_instance_, id)(lambda);

// 辅助宏：内部使用的EXEC_LAMBDA实现（确保id只生成一次）
#define _EXEC_LAMBDA_IMPL(lambda, id) \
    _DEFINE_LAMBDA_EXEC_STRUCT(lambda, id)       \
    _DEFINE_LAMBDA_EXEC_INSTANCE(id, lambda)

// 宏定义：在文件顶层作用域执行lambda
#define EXEC_LAMBDA(lambda) _EXEC_LAMBDA_IMPL(lambda, _UTILS_GET_UNIQUE_ID())



// 使用VFS API实现ls功能
void list_root_directory();