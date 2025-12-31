#include "led.hpp"
#include "esp_log.h"
#include "stdexcept"

static const char* TAG = "Led";

// 静态单例实例指针
static Led* s_singleton_instance = nullptr;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t led_init(void) {
    try {
        Led::getInstance();
        ESP_LOGI(TAG, "Led module initialized successfully");
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Led module初始化失败: %s", e.what());
        return ESP_FAIL;
    }
}

#ifdef __cplusplus
}
#endif

Led* Led::getInstance() {
    if (s_singleton_instance == nullptr) {
        s_singleton_instance = new Led();
    }
    return s_singleton_instance;
}

Led::Led() {
    try {
        ESP_LOGI(TAG, "Led() constructing...");

        // 初始化LED灯带
        if (!initLed()) {
            ESP_LOGE(TAG, "Failed to initialize LED strip");
            throw std::runtime_error("Failed to initialize LED strip");
        }

        // 初始化定时器
        if (!initTimer()) {
            ESP_LOGE(TAG, "Failed to initialize timer");
            throw std::runtime_error("Failed to initialize timer");
        }

        m_initialized = true;
        ESP_LOGI(TAG, "Led initialized successfully");
    } catch (...) {
        ESP_LOGE(TAG, "Failed to construct Led");
        // 清理已分配的资源
        if (m_timer != nullptr) {
            xTimerDelete(m_timer, 0);
            m_timer = nullptr;
        }
        if (m_led_strip != nullptr) {
            led_strip_del(m_led_strip);
            m_led_strip = nullptr;
        }
        throw;
    }
}

Led::~Led() {
    ESP_LOGI(TAG, "~Led() deconstructing...");

    // 停止并删除定时器
    if (m_timer != nullptr) {
        if (xTimerStop(m_timer, pdMS_TO_TICKS(100)) == pdFAIL) {
            ESP_LOGW(TAG, "Failed to stop timer");
        }
        if (xTimerDelete(m_timer, pdMS_TO_TICKS(100)) == pdFAIL) {
            ESP_LOGW(TAG, "Failed to delete timer");
        }
        m_timer = nullptr;
    }

    // 清理LED灯带资源
    if (m_led_strip != nullptr) {
        // 清除LED
        led_strip_clear(m_led_strip);
        led_strip_refresh(m_led_strip);

        if (led_strip_del(m_led_strip) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to delete LED strip");
        }
        m_led_strip = nullptr;
    }

    ESP_LOGI(TAG, "Led destroyed");
}

bool Led::initLed() {
    // LED灯带通用初始化配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_PIN,        // 连接到LED灯带数据线的GPIO
        .max_leds = LED_COUNT,                 // LED灯带中LED的数量
        .led_model = LED_MODEL_WS2812,         // LED灯带型号
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,  // LED灯带颜色顺序：GRB
        .flags = {
            .invert_out = false,               // 不反转输出信号
        }
    };

    // LED灯带后端配置：SPI
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,  // 时钟源
        .spi_bus = SPI2_HOST,             // SPI总线ID
        .flags = {
            .with_dma = false,            // 不使用DMA
        }
    };

    // 创建LED灯带对象
    esp_err_t ret = led_strip_new_spi_device(&strip_config, &spi_config, &m_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return false;
    }

    // 清除LED灯带（关闭所有LED）
    ret = led_strip_clear(m_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(ret));
        led_strip_del(m_led_strip);
        m_led_strip = nullptr;
        return false;
    }

    // 刷新LED灯带
    ret = led_strip_refresh(m_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(ret));
        led_strip_del(m_led_strip);
        m_led_strip = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "LED strip initialized: GPIO=%d, LED count=%lu", LED_GPIO_PIN, LED_COUNT);
    return true;
}

bool Led::initTimer() {
    // 创建软件定时器
    m_timer = xTimerCreate("led_timer",                   // 定时器名称
                           pdMS_TO_TICKS(TIMER_INTERVAL_MS),  // 定时器周期（毫秒）
                           pdTRUE,                        // 自动重载
                           nullptr,                       // 定时器ID
                           timerCallback                  // 回调函数
    );

    if (m_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create timer");
        return false;
    }

    // 启动定时器
    if (xTimerStart(m_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        xTimerDelete(m_timer, 0);
        m_timer = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Timer initialized: interval=%dms", TIMER_INTERVAL_MS);
    return true;
}

void Led::timerCallback(TimerHandle_t timer) {
    if (s_singleton_instance == nullptr) {
        return;
    }

    // 更新LED状态
    s_singleton_instance->updateLed();
}

void Led::updateLed() {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t brightness = 0;

    switch (m_mode) {
        case LedMode::OFF:
            brightness = 0;
            break;

        case LedMode::SOLID:
            brightness = MAX_BRIGHTNESS;
            break;

        case LedMode::BLINK: {
            // 计算当前亮度（三角波：0 -> MAX_BRIGHTNESS -> 0）
            uint32_t steps = m_blink_period_ms / TIMER_INTERVAL_MS;  // 总步数
            uint32_t half_steps = steps / 2;  // 半周期步数

            // 使用三角波计算亮度
            if (m_blink_counter < half_steps) {
                // 上升沿：0 -> MAX_BRIGHTNESS
                brightness = (m_blink_counter * MAX_BRIGHTNESS) / half_steps;
            } else {
                // 下降沿：MAX_BRIGHTNESS -> 0
                brightness = ((steps - m_blink_counter) * MAX_BRIGHTNESS) / half_steps;
            }

            // 更新计数器
            m_blink_counter++;
            if (m_blink_counter >= steps) {
                m_blink_counter = 0;
            }
            break;
        }

        case LedMode::BLINK_ON_OFF: {
            // 开关量闪烁模式（亮-灭）
            uint32_t half_period_ms = m_blink_period_ms / 2;  // 半周期（毫秒）

            // 计算当前处于周期的哪个半周期
            uint32_t elapsed_time = (m_blink_counter * TIMER_INTERVAL_MS) % m_blink_period_ms;

            if (elapsed_time < half_period_ms) {
                // 前半周期：亮
                brightness = MAX_BRIGHTNESS;
            } else {
                // 后半周期：灭
                brightness = 0;
            }

            // 更新计数器
            m_blink_counter++;
            break;
        }

        case LedMode::ERROR_CODE: {
            // 故障码显示模式
            // 周期：闪烁N次（N=故障码） -> 等待 -> 重复
            m_error_timer += TIMER_INTERVAL_MS;

            // 计算一个完整闪烁周期的时间
            uint32_t blink_cycle_time = (ERROR_BLINK_ON_MS + ERROR_BLINK_OFF_MS) * m_error_code;

            if (m_error_timer < blink_cycle_time) {
                // 在闪烁周期内
                uint32_t current_blink_time = m_error_timer % (ERROR_BLINK_ON_MS + ERROR_BLINK_OFF_MS);

                if (current_blink_time < ERROR_BLINK_ON_MS) {
                    // 亮
                    if (!m_error_led_state) {
                        m_error_led_state = true;
                        m_error_blink_count++;
                    }
                    brightness = MAX_BRIGHTNESS;
                } else {
                    // 灭
                    m_error_led_state = false;
                    brightness = 0;
                }
            } else if (m_error_timer < (blink_cycle_time + ERROR_REPEAT_MS)) {
                // 等待间隔
                brightness = 0;
                m_error_led_state = false;
            } else {
                // 重新开始
                m_error_timer = 0;
                m_error_blink_count = 0;
                brightness = 0;
            }
            break;
        }
    }

    // 保存当前亮度
    m_current_brightness = brightness;

    // 计算实际RGB值（应用亮度）
    uint32_t red = (m_color.red * brightness) / 255;
    uint32_t green = (m_color.green * brightness) / 255;
    uint32_t blue = (m_color.blue * brightness) / 255;

    // 设置LED颜色
    esp_err_t ret = led_strip_set_pixel(m_led_strip, 0, red, green, blue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pixel: %s", esp_err_to_name(ret));
        return;
    }

    // 刷新LED
    ret = led_strip_refresh(m_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGD(TAG, "LED updated: mode=%d, brightness=%lu, R=%lu, G=%lu, B=%lu",
             static_cast<int>(m_mode), brightness, red, green, blue);
}

bool Led::setColor(led_color_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_color = color;
    ESP_LOGI(TAG, "Color set: R=%lu, G=%lu, B=%lu", color.red, color.green, color.blue);
    return true;
}

bool Led::setSolid() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mode = LedMode::SOLID;
    ESP_LOGI(TAG, "Mode set to SOLID");
    return true;
}

bool Led::setBlink(uint32_t period_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mode = LedMode::BLINK;
    m_blink_period_ms = period_ms;
    m_blink_counter = 0;  // 重置计数器
    ESP_LOGI(TAG, "Mode set to BLINK: period=%lu ms", period_ms);
    return true;
}

bool Led::setBlinkOnOff(uint32_t period_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mode = LedMode::BLINK_ON_OFF;
    m_blink_period_ms = period_ms;
    m_blink_counter = 0;  // 重置计数器
    ESP_LOGI(TAG, "Mode set to BLINK_ON_OFF: period=%lu ms", period_ms);
    return true;
}

bool Led::turnOff() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mode = LedMode::OFF;
    ESP_LOGI(TAG, "Mode set to OFF");
    return true;
}

bool Led::showErrorCode(uint8_t error_code) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (error_code == 0) {
        ESP_LOGW(TAG, "Error code is 0, treating as no error");
        return setSuccess();
    }

    m_mode = LedMode::ERROR_CODE;
    m_error_code = error_code;
    m_error_timer = 0;
    m_error_led_state = false;
    m_error_blink_count = 0;
    m_color = LED_COLOR_RED;  // 故障灯为红色

    ESP_LOGI(TAG, "Error code display: 0x%02X (%d blinks)", error_code, error_code);
    return true;
}

bool Led::setSuccess() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mode = LedMode::SOLID;
    m_color = LED_COLOR_GREEN;  // 成功为绿色

    ESP_LOGI(TAG, "Status set to SUCCESS (green solid)");
    return true;
}
