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

/**
 * @brief 打印所有编译选项的开启状态
 *
 * 打印 def.h 中定义的所有 CONFIG_ENABLE_* 配置选项的状态
 */
void printBuildConfigOptions();

/**
 * @brief 带LED故障码显示的错误检查宏
 * @param _expr 要检查的表达式（esp_err_t类型）
 * @param _err_code 故障码（err.h中定义）
 *
 * 如果表达式返回ESP_OK，继续执行
 * 如果表达式返回错误：
 *   1. 如果LED已初始化（s_led_initialized为true），显示对应的故障码
 *   2. 调用ESP_ERROR_CHECK中止程序并显示错误信息
 *
 * 注意：需要在使用的文件中声明外部变量：extern bool s_led_initialized;
 *       并且需要包含 led.hpp 头文件
 */
#define ESP_ERROR_CHECK_WITH_LED(_expr, _err_code) do { \
    esp_err_t _err_rc = (_expr); \
    if (_err_rc != ESP_OK) { \
        if (s_led_initialized) { \
            Led::getInstance()->showErrorCode(_err_code); \
            vTaskDelay(pdMS_TO_TICKS(100)); \
        } \
        ESP_ERROR_CHECK(_err_rc); \
    } \
} while(0)

/**
 * @brief 带忽略特定错误码的错误检查宏
 * @param _expr 要检查的表达式（esp_err_t类型）
 * @param _ignore_err_code 要忽略的错误码
 *
 * 如果表达式返回ESP_OK或指定的忽略错误码，继续执行
 * 如果表达式返回其他错误，调用ESP_ERROR_CHECK中止程序并显示错误信息
 *
 * 使用场景：当某个函数可能返回"已初始化"或"不存在"等非致命错误时
 */
#define ESP_ERROR_CHECK_IGNORE(_expr, _ignore_err_code) do { \
    esp_err_t _err_rc = (_expr); \
    if (_err_rc != ESP_OK && _err_rc != (_ignore_err_code)) { \
        ESP_ERROR_CHECK(_err_rc); \
    } \
} while(0)

/**
 * @brief 带忽略多个错误码的错误检查宏
 * @param _expr 要检查的表达式（esp_err_t类型）
 * @param ... 要忽略的错误码列表（可变参数）
 *
 * 如果表达式返回ESP_OK或任意一个指定的忽略错误码，继续执行
 * 如果表达式返回其他错误，调用ESP_ERROR_CHECK中止程序并显示错误信息
 *
 * 使用场景：当某个函数可能返回多个非致命错误时
 */
#define ESP_ERROR_CHECK_IGNORE_MULTI(_expr, ...) do { \
    esp_err_t _err_rc = (_expr); \
    if (_err_rc != ESP_OK) { \
        esp_err_t _ignore_errs[] = {__VA_ARGS__}; \
        bool _should_ignore = false; \
        for (size_t _i = 0; _i < sizeof(_ignore_errs)/sizeof(_ignore_errs[0]); _i++) { \
            if (_err_rc == _ignore_errs[_i]) { \
                _should_ignore = true; \
                break; \
            } \
        } \
        if (!_should_ignore) { \
            ESP_ERROR_CHECK(_err_rc); \
        } \
    } \
} while(0)