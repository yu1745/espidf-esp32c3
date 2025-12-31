#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "mutex"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 电压事件基声明
ESP_EVENT_DECLARE_BASE(VOLTAGE_EVENT);

// 电压事件ID定义
typedef enum {
    VOLTAGE_EVENT_READING = 0,  /**< 电压读数 */
} voltage_event_id_t;

// 电压事件数据结构
typedef struct {
    float voltage;        /**< 电压值（V） */
    int adc_raw;          /**< ADC原始值 */
    int64_t timestamp;    /**< 时间戳（微秒） */
} voltage_reading_event_data_t;

/**
 * @brief 初始化Voltage（电压读取模块）
 *
 * @return esp_err_t
 *         - ESP_OK: 初始化成功
 *         - ESP_FAIL: 初始化失败
 */
esp_err_t voltage_init(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief 电压读取类（单例模式）
 *
 * 负责通过 ADC 定时采样并计算实际电压值
 * IO4: ADC 输入（ADC1_CHANNEL_4）
 */
class Voltage {
   public:
    /**
     * @brief 获取单例实例
     * @return Voltage单例实例的指针
     */
    static Voltage* getInstance();

    /**
     * @brief 析构函数
     */
    ~Voltage();

    /**
     * @brief 获取当前电压
     * @return 当前电压（V）
     */
    float getVoltage();

    /**
     * @brief 获取ADC原始值
     * @return ADC原始采样值
     */
    int getAdcRawValue();

    // 删除拷贝构造和拷贝赋值
    Voltage(const Voltage&) = delete;
    Voltage& operator=(const Voltage&) = delete;

   private:
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

    // ADC 配置
    static constexpr adc_channel_t ADC_CHANNEL = ADC_CHANNEL_4;
    static constexpr adc_atten_t ADC_ATTEN = ADC_ATTEN_DB_12;  // 最大衰减
    static constexpr adc_bitwidth_t ADC_BITWIDTH = ADC_BITWIDTH_DEFAULT;
    static constexpr int ADC_SAMPLES = 8;                      // 采样次数
    static constexpr int TIMER_INTERVAL_MS = 10;                // 定时器间隔（毫秒）

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
    Voltage();
};
