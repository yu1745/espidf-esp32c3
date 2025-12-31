#include "led.hpp"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

namespace {
const char* TAG = "led";
// LED配置
#define LED_STRIP_GPIO_PIN 5                         // LED灯带连接的GPIO引脚
#define LED_STRIP_LED_COUNT 1                        // LED灯带中LED的数量
#define LED_STRIP_RMT_RES_HZ 1 * (10 * 1000 * 1000)  // RMT分辨率，10MHz

// LED灯带句柄
static led_strip_handle_t g_led_strip = nullptr;
}  // namespace

esp_err_t led_init(void) {
    // 检查是否已经初始化
    if (g_led_strip != nullptr) {
        ESP_LOGW(TAG, "LED strip already initialized");
        return ESP_OK;
    }

    // LED灯带通用初始化配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,  // 连接到LED灯带数据线的GPIO
        .max_leds = LED_STRIP_LED_COUNT,       // LED灯带中LED的数量
        .led_model = LED_MODEL_WS2812,         // LED灯带型号
        .color_component_format =
            LED_STRIP_COLOR_COMPONENT_FMT_GRB,  // LED灯带颜色顺序：GRB
        .flags = {
            .invert_out = false,  // 不反转输出信号
        }};

    // LED灯带后端配置：RMT
    // led_strip_rmt_config_t rmt_config = {
    //     .clk_src = RMT_CLK_SRC_DEFAULT,         // 时钟源
    //     .resolution_hz = LED_STRIP_RMT_RES_HZ,  // RMT计数器时钟频率
    //     .mem_block_symbols = 48,
    //     .flags = {
    //         .with_dma = false,  // 不使用DMA
    //     }};

    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .spi_bus = SPI2_HOST,           // SPI bus ID
        .flags = {
            .with_dma = false, // Using DMA can improve performance and help drive more LEDs
        }
    };

    // 创建LED灯带对象
    esp_err_t ret =
        led_strip_new_spi_device(&strip_config, &spi_config, &g_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    // // 清除LED灯带（关闭所有LED）
    // ret = led_strip_clear(g_led_strip);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(ret));
    //     return ret;
    // }

    // // 刷新LED灯带
    // ret = led_strip_refresh(g_led_strip);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to refresh LED strip: %s",
    //     esp_err_to_name(ret)); return ret;
    // }

    ESP_LOGI(TAG, "LED strip initialized successfully");
    return ESP_OK;
}

esp_err_t led_set_pixel(uint32_t index,
                        uint32_t red,
                        uint32_t green,
                        uint32_t blue) {
    if (g_led_strip == nullptr) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (index >= LED_STRIP_LED_COUNT) {
        ESP_LOGE(TAG, "LED index out of range: %lu >= %d", index,
                 LED_STRIP_LED_COUNT);
        return ESP_ERR_INVALID_ARG;
    }

    return led_strip_set_pixel(g_led_strip, index, red, green, blue);
}

esp_err_t led_refresh(void) {
    if (g_led_strip == nullptr) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    return led_strip_refresh(g_led_strip);
}

esp_err_t led_clear(void) {
    if (g_led_strip == nullptr) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    return led_strip_clear(g_led_strip);
}

esp_err_t led_set_all(uint32_t red, uint32_t green, uint32_t blue) {
    if (g_led_strip == nullptr) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    for (uint32_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        esp_err_t err = led_strip_set_pixel(g_led_strip, i, red, green, blue);
        if (err != ESP_OK) {
            ret = err;
            ESP_LOGE(TAG, "Failed to set pixel %lu: %s", i,
                     esp_err_to_name(err));
        }
    }

    return ret;
}

esp_err_t led0_set_color(led_color_t color) {
    // 直接调用led_set_pixel，index固定为0（LED0）
    return led_set_pixel(0, color.red, color.green, color.blue);
}

esp_err_t led0_set_color_and_refresh(led_color_t color) {
    esp_err_t ret = led0_set_color(color);
    if (ret != ESP_OK) {
        return ret;
    }

    // 立即刷新LED
    return led_refresh();
}

led_color_t led_brightness(led_color_t color, uint32_t brightness) {
    // 限制亮度范围在0-100之间
    if (brightness > 100) {
        brightness = 100;
    }

    led_color_t result;
    // 计算调整后的RGB值
    result.red = (color.red * brightness) / 100;
    result.green = (color.green * brightness) / 100;
    result.blue = (color.blue * brightness) / 100;

    return result;
}

esp_err_t led_color_test(uint32_t test_duration) {
    if (g_led_strip == nullptr) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 定义要测试的颜色数组
    led_color_t test_colors[] = {
        LED_COLOR_GREEN,    LED_COLOR_RED,   LED_COLOR_BLUE,  LED_COLOR_YELLOW,
        LED_COLOR_CYAN,   LED_COLOR_MAGENTA, LED_COLOR_WHITE, LED_COLOR_ORANGE,
        LED_COLOR_PURPLE, LED_COLOR_PINK};

    size_t color_count = sizeof(test_colors) / sizeof(test_colors[0]);
    uint32_t start_time = 0;
    esp_err_t ret = ESP_OK;

    // 如果test_duration大于0，记录开始时间
    if (test_duration > 0) {
        start_time = esp_timer_get_time() / 1000;  // 转换为毫秒
    }

    ESP_LOGI(TAG, "Starting LED color test on GPIO %d", LED_STRIP_GPIO_PIN);

    while (1) {
        // 循环显示每种颜色
        for (size_t i = 0; i < color_count; i++) {
            // 设置LED颜色
            ret = led_set_all(test_colors[i].red, test_colors[i].green,
                              test_colors[i].blue);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set LED color: %s",
                         esp_err_to_name(ret));
                return ret;
            }

            // 刷新LED
            ret = led_refresh();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to refresh LED: %s",
                         esp_err_to_name(ret));
                return ret;
            }

            ESP_LOGI(TAG, "LED color: R=%lu, G=%lu, B=%lu", test_colors[i].red,
                     test_colors[i].green, test_colors[i].blue);

            // 延时1秒
            vTaskDelay(pdMS_TO_TICKS(1000));

            // 检查是否达到测试持续时间
            if (test_duration > 0) {
                uint32_t current_time = esp_timer_get_time() / 1000;
                if ((current_time - start_time) >= (test_duration * 1000)) {
                    ESP_LOGI(TAG, "LED color test completed");
                    // 关闭LED
                    led_clear();
                    led_refresh();
                    return ESP_OK;
                }
            }
        }
    }

    return ret;
}