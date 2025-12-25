#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "led_strip_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB颜色结构体
 */
typedef struct {
    uint32_t red;   /**< 红色分量 (0-255) */
    uint32_t green; /**< 绿色分量 (0-255) */
    uint32_t blue;  /**< 蓝色分量 (0-255) */
} led_color_t;

// 常见颜色宏定义
#define LED_COLOR_BLACK {0, 0, 0}       /**< 黑色 */
#define LED_COLOR_WHITE {255, 255, 255} /**< 白色 */
#define LED_COLOR_RED {255, 0, 0}       /**< 红色 */
#define LED_COLOR_GREEN {0, 255, 0}     /**< 绿色 */
#define LED_COLOR_BLUE {0, 0, 255}      /**< 蓝色 */
#define LED_COLOR_YELLOW {255, 255, 0}  /**< 黄色 */
#define LED_COLOR_CYAN {0, 255, 255}    /**< 青色 */
#define LED_COLOR_MAGENTA {255, 0, 255} /**< 品红色 */
#define LED_COLOR_ORANGE {255, 165, 0}  /**< 橙色 */
#define LED_COLOR_PURPLE {128, 0, 128}  /**< 紫色 */
#define LED_COLOR_PINK {255, 192, 203}  /**< 粉色 */

/**
 * @brief 初始化LED灯带
 *
 * @return esp_err_t
 *         - ESP_OK: 初始化成功
 *         - ESP_ERR_INVALID_ARG: 参数错误
 *         - ESP_ERR_NO_MEM: 内存不足
 *         - ESP_FAIL: 其他错误
 */
esp_err_t led_init(void);

/**
 * @brief 设置指定LED的颜色
 *
 * @param index LED索引
 * @param red 红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue 蓝色分量 (0-255)
 * @return esp_err_t
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 参数错误
 *         - ESP_FAIL: 其他错误
 */
esp_err_t led_set_pixel(uint32_t index,
                        uint32_t red,
                        uint32_t green,
                        uint32_t blue);

/**
 * @brief 刷新LED灯带，将内存中的颜色数据发送到LED
 *
 * @return esp_err_t
 *         - ESP_OK: 刷新成功
 *         - ESP_FAIL: 刷新失败
 */
esp_err_t led_refresh(void);

/**
 * @brief 清除LED灯带（关闭所有LED）
 *
 * @return esp_err_t
 *         - ESP_OK: 清除成功
 *         - ESP_FAIL: 清除失败
 */
esp_err_t led_clear(void);

/**
 * @brief 设置所有LED为同一颜色
 *
 * @param red 红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue 蓝色分量 (0-255)
 * @return esp_err_t
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 参数错误
 *         - ESP_FAIL: 其他错误
 */
esp_err_t led_set_all(uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief 设置LED0的颜色（便捷函数）
 *
 * @param color 颜色结构体
 * @return esp_err_t
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 参数错误
 *         - ESP_FAIL: 其他错误
 */
esp_err_t led0_set_color(led_color_t color);

/**
 * @brief 设置LED0的颜色并立即刷新（便捷函数）
 *
 * @param color 颜色结构体
 * @return esp_err_t
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 参数错误
 *         - ESP_FAIL: 其他错误
 */
esp_err_t led0_set_color_and_refresh(led_color_t color);

/**
 * @brief 调整颜色亮度
 *
 * @param color 原始颜色
 * @param brightness 亮度百分比 (0-100)
 * @return led_color_t 调整亮度后的颜色
 */
led_color_t led_brightness(led_color_t color, uint32_t brightness);

/**
 * @brief LED颜色测试函数
 *
 * 该函数会循环设置LED为不同颜色，每种颜色持续1秒
 * 使用IO5引脚控制LED
 *
 * @param test_duration 测试持续时间（秒），0表示无限循环
 * @return esp_err_t
 *         - ESP_OK: 测试成功完成
 *         - ESP_ERR_INVALID_STATE: LED未初始化
 *         - ESP_FAIL: 其他错误
 */
esp_err_t led_color_test(uint32_t test_duration);

#ifdef __cplusplus
}
#endif