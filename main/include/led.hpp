#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "mutex"
#include "led_strip.h"

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
 * @brief 初始化LED（LED控制模块）
 *
 * @return esp_err_t
 *         - ESP_OK: 初始化成功
 *         - ESP_FAIL: 初始化失败
 */
esp_err_t led_init(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief LED控制类（单例模式）
 *
 * 负责控制LED灯带的显示模式
 * IO5: LED灯带数据线（SPI接口）
 *
 * 支持两种模式：
 * 1. 常亮模式：亮度恒为32
 * 2. 闪烁模式：亮度在0-32之间变化
 */
class Led {
public:
    /**
     * @brief 获取单例实例
     * @return Led单例实例的指针
     */
    static Led* getInstance();

    /**
     * @brief 析构函数
     */
    ~Led();

    /**
     * @brief 设置LED颜色
     * @param color 颜色结构体
     * @return true 设置成功，false 设置失败
     */
    bool setColor(led_color_t color);

    /**
     * @brief 设置为常亮模式
     * @return true 设置成功，false 设置失败
     */
    bool setSolid();

    /**
     * @brief 设置为闪烁模式
     * @param period_ms 闪烁周期（毫秒）
     * @return true 设置成功，false 设置失败
     */
    bool setBlink(uint32_t period_ms);

    /**
     * @brief 关闭LED
     * @return true 设置成功，false 设置失败
     */
    bool turnOff();

    /**
     * @brief 显示故障码
     * @param error_code 故障码（err.h中定义的错误码）
     * @return true 设置成功，false 设置失败
     * @details 故障灯为红色，恒亮度32，闪烁次数表示故障码
     */
    bool showErrorCode(uint8_t error_code);

    /**
     * @brief 设置成功状态（绿灯常亮）
     * @return true 设置成功，false 设置失败
     */
    bool setSuccess();

    /**
     * @brief 设置为开关量闪烁模式（亮-灭）
     * @param period_ms 闪烁周期（毫秒）
     * @return true 设置成功，false 设置失败
     * @details LED在亮和灭之间切换，亮和灭的时间各占周期的一半
     */
    bool setBlinkOnOff(uint32_t period_ms);

    // 删除拷贝构造和拷贝赋值
    Led(const Led&) = delete;
    Led& operator=(const Led&) = delete;

private:
    /**
     * @brief 初始化LED灯带
     * @return true 初始化成功，false 初始化失败
     */
    bool initLed();

    /**
     * @brief 初始化定时器
     * @return true 初始化成功，false 初始化失败
     */
    bool initTimer();

    /**
     * @brief 定时器回调函数 - 更新LED状态
     * @param timer 定时器句柄
     */
    static void timerCallback(TimerHandle_t timer);

    /**
     * @brief 更新LED显示
     */
    void updateLed();

    // LED模式枚举
    enum class LedMode {
        OFF,            // 关闭
        SOLID,          // 常亮
        BLINK,          // 模拟量闪烁（三角波）
        BLINK_ON_OFF,   // 开关量闪烁（亮-灭）
        ERROR_CODE      // 故障码显示模式
    };

    // LED配置
    static constexpr int LED_GPIO_PIN = 5;              // LED灯带GPIO引脚
    static constexpr uint32_t LED_COUNT = 1;            // LED数量
    static constexpr int TIMER_INTERVAL_MS = 100;       // 定时器间隔（毫秒）
    static constexpr uint32_t MAX_BRIGHTNESS = 32;      // 最大亮度（常亮亮度）
    static constexpr uint32_t ERROR_BLINK_ON_MS = 200;  // 故障码闪烁亮时间（毫秒）
    static constexpr uint32_t ERROR_BLINK_OFF_MS = 300; // 故障码闪烁灭时间（毫秒）
    static constexpr uint32_t ERROR_REPEAT_MS = 2000;   // 故障码重复间隔（毫秒）

    std::mutex m_mutex;           // 互斥锁
    bool m_initialized = false;   // 初始化状态
    led_strip_handle_t m_led_strip = nullptr;  // LED灯带句柄

    // LED控制状态
    LedMode m_mode = LedMode::OFF;     // 当前模式
    led_color_t m_color = LED_COLOR_BLACK;  // 当前颜色
    uint32_t m_blink_period_ms = 1000; // 闪烁周期（毫秒）
    uint32_t m_blink_counter = 0;      // 闪烁计数器
    uint32_t m_current_brightness = 0; // 当前亮度

    // 故障码显示相关
    uint8_t m_error_code = 0;          // 当前显示的故障码
    uint32_t m_error_timer = 0;        // 故障码显示计时器
    bool m_error_led_state = false;    // 故障码LED状态（true=亮，false=灭）
    uint8_t m_error_blink_count = 0;   // 故障码已闪烁次数

    TimerHandle_t m_timer = nullptr;   // FreeRTOS定时器句柄

    // 私有构造函数（单例模式）
    Led();
};