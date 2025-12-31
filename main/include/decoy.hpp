#pragma once

#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "mutex"
#include "soc/gpio_num.h"
#include "esp_err.h"

/**
 * @brief 电压等级枚举
 */
enum class VoltageLevel {
    V9 = 1,   // 9V
    V12 = 2,  // 12V
    V15 = 3   // 15V
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化Decoy（电压控制模块）
 *
 * @return esp_err_t
 *         - ESP_OK: 初始化成功
 *         - ESP_FAIL: 初始化失败
 */
esp_err_t decoy_init(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief 电压控制类（单例模式）
 *
 * 通过控制 MOD1、MOD2 引脚和读取 ADC 值来实现电压控制和监测
 * IO5: ADC 输入（ADC1_CHANNEL_0）
 * IO6: MOD1 控制
 * IO7: MOD2 控制
 * MOD3: 电路上拉固定为高电平
 */
class Decoy {
   public:
    /**
     * @brief 获取单例实例
     * @return Decoy单例实例的指针
     */
    static Decoy* getInstance();

    /**
     * @brief 析构函数
     */
    ~Decoy();

    /**
     * @brief 设置目标电压
     * @param level 目标电压等级
     * @return true 设置成功，false 设置失败
     */
    bool setVoltage(VoltageLevel level);

    /**
     * @brief 获取当前电压
     * @return 当前电压（V）
     */
    float getVoltage();

    // 删除拷贝构造和拷贝赋值
    Decoy(const Decoy&) = delete;
    Decoy& operator=(const Decoy&) = delete;

   private:
    /**
     * @brief 初始化 GPIO
     * @return true 初始化成功，false 初始化失败
     */
    bool initGPIO();

    /**
     * @brief 初始化 ADC
     * @return true 初始化成功，false 初始化失败
     */
    bool initADC();

    /**
     * @brief 初始化定时器
     * @return true 初始化成功，false 初始化失败
     */
    bool initTimer();

    /**
     * @brief 定时器回调函数 - 采样ADC
     * @param timer 定时器句柄
     */
    static void timerCallback(TimerHandle_t timer);

    // GPIO 引脚定义
    static constexpr gpio_num_t PIN_ADC_EN = GPIO_NUM_5;  // ADC 使能引脚（实际用于控制）
    static constexpr gpio_num_t PIN_MOD1 = GPIO_NUM_6;    // MOD1 控制引脚
    static constexpr gpio_num_t PIN_MOD2 = GPIO_NUM_7;    // MOD2 控制引脚

    // ADC 配置
    static constexpr adc_channel_t ADC_CHANNEL = ADC_CHANNEL_4;
    static constexpr adc_atten_t ADC_ATTEN = ADC_ATTEN_DB_12;  // 最大衰减
    static constexpr adc_bitwidth_t ADC_BITWIDTH = ADC_BITWIDTH_DEFAULT;
    static constexpr int ADC_SAMPLES = 8;                      // 采样次数
    static constexpr int TIMER_INTERVAL_MS = 10;                // 定时器间隔（毫秒）

    // 电压组合配置表 [MOD1, MOD2, MOD3]
    // 使用 VoltageLevel 枚举值作为数组下标访问
    static constexpr bool VOLTAGE_CONFIGS[4][3] = {
        {0, 1, 1},  // V9:  9V   (MOD1=0, MOD2=1, MOD3=1)
        {1, 0, 1},  // V12: 12V   (MOD1=1, MOD2=0, MOD3=1)
        {0, 0, 1}   // V15: 15V   (MOD1=0, MOD2=0, MOD3=1)
    };

    // ADC 校准系数
    static constexpr float V_REF = 3.3f;
    static constexpr float ADC_RAW_MAX = 4095.0f;
    static constexpr float RESISTANCE_RATIO = 11.0f;    // 分压比
    static constexpr float CALIBRATION_FACTOR = 20.0f / 21.67f;  // 校准系数

    std::mutex m_mutex;         // 互斥锁
    bool m_initialized = false;  // 初始化状态

    // ADC采样相关
    adc_oneshot_unit_handle_t m_adc_handle = nullptr;  // ADC oneshot句柄
    int m_adc_raw_value = 0;      // 最近一次ADC采样原始值
    float m_voltage_value = 0.0f;    // 最近一次计算的电压值
    TimerHandle_t m_timer = nullptr;   // FreeRTOS定时器句柄

    // 私有构造函数（单例模式）
    Decoy();
};

