#include "actuator/ledc_actuator.hpp"
#include <esp_log.h>
#include <mutex>
#include <stdexcept>

static const char* TAG = "LEDCActuator";

namespace actuator {

LEDCActuator::LEDCActuator(int gpio_num,
                           ledc_channel_t channel,
                           ledc_timer_t timer,
                           uint32_t freq_hz,
                           float offset)
    : Actuator(offset), m_gpio_num(gpio_num), m_channel(channel), m_timer(timer), m_freq_hz(freq_hz) {
    if (freq_hz < 50 || freq_hz > 333) {
        ESP_LOGE(TAG, "Invalid frequency %dHz, must be between 50Hz and 333Hz",
                 freq_hz);
        throw std::runtime_error(
            "Invalid frequency, must be between 50Hz and 333Hz");
    }
    // 初始化LEDC
    if (!initLEDC()) {
        ESP_LOGE(TAG, "Failed to initialize LEDC actuator");
        throw std::runtime_error("Failed to initialize LEDC actuator");
    }

    ESP_LOGI(TAG, "LEDC actuator initialized successfully");
}

LEDCActuator::~LEDCActuator() {
    // 停止PWM输出
    ledc_stop(LEDC_LOW_SPEED_MODE, m_channel, 0);
    ESP_LOGI(TAG, "LEDC actuator deinitialized");
}

bool LEDCActuator::initLEDC() {
    esp_err_t ret;

    // 配置LEDC定时器
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = m_duty_resolution,
        .timer_num = m_timer,
        .freq_hz = m_freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,  // 自动选择时钟源
    };

    ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return false;
    }

    // 配置LEDC通道
    ledc_channel_config_t channel_conf = {
        .gpio_num = m_gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = m_channel,
        .intr_type = LEDC_INTR_DISABLE,  // 禁用中断
        .timer_sel = m_timer,
        .duty = 0,  // 初始占空比为0
        .hpoint = 0,
        .flags = {
            .output_invert = 0  // 不反转输出
        }};

    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

void LEDCActuator::setTarget(float target) {
    // 自动加上offset
    float target_with_offset = target + m_offset;
    
    // 限制目标值在[-1, 1]范围内
    if (target_with_offset < -1.0f) {
        m_target = -1.0f;
    } else if (target_with_offset > 1.0f) {
        m_target = 1.0f;
    } else {
        m_target = target_with_offset;
    }

    // 调用具体的执行实现，不等待
    actuate(0);
}

bool LEDCActuator::actuate(int wait) {
    // ledc是无反馈的，忽略等待参数
    std::lock_guard<std::mutex> lock(m_mutex);

    // 获取当前目标值
    float target = getTarget();

    // 将目标值转换为PWM占空比
    uint32_t duty = targetToDuty(target);

    // 设置占空比
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, m_channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty: %s", esp_err_to_name(ret));
        return false;
    }
    // 更新占空比
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, m_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGD(TAG, "Set target %.2f to duty %u", target, duty);
    return true;
}

uint32_t LEDCActuator::targetToDuty(float target) {
    // 将-1到1的输入映射到500-2500us的高电平持续时间
    // PWM周期 = 1/freq_hz (秒)
    // 高电平时间范围：500us到2500us
    // 占空比 = 高电平时间 / PWM周期

    float period_us = 1000000.0f / m_freq_hz;  // PWM周期(微秒)
    float pulse_width_us;                      // 高电平持续时间(微秒)

    // 线性映射：-1 -> 500us, 0 -> 1500us, 1 -> 2500us
    // 公式：pulse_width = 1500 + target * 1000
    pulse_width_us = 1500.0f + target * 1000.0f;

    // 限制范围在500-2500us之间
    if (pulse_width_us < 500.0f) {
        pulse_width_us = 500.0f;
    } else if (pulse_width_us > 2500.0f) {
        pulse_width_us = 2500.0f;
    }

    // 计算占空比值
    // duty = (pulse_width_us / period_us) * (2^resolution - 1)
    float duty_float =
        (pulse_width_us / period_us) * ((1 << m_duty_resolution) - 1);

    // 限制在有效范围内
    uint32_t max_duty = (1 << m_duty_resolution) - 1;
    if (duty_float < 0) {
        duty_float = 0;
    } else if (duty_float > max_duty) {
        duty_float = max_duty;
    }

    return static_cast<uint32_t>(duty_float);
}

}  // namespace actuator