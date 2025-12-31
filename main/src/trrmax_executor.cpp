#include "executor/trrmax_executor.hpp"
#include "utils.hpp"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include <stdexcept>
#include <algorithm>

const char* TrRMaxExecutor::TAG = "TrRMaxExecutor";

TrRMaxExecutor::TrRMaxExecutor(const SettingWrapper& setting)
    : Executor(setting),
      m_servo_a_duty(0),
      m_servo_b_duty(0),
      m_servo_c_duty(0) {
    try {
        ESP_LOGI(TAG, "TrRMaxExecutor() constructing...");

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

        // 回中 - 初始计算一次
        compute();

        ESP_LOGI(TAG, "TrRMaxExecutor initialized successfully");
    } catch (...) {
        ESP_LOGE(TAG, "Failed to construct TrRMaxExecutor");
        // 清理已分配的资源
        m_servo_a.reset();
        m_servo_b.reset();
        m_servo_c.reset();
        throw;
    }
}

TrRMaxExecutor::~TrRMaxExecutor() {
    ESP_LOGI(TAG, "~TrRMaxExecutor() deconstructing...");
    m_servo_a.reset();
    m_servo_b.reset();
    m_servo_c.reset();
    ESP_LOGI(TAG, "TrRMaxExecutor destroyed");
}

bool TrRMaxExecutor::initLEDC() {
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

void TrRMaxExecutor::compute() {
    std::lock_guard<std::mutex> lock(m_compute_mutex);

    int64_t now = esp_timer_get_time();

    // 从tcode中获取插值后的轴值
    // 插值结果顺序为：L0, L1, L2, R0, R1, R2
    float* interpolated = tcode.interpolate();
    float stroke_input = interpolated[0];  // L0
    float roll_input = interpolated[4];    // R1
    float pitch_input = interpolated[5];  // R2

    // 处理stroke输入，映射到-40到+40范围
    float stroke = map_(stroke_input, 0.0f, 1.0f, m_setting->servo.L0_LEFT,
                        m_setting->servo.L0_RIGHT);
    if (m_setting->servo.L0_REVERSE) {
        stroke = m_setting->servo.L0_LEFT + m_setting->servo.L0_RIGHT - stroke;
    }
    stroke = map_(stroke, 0.0f, 1.0f, -50.0f, 50.0f) * m_setting->servo.L0_SCALE;

    // 处理roll输入
    float roll = map_(roll_input, 0.0f, 1.0f, m_setting->servo.R1_LEFT,
                      m_setting->servo.R1_RIGHT);
    if (m_setting->servo.R1_REVERSE) {
        roll = m_setting->servo.R1_LEFT + m_setting->servo.R1_RIGHT - roll;
    }
    roll = map_(roll, 0.0f, 1.0f, -45.0f, 45.0f) *
           m_setting->servo.R1_SCALE;  // 假设roll范围为±45度

    // 处理pitch输入
    float pitch = map_(pitch_input, 0.0f, 1.0f, m_setting->servo.R2_LEFT,
                       m_setting->servo.R2_RIGHT);
    if (m_setting->servo.R2_REVERSE) {
        pitch = m_setting->servo.R2_LEFT + m_setting->servo.R2_RIGHT - pitch;
    }
    pitch = map_(pitch, 0.0f, 1.0f, -45.0f, 45.0f) *
            m_setting->servo.R2_SCALE;  // 假设pitch范围为±45度

    // 使用testA.py中的方法计算roll和pitch反解的摇臂末端z高度
    // 直径设为80mm（根据z=80*sin(theta)公式推断）
    float R = 40.0f;  // 半径 = 直径/2 = 80/2 = 40mm
    float roll_rad = roll * M_PI / 180.0f;
    float pitch_rad = pitch * M_PI / 180.0f;

    // 计算三个挂点的z高度偏移
    float h1 = -R * sinf(roll_rad);
    float h2 = (R / 2.0f) * sinf(roll_rad) +
               (sqrtf(3.0f) / 2.0f * R) * cosf(roll_rad) * sinf(pitch_rad);
    float h3 = (R / 2.0f) * sinf(roll_rad) -
               (sqrtf(3.0f) / 2.0f * R) * cosf(roll_rad) * sinf(pitch_rad);

    // 计算每个舵机的总z高度 = stroke高度 + roll/pitch反解的z高度
    float z_a = stroke + h2;
    float z_b = stroke + h3;
    float z_c = stroke + h1;

    // 使用z=80*sin(theta)公式将z高度转换为舵机角度
    // theta = arcsin(z/80)，结果为弧度
    float theta_a_rad = asinf(std::max(-1.0f, std::min(1.0f, z_a / 80.0f)));
    float theta_b_rad = asinf(std::max(-1.0f, std::min(1.0f, z_b / 80.0f)));
    float theta_c_rad = asinf(std::max(-1.0f, std::min(1.0f, z_c / 80.0f)));

    // 将弧度转换为角度（0-180度范围）
    float theta_a_deg = theta_a_rad * 180.0f / M_PI;
    float theta_b_deg = theta_b_rad * 180.0f / M_PI;
    float theta_c_deg = theta_c_rad * 180.0f / M_PI;

    // 将角度映射到PWM脉宽（500-2500us对应0-180度）
    // PWM脉宽 = 500 + (角度/180) * 2000
    // 但这里我们使用ZERO和90度范围来计算
    float pulse_a_us =
        m_setting->servo.A_SERVO_ZERO + (theta_a_deg / 90.0f) * 1000.0f;
    float pulse_b_us =
        m_setting->servo.B_SERVO_ZERO + (theta_b_deg / 90.0f) * 1000.0f;
    float pulse_c_us =
        m_setting->servo.C_SERVO_ZERO + (theta_c_deg / 90.0f) * 1000.0f;

    // 将PWM脉宽转换为LEDCActuator所需的-1到1范围
    // LEDCActuator内部会将-1到1映射到500-2500us
    // 所以这里需要将脉宽转换为-1到1的范围
    // 脉宽范围是500-2500us，对应-1到1
    // target = (pulse_us - 1500) / 1000.0f
    m_servo_a_duty = (pulse_a_us - 1500.0f) / 1000.0f;
    m_servo_b_duty = (pulse_b_us - 1500.0f) / 1000.0f;
    m_servo_c_duty = (pulse_c_us - 1500.0f) / 1000.0f;

    // 限制在-1到1范围内
    m_servo_a_duty = std::max(-1.0f, std::min(1.0f, m_servo_a_duty));
    m_servo_b_duty = std::max(-1.0f, std::min(1.0f, m_servo_b_duty));
    m_servo_c_duty = std::max(-1.0f, std::min(1.0f, m_servo_c_duty));

    int64_t now2 = esp_timer_get_time();
    if (now2 - now > COMPUTE_TIMEOUT) {
        ESP_LOGW(TAG, "TrRMax解算耗时: %lld us", (long long)(now2 - now));
    }
}

void TrRMaxExecutor::execute() {
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
}

