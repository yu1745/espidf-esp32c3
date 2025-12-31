#include "executor/o6_executor.hpp"
#include "esp_log.h"
#include <cmath>

const char* O6Executor::TAG = "O6Executor";

O6Executor::O6Executor(const SettingWrapper& setting)
    : Executor(setting),
      m_theta_values{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      m_servo_a_target(0.0f),
      m_servo_b_target(0.0f),
      m_servo_c_target(0.0f),
      m_servo_d_target(0.0f),
      m_servo_e_target(0.0f),
      m_servo_f_target(0.0f) {
    ESP_LOGI(TAG, "O6Executor Constructor");

    // Initialize all 6 servo channels
    if (!initLEDC()) {
        ESP_LOGE(TAG, "Failed to initialize LEDC");
        throw std::runtime_error("Failed to initialize LEDC");
    }

    m_init_done = true;
    ESP_LOGI(TAG, "O6Executor initialized successfully");
}

O6Executor::~O6Executor() {
    ESP_LOGI(TAG, "O6Executor Destructor");
}

bool O6Executor::initLEDC() {
    // Get servo frequency from settings (use A_SERVO_PWM_FREQ as reference)
    uint32_t freq_hz = m_setting->servo.A_SERVO_PWM_FREQ;

    // Initialize 6 servos with LEDC channels 0-5 and timers 0-5
    // Use gpio -1 to disable unused servos

    // Servo A - Channel 0, Timer 0
    if (m_setting->servo.A_SERVO_PIN >= 0) {
        m_servo_a = std::make_unique<actuator::LEDCActuator>(
            m_setting->servo.A_SERVO_PIN,
            LEDC_CHANNEL_0,
            LEDC_TIMER_0,
            freq_hz,
            0.0f);
        ESP_LOGI(TAG, "Servo A initialized on GPIO %d", m_setting->servo.A_SERVO_PIN);
    }

    // Servo B - Channel 1, Timer 1
    if (m_setting->servo.B_SERVO_PIN >= 0) {
        m_servo_b = std::make_unique<actuator::LEDCActuator>(
            m_setting->servo.B_SERVO_PIN,
            LEDC_CHANNEL_1,
            LEDC_TIMER_0,
            freq_hz,
            0.0f);
        ESP_LOGI(TAG, "Servo B initialized on GPIO %d", m_setting->servo.B_SERVO_PIN);
    }

    // Servo C - Channel 2, Timer 2
    if (m_setting->servo.C_SERVO_PIN >= 0) {
        m_servo_c = std::make_unique<actuator::LEDCActuator>(
            m_setting->servo.C_SERVO_PIN,
            LEDC_CHANNEL_2,
            LEDC_TIMER_0,
            freq_hz,
            0.0f);
        ESP_LOGI(TAG, "Servo C initialized on GPIO %d", m_setting->servo.C_SERVO_PIN);
    }

    // Servo D - Channel 3, Timer 3
    if (m_setting->servo.D_SERVO_PIN >= 0) {
        m_servo_d = std::make_unique<actuator::LEDCActuator>(
            m_setting->servo.D_SERVO_PIN,
            LEDC_CHANNEL_3,
            LEDC_TIMER_0,
            freq_hz,
            0.0f);
        ESP_LOGI(TAG, "Servo D initialized on GPIO %d", m_setting->servo.D_SERVO_PIN);
    }

    // Servo E - Channel 4, Timer 4
    if (m_setting->servo.E_SERVO_PIN >= 0) {
        m_servo_e = std::make_unique<actuator::LEDCActuator>(
            m_setting->servo.E_SERVO_PIN,
            LEDC_CHANNEL_4,
            LEDC_TIMER_0,
            freq_hz,
            0.0f);
        ESP_LOGI(TAG, "Servo E initialized on GPIO %d", m_setting->servo.E_SERVO_PIN);
    }

    // Servo F - Channel 5, Timer 5
    if (m_setting->servo.F_SERVO_PIN >= 0) {
        m_servo_f = std::make_unique<actuator::LEDCActuator>(
            m_setting->servo.F_SERVO_PIN,
            LEDC_CHANNEL_5,
            LEDC_TIMER_0,
            freq_hz,
            0.0f);
        ESP_LOGI(TAG, "Servo F initialized on GPIO %d", m_setting->servo.F_SERVO_PIN);
    }

    return true;
}

void O6Executor::compute() {
    std::lock_guard<std::mutex> lock(m_compute_mutex);

    // Get interpolated values from tcode
    float* interpolated = tcode.interpolate();
    float L0 = interpolated[0];  // Z axis
    float L1 = interpolated[1];  // Y axis
    float L2 = interpolated[2];  // X axis
    float R0 = interpolated[3];  // Yaw angle
    float R1 = interpolated[4];  // Roll angle
    float R2 = interpolated[5];  // Pitch angle

    // Process X coordinate (L2)
    float x = map_(L2, 0.0f, 1.0f, m_setting->servo.L2_LEFT, m_setting->servo.L2_RIGHT);
    if (m_setting->servo.L2_REVERSE) {
        x = m_setting->servo.L2_LEFT + m_setting->servo.L2_RIGHT - x;
    }
    x = map_(x, 0.0f, 1.0f, -3.0f, 3.0f);
    x *= m_setting->servo.L2_SCALE;

    // Process Y coordinate (L1)
    float y = map_(L1, 0.0f, 1.0f, m_setting->servo.L1_LEFT, m_setting->servo.L1_RIGHT);
    if (m_setting->servo.L1_REVERSE) {
        y = m_setting->servo.L1_LEFT + m_setting->servo.L1_RIGHT - y;
    }
    y = map_(y, 0.0f, 1.0f, -3.0f, 3.0f);
    y *= m_setting->servo.L1_SCALE;

    // Process Z coordinate (L0)
    float z = map_(L0, 0.0f, 1.0f, m_setting->servo.L0_LEFT, m_setting->servo.L0_RIGHT);
    if (m_setting->servo.L0_REVERSE) {
        z = m_setting->servo.L0_LEFT + m_setting->servo.L0_RIGHT - z;
    }
    z = map_(z, 0.0f, 1.0f, -6.0f, 6.0f);
    z *= m_setting->servo.L0_SCALE;

    // Process Roll angle (R1)
    float roll = map_(R1, 0.0f, 1.0f, m_setting->servo.R1_LEFT, m_setting->servo.R1_RIGHT);
    if (m_setting->servo.R1_REVERSE) {
        roll = m_setting->servo.R1_LEFT + m_setting->servo.R1_RIGHT - roll;
    }
    roll = map_(roll, 0.0f, 1.0f, -25.0f, 25.0f);
    roll *= m_setting->servo.R1_SCALE;

    // Process Pitch angle (R2)
    float pitch = map_(R2, 0.0f, 1.0f, m_setting->servo.R2_LEFT, m_setting->servo.R2_RIGHT);
    if (m_setting->servo.R2_REVERSE) {
        pitch = m_setting->servo.R2_LEFT + m_setting->servo.R2_RIGHT - pitch;
    }
    pitch = map_(pitch, 0.0f, 1.0f, -25.0f, 25.0f);
    pitch *= m_setting->servo.R2_SCALE;

    // Process Yaw angle (R0)
    float yaw = map_(R0, 0.0f, 1.0f, m_setting->servo.R0_LEFT, m_setting->servo.R0_RIGHT);
    if (m_setting->servo.R0_REVERSE) {
        yaw = m_setting->servo.R0_LEFT + m_setting->servo.R0_RIGHT - yaw;
    }
    yaw = map_(yaw, 0.0f, 1.0f, -25.0f, 25.0f);
    yaw *= m_setting->servo.R0_SCALE;

    // Use O6 kinematics solver to calculate 6 servo angles
    // Note: z offset needs adjustment: z + 19.3 - O6_OFFSET
    auto result = geometry::solve_robot_kinematics(
        x, y, z + 19.3,
        roll, pitch, yaw);

    if (result) {
        m_theta_values = *result;

        // Apply sign correction (even indices inverted)
        for (size_t i = 0; i < 6; i++) {
            if (i % 2 == 0) {
                m_theta_values[i] = -m_theta_values[i];
            }
        }

        static int count = 0;
        if (count++ % 50 == 0) {
            ESP_LOGI(TAG, "Kinematics success - theta values: %.1f°, %.1f°, %.1f°, %.1f°, %.1f°, %.1f°",
                     m_theta_values[0] * 180.0 / M_PI,
                     m_theta_values[1] * 180.0 / M_PI,
                     m_theta_values[2] * 180.0 / M_PI,
                     m_theta_values[3] * 180.0 / M_PI,
                     m_theta_values[4] * 180.0 / M_PI,
                     m_theta_values[5] * 180.0 / M_PI);
        }
    } else {
        // No solution, keep current values
        ESP_LOGW(TAG, "Kinematics no solution, keeping current values");
    }
}

void O6Executor::execute() {
    std::lock_guard<std::mutex> lock(m_compute_mutex);

    // Servo zero offsets array
    int servo_zeros[6] = {
        m_setting->servo.A_SERVO_ZERO,
        m_setting->servo.B_SERVO_ZERO,
        m_setting->servo.C_SERVO_ZERO,
        m_setting->servo.D_SERVO_ZERO,
        m_setting->servo.E_SERVO_ZERO,
        m_setting->servo.F_SERVO_ZERO
    };

    // Send PWM signal for each servo
    for (int i = 0; i < 6; i++) {
        // Convert angle from radians to degrees
        double angle_deg = m_theta_values[i] * 180.0 / M_PI;

        // Map angle to servo pulse width range (typically 500-2500us)
        // Assuming servo working range is ±90 degrees
        double pulse_width = map_(angle_deg, -90.0, 90.0, 500.0, 2500.0);

        // Apply zero offset
        pulse_width = servo_zeros[i] + (pulse_width - 1500.0);

        // Constrain pulse width to valid range
        if (pulse_width < 500.0) pulse_width = 500.0;
        if (pulse_width > 2500.0) pulse_width = 2500.0;

        // Convert pulse width to target value (-1.0 to 1.0) for LEDC actuator
        // 1500us center = 0.0, 500us = -1.0, 2500us = 1.0
        float target = (pulse_width - 1500.0) / 1000.0;

        // Set target for corresponding servo
        switch (i) {
            case 0:
                if (m_servo_a) m_servo_a->setTarget(target);
                m_servo_a_target = target;
                break;
            case 1:
                if (m_servo_b) m_servo_b->setTarget(target);
                m_servo_b_target = target;
                break;
            case 2:
                if (m_servo_c) m_servo_c->setTarget(target);
                m_servo_c_target = target;
                break;
            case 3:
                if (m_servo_d) m_servo_d->setTarget(target);
                m_servo_d_target = target;
                break;
            case 4:
                if (m_servo_e) m_servo_e->setTarget(target);
                m_servo_e_target = target;
                break;
            case 5:
                if (m_servo_f) m_servo_f->setTarget(target);
                m_servo_f_target = target;
                break;
        }
    }
}
