#include "actuator/rmt_actuator.hpp"
#include <esp_log.h>
#include <mutex>
#include <stdexcept>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "RMTActuator";

namespace actuator {

RMTActuator::RMTActuator(int gpio_num, uint32_t tick_us)
    : m_gpio_num(gpio_num), m_tick_us(tick_us) {
    // 初始化RMT
    if (!initRMT()) {
        ESP_LOGE(TAG, "Failed to initialize RMT actuator");
        throw std::runtime_error("Failed to initialize RMT actuator");
    }

    ESP_LOGI(TAG, "RMT actuator initialized successfully");
}

RMTActuator::~RMTActuator() {
    // 删除编码器
    if (m_encoder) {
        rmt_del_encoder(m_encoder);
    }

    // 禁用RMT通道
    if (m_initialized && m_tx_channel) {
        rmt_disable(m_tx_channel);
        rmt_del_channel(m_tx_channel);
    }

    ESP_LOGI(TAG, "RMT actuator deinitialized");
}

bool RMTActuator::initRMT() {
    esp_err_t ret;

    // 配置RMT TX通道
    rmt_tx_channel_config_t tx_channel_config = {
        .gpio_num = static_cast<gpio_num_t>(m_gpio_num),  // GPIO引脚号
        .clk_src = RMT_CLK_SRC_DEFAULT,                   // 使用默认时钟源
        .resolution_hz = 1000000 / m_tick_us,  // 分辨率，根据tick_us计算
        .mem_block_symbols = 48,               // 内存块大小，64 * 4 = 256字节
        .trans_queue_depth = 4,                // 传输队列深度
        .flags = {
            .invert_out = 0,    // 不反转输出
            .with_dma = 0,      // 不使用DMA
            .io_loop_back = 0,  // 不启用回环
            .io_od_mode = 0,    // 不使用开漏模式
            .allow_pd = 0       // 不允许在睡眠时断电
        }};

    // 创建RMT通道
    ret = rmt_new_tx_channel(&tx_channel_config, &m_tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %s",
                 esp_err_to_name(ret));
        return false;
    }

    // 配置简单编码器
    rmt_simple_encoder_config_t encoder_config = {
        .callback = rmt_encoder_cb,  // 编码器回调函数
        .arg = this,                 // 传递当前对象作为参数
        .min_chunk_size = 2  // 最小块大小，至少需要2个符号(高电平和低电平)
    };

    // 创建编码器
    ret = rmt_new_simple_encoder(&encoder_config, &m_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT encoder: %s", esp_err_to_name(ret));
        return false;
    }

    // 启用RMT通道
    ret = rmt_enable(m_tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        return false;
    }

    // 配置传输参数
    m_tx_config = {.loop_count = 0,  // 不循环
                   .flags = {
                       .eot_level = 0,         // 传输结束后输出低电平
                       .queue_nonblocking = 0  // 阻塞模式，如果队列满了则等待
                   }};

    // 保存通道句柄以便后续使用
    // 由于类定义中没有保存通道句柄的成员，我们需要添加一个
    // 但为了不修改头文件，我们可以使用静态变量或其他方法
    // 这里我们暂时不保存，因为actuate方法中可能不需要

    m_initialized = true;
    return true;
}

void RMTActuator::setTarget(float target) {
    // 限制目标值在[-1, 1]范围内
    if (target < -1.0f) {
        m_target = -1.0f;
    } else if (target > 1.0f) {
        m_target = 1.0f;
    } else {
        m_target = target;
    }

    // 调用具体的执行实现，不等待
    actuate(-1);
}

bool RMTActuator::actuate(int wait) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        ESP_LOGE(TAG, "RMT not initialized");
        return false;
    }

    // 获取当前目标值
    float target = getTarget();

    // 将目标值转换为脉冲宽度
    uint32_t pulse_width = targetToPulseWidth(target);

    // 传输脉冲宽度数据
    esp_err_t ret = rmt_transmit(m_tx_channel, m_encoder, &pulse_width,
                                 sizeof(pulse_width), &m_tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit RMT data: %s", esp_err_to_name(ret));
        return false;
    }

    // 等待传输完成
    ret = rmt_tx_wait_all_done(
        m_tx_channel, wait == -1 ? portMAX_DELAY : pdMS_TO_TICKS(wait));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wait for RMT transmission: %s",
                 esp_err_to_name(ret));
        return false;
    }

    ESP_LOGD(TAG, "Set target %.2f to pulse width %u us", target, pulse_width);
    return true;
}

uint32_t RMTActuator::targetToPulseWidth(float target) {
    // 将-1到1的输入映射到500-2500us的高电平持续时间
    // 线性映射：-1 -> 500us, 0 -> 1500us, 1 -> 2500us
    // 公式：pulse_width = 1500 + target * 1000
    float pulse_width_us = 1500.0f + target * 1000.0f;

    // 限制范围在500-2500us之间
    if (pulse_width_us < 500.0f) {
        pulse_width_us = 500.0f;
    } else if (pulse_width_us > 2500.0f) {
        pulse_width_us = 2500.0f;
    }

    return static_cast<uint32_t>(pulse_width_us);
}

size_t RMTActuator::rmt_encoder_cb(const void* data,
                                   size_t data_size,
                                   size_t symbols_written,
                                   size_t symbols_free,
                                   rmt_symbol_word_t* symbols,
                                   bool* done,
                                   void* arg) {
    // 获取当前对象的指针
    RMTActuator* actuator = static_cast<RMTActuator*>(arg);

    // 检查数据大小是否正确
    if (data_size != sizeof(uint32_t)) {
        ESP_LOGE(TAG, "Invalid data size: %d, expected: %d", data_size,
                 sizeof(uint32_t));
        *done = true;
        return 0;
    }

    // 获取脉冲宽度
    uint32_t pulse_width = *static_cast<const uint32_t*>(data);

    // 检查是否有足够的空间写入符号
    // 我们需要2个符号：高电平和低电平
    if (symbols_free < 2) {
        ESP_LOGD(TAG, "Not enough symbol space: %d, required: 2", symbols_free);
        return 0;  // 返回0表示没有写入任何符号
    }

    // 计算高电平和低电平的持续时间（以RMT tick为单位）
    uint32_t high_ticks = pulse_width / actuator->m_tick_us;
    uint32_t low_ticks = 500 / actuator->m_tick_us;  // 低电平固定500us

    // 设置高电平符号
    symbols[0].duration0 = high_ticks;
    symbols[0].level0 = 1;  // 高电平
    symbols[0].duration1 = 0;
    symbols[0].level1 = 0;

    // 设置低电平符号
    symbols[1].duration0 = low_ticks;
    symbols[1].level0 = 0;  // 低电平
    symbols[1].duration1 = 0;
    symbols[1].level1 = 0;

    // 标记编码完成
    *done = true;

    ESP_LOGD(TAG, "Encoded pulse width %u us to %u high ticks and %u low ticks",
             pulse_width, high_ticks, low_ticks);

    // 返回写入的符号数量
    return 2;
}

void testRMTActuator(int gpio_num, uint32_t channel, uint32_t tick_us) {
    ESP_LOGI(TAG, "Testing RMT Actuator on GPIO %d", gpio_num);
    try {
        // 在堆上创建RMT执行器，确保它在任务执行期间一直存在
        RMTActuator* rmt_actuator = new RMTActuator(gpio_num, tick_us);

        // 创建测试任务
        xTaskCreate(
            [](void* arg) {
                RMTActuator* actuator = static_cast<RMTActuator*>(arg);
                float target = -1.0f;
                bool increasing = true;

                while (true) {
                    // 设置目标值
                    actuator->setTarget(target);
                    // ESP_LOGI(TAG, "RMT Actuator set to: %.2f", target);

                    // 更新目标值
                    if (increasing) {
                        target += 0.04f;  // 步长20ms，周期1s，所以每次变化0.04
                                          // (2.0/50)
                        if (target >= 1.0f) {
                            target = 1.0f;
                            increasing = false;
                        }
                    } else {
                        target -= 0.04f;
                        if (target <= -1.0f) {
                            target = -1.0f;
                            increasing = true;
                        }
                    }

                    // 延迟20ms
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            },
            "rmt_test", 4096, rmt_actuator, 5, NULL);

        ESP_LOGI(TAG, "RMT Actuator test started");
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to create RMT Actuator: %s", e.what());
    }
}

}  // namespace actuator