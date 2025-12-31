#include "executor/osr_executor.hpp"
#include "utils.hpp"
#include "esp_log.h"
#include "driver/ledc.h"
#include <stdexcept>

const char* OSRExecutor::TAG = "OSRExecutor";

OSRExecutor::OSRExecutor(const SettingWrapper& setting)
    : Executor(setting),
      m_servo_a_duty(0),
      m_servo_b_duty(0),
      m_servo_c_duty(0),
      m_servo_d_duty(0) {
    try {
        ESP_LOGI(TAG, "OSRExecutor() constructing...");
        //TODO 暂时
        // m_setting.printServoSetting();

        // 初始化LEDC定时器
        if (!initLEDC()) {
            ESP_LOGE(TAG, "Failed to initialize LEDC");
            throw std::runtime_error("Failed to initialize LEDC");
        }

        // 创建舵机A执行器
        if (m_setting->servo.A_SERVO_PIN != -1) {
            float offset_a = ((float)m_setting->servo.A_SERVO_ZERO - 1500.0f) / 1000.0f;
            m_servo_a = std::make_unique<actuator::LEDCActuator>(
                m_setting->servo.A_SERVO_PIN, LEDC_CHANNEL_0, LEDC_TIMER_0,
                m_setting->servo.A_SERVO_PWM_FREQ, offset_a);
            ESP_LOGI(TAG, "Servo A initialized on GPIO %d, offset: %.3f",
                     m_setting->servo.A_SERVO_PIN, offset_a);
        }

        // 创建舵机B执行器
        if (m_setting->servo.B_SERVO_PIN != -1) {
            float offset_b = ((float)m_setting->servo.B_SERVO_ZERO - 1500.0f) / 1000.0f;
            m_servo_b = std::make_unique<actuator::LEDCActuator>(
                m_setting->servo.B_SERVO_PIN, LEDC_CHANNEL_1, LEDC_TIMER_0,
                m_setting->servo.B_SERVO_PWM_FREQ, offset_b);
            ESP_LOGI(TAG, "Servo B initialized on GPIO %d, offset: %.3f",
                     m_setting->servo.B_SERVO_PIN, offset_b);
        }

        // 创建舵机C执行器
        if (m_setting->servo.C_SERVO_PIN != -1) {
            float offset_c = ((float)m_setting->servo.C_SERVO_ZERO - 1500.0f) / 1000.0f;
            m_servo_c = std::make_unique<actuator::LEDCActuator>(
                m_setting->servo.C_SERVO_PIN, LEDC_CHANNEL_2, LEDC_TIMER_0,
                m_setting->servo.C_SERVO_PWM_FREQ, offset_c);
            ESP_LOGI(TAG, "Servo C initialized on GPIO %d, offset: %.3f",
                     m_setting->servo.C_SERVO_PIN, offset_c);
        }

        // 创建舵机D执行器
        if (m_setting->servo.D_SERVO_PIN != -1) {
            float offset_d = ((float)m_setting->servo.D_SERVO_ZERO - 1500.0f) / 1000.0f;
            m_servo_d = std::make_unique<actuator::LEDCActuator>(
                m_setting->servo.D_SERVO_PIN, LEDC_CHANNEL_3, LEDC_TIMER_0,
                m_setting->servo.D_SERVO_PWM_FREQ, offset_d);
            ESP_LOGI(TAG, "Servo D initialized on GPIO %d, offset: %.3f",
                     m_setting->servo.D_SERVO_PIN, offset_d);
        }

        // 回中 - 初始计算一次
        compute();

        ESP_LOGI(TAG, "OSRExecutor initialized successfully");
    } catch (...) {
        ESP_LOGE(TAG, "Failed to construct OSRExecutor");
        // 清理已分配的资源
        m_servo_a.reset();
        m_servo_b.reset();
        m_servo_c.reset();
        m_servo_d.reset();
        throw;
    }
}

OSRExecutor::~OSRExecutor() {
    ESP_LOGI(TAG, "~OSRExecutor() deconstructing...");
    m_servo_a.reset();
    m_servo_b.reset();
    m_servo_c.reset();
    m_servo_d.reset();
    ESP_LOGI(TAG, "OSRExecutor destroyed");
}

bool OSRExecutor::initLEDC() {
    esp_err_t ret;

    // 配置LEDC定时器0
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = static_cast<uint32_t>(m_setting->servo.A_SERVO_PWM_FREQ),
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer0 config failed: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

void OSRExecutor::compute() {
    std::lock_guard<std::mutex> lock(m_compute_mutex);

    // 从tcode中获取插值后的轴值
    // 插值结果顺序为：L0, L1, L2, R0, R1, R2
    float* interpolated = tcode.interpolate();
    float stroke_input = interpolated[0];  // L0
    float roll_input = interpolated[4];    // R1
    float pitch_input = interpolated[5];   // R2
    float twist_input = interpolated[3];    // R0

    ESP_LOGD(TAG,
             "Input - stroke: %.2f, roll: %.2f, pitch: %.2f, twist: %.2f",
             stroke_input, roll_input, pitch_input, twist_input);

    // 计算stroke（行程）
    float stroke = map_(stroke_input, 0.0f, 1.0f, m_setting->servo.L0_LEFT,
                        m_setting->servo.L0_RIGHT);
    if (m_setting->servo.L0_REVERSE) {
        stroke = m_setting->servo.L0_LEFT + m_setting->servo.L0_RIGHT - stroke;
    }
    stroke = map_(stroke, 0.0f, 1.0f, -0.35f, 0.35f) * m_setting->servo.L0_SCALE;

    // 计算roll（滚转）
    float roll = map_(roll_input, 0.0f, 1.0f, m_setting->servo.R1_LEFT,
                      m_setting->servo.R1_RIGHT);
    if (m_setting->servo.R1_REVERSE) {
        roll = m_setting->servo.R1_LEFT + m_setting->servo.R1_RIGHT - roll;
    }
    roll = map_(roll, 0.0f, 1.0f, -0.18f, 0.18f) * m_setting->servo.R1_SCALE;

    // 计算pitch（俯仰）
    float pitch = map_(pitch_input, 0.0f, 1.0f, m_setting->servo.R2_LEFT,
                       m_setting->servo.R2_RIGHT);
    if (m_setting->servo.R2_REVERSE) {
        pitch = m_setting->servo.R2_LEFT + m_setting->servo.R2_RIGHT - pitch;
    }
    pitch = map_(pitch, 0.0f, 1.0f, -0.35f, 0.35f) * m_setting->servo.R2_SCALE;

    // 计算twist（扭转）
    float twist = map_(twist_input, 0.0f, 1.0f, m_setting->servo.R0_LEFT,
                       m_setting->servo.R0_RIGHT);
    if (m_setting->servo.R0_REVERSE) {
        twist = m_setting->servo.R0_LEFT + m_setting->servo.R0_RIGHT - twist;
    }
    twist = map_(twist, 0.0f, 1.0f, -1.0f, 1.0f) * m_setting->servo.R0_SCALE;

    ESP_LOGD(TAG, "Motion - stroke: %.3f, roll: %.3f, pitch: %.3f, twist: %.3f",
             stroke, roll, pitch, twist);

    // 计算各个舵机的目标值（转换为LEDCActuator所需的-1到1范围）
    // offset已经在构造函数中设置，setTarget时会自动加上offset
    // 所以这里只需要计算相对运动量，不需要再减去1.5f

    float servo_a_target = -stroke + roll;
    float servo_b_target = stroke + roll;
    float servo_c_target = pitch;
    float servo_d_target = twist;

    m_servo_a_duty = servo_a_target;
    m_servo_b_duty = servo_b_target;
    m_servo_c_duty = servo_c_target;
    m_servo_d_duty = servo_d_target;

    // ESP_LOGD(TAG, "Duty - A: %.3f, B: %.3f, C: %.3f, D: %.3f",
    //          m_servo_a_duty, m_servo_b_duty, m_servo_c_duty, m_servo_d_duty);

}

void OSRExecutor::execute() {
    std::lock_guard<std::mutex> lock(m_compute_mutex);

    // 将占空比应用到各个舵机
    if (m_servo_a) {
        m_servo_a->setTarget(m_servo_a_duty);
    }
    if (m_servo_b) {
        m_servo_b->setTarget(m_servo_b_duty);
    }
    if (m_servo_c) {
        m_servo_c->setTarget(m_servo_c_duty);
    }
    if (m_servo_d) {
        m_servo_d->setTarget(m_servo_d_duty);
    }
}

