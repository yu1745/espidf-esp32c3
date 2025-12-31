#include "executor/sr6_executor.hpp"
#include "actuator/rmt_actuator.hpp"
#include "actuator/spi_actuator.hpp"
#include "def.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "setting.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

const char *SR6Executor::TAG = "SR6Executor";

SR6Executor::SR6Executor(const SettingWrapper &setting)
    : Executor(setting), m_servo_a_duty(0), m_servo_b_duty(0),
      m_servo_c_duty(0), m_servo_d_duty(0), m_servo_e_duty(0),
      m_servo_f_duty(0), m_servo_g_duty(0) {
  try {
    ESP_LOGI(TAG, "SR6Executor() constructing...");

    // 初始化LEDC定时器
    if (!initLEDC()) {
      ESP_LOGE(TAG, "Failed to initialize LEDC");
      throw std::runtime_error("Failed to initialize LEDC");
    }

    // 创建舵机A执行器
    if (m_setting->servo.A_SERVO_PIN != -1) {
      float offset_a =
          ((float)m_setting->servo.A_SERVO_ZERO - 1500.0f) / 1000.0f;
      m_servo_a = std::make_unique<actuator::LEDCActuator>(
          m_setting->servo.A_SERVO_PIN, LEDC_CHANNEL_0, LEDC_TIMER_0,
          m_setting->servo.A_SERVO_PWM_FREQ, offset_a);
      ESP_LOGI(TAG, "Servo A initialized on GPIO %d, offset: %.3f",
               m_setting->servo.A_SERVO_PIN, offset_a);
    }

    // 创建舵机B执行器
    if (m_setting->servo.B_SERVO_PIN != -1) {
      float offset_b =
          ((float)m_setting->servo.B_SERVO_ZERO - 1500.0f) / 1000.0f;
      m_servo_b = std::make_unique<actuator::LEDCActuator>(
          m_setting->servo.B_SERVO_PIN, LEDC_CHANNEL_1, LEDC_TIMER_0,
          m_setting->servo.A_SERVO_PWM_FREQ, offset_b);
      ESP_LOGI(TAG, "Servo B initialized on GPIO %d, offset: %.3f",
               m_setting->servo.B_SERVO_PIN, offset_b);
    }

    // 创建舵机C执行器
    if (m_setting->servo.C_SERVO_PIN != -1) {
      float offset_c =
          ((float)m_setting->servo.C_SERVO_ZERO - 1500.0f) / 1000.0f;
      m_servo_c = std::make_unique<actuator::LEDCActuator>(
          m_setting->servo.C_SERVO_PIN, LEDC_CHANNEL_2, LEDC_TIMER_0,
          m_setting->servo.A_SERVO_PWM_FREQ, offset_c);
      ESP_LOGI(TAG, "Servo C initialized on GPIO %d, offset: %.3f",
               m_setting->servo.C_SERVO_PIN, offset_c);
    }

    // 创建舵机D执行器
    if (m_setting->servo.D_SERVO_PIN != -1) {
      float offset_d =
          ((float)m_setting->servo.D_SERVO_ZERO - 1500.0f) / 1000.0f;
      m_servo_d = std::make_unique<actuator::LEDCActuator>(
          m_setting->servo.D_SERVO_PIN, LEDC_CHANNEL_3, LEDC_TIMER_0,
          m_setting->servo.A_SERVO_PWM_FREQ, offset_d);
      ESP_LOGI(TAG, "Servo D initialized on GPIO %d, offset: %.3f",
               m_setting->servo.D_SERVO_PIN, offset_d);
    }

    // 创建舵机E执行器
    if (m_setting->servo.E_SERVO_PIN != -1) {
      float offset_e =
          ((float)m_setting->servo.E_SERVO_ZERO - 1500.0f) / 1000.0f;
      m_servo_e = std::make_unique<actuator::LEDCActuator>(
          m_setting->servo.E_SERVO_PIN, LEDC_CHANNEL_4, LEDC_TIMER_0,
          m_setting->servo.A_SERVO_PWM_FREQ, offset_e);
      ESP_LOGI(TAG, "Servo E initialized on GPIO %d, offset: %.3f",
               m_setting->servo.E_SERVO_PIN, offset_e);
    }

    // 创建舵机F执行器
    if (m_setting->servo.F_SERVO_PIN != -1) {
      float offset_f =
          ((float)m_setting->servo.F_SERVO_ZERO - 1500.0f) / 1000.0f;
      m_servo_f = std::make_unique<actuator::LEDCActuator>(
          m_setting->servo.F_SERVO_PIN, LEDC_CHANNEL_5, LEDC_TIMER_0,
          m_setting->servo.A_SERVO_PWM_FREQ, offset_f);
      // m_servo_f = std::make_unique<actuator::RMTActuator>(
      //     m_setting->servo.F_SERVO_PIN, offset_f);
      ESP_LOGI(TAG, "Servo F initialized on GPIO %d, offset: %.3f",
               m_setting->servo.F_SERVO_PIN, offset_f);
    }

    // 创建舵机G执行器（使用SPI）
    if (m_setting->servo.G_SERVO_PIN != -1) {
      float offset_g =
          ((float)m_setting->servo.G_SERVO_ZERO - 1500.0f) / 1000.0f;
      m_servo_g = std::make_unique<actuator::RMTActuator>(
          m_setting->servo.G_SERVO_PIN, offset_g);
      ESP_LOGI(TAG, "Servo G (SPI) initialized on GPIO %d, offset: %.3f",
               m_setting->servo.G_SERVO_PIN, offset_g);
    }

    // 回中 - 初始计算一次
    compute();

    ESP_LOGI(TAG, "SR6Executor initialized successfully");
  } catch (...) {
    ESP_LOGE(TAG, "Failed to construct SR6Executor");
    // 清理已分配的资源
    m_servo_a.reset();
    m_servo_b.reset();
    m_servo_c.reset();
    m_servo_d.reset();
    m_servo_e.reset();
    m_servo_f.reset();
    m_servo_g.reset();
    throw;
  }
}

SR6Executor::~SR6Executor() {
  ESP_LOGI(TAG, "~SR6Executor() deconstructing...");
  m_servo_a.reset();
  m_servo_b.reset();
  m_servo_c.reset();
  m_servo_d.reset();
  m_servo_e.reset();
  m_servo_f.reset();
  m_servo_g.reset();
  ESP_LOGI(TAG, "SR6Executor destroyed");
}

bool SR6Executor::initLEDC() {
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

void SR6Executor::compute() {
  std::lock_guard<std::mutex> lock(m_compute_mutex);

  // 从tcode中获取插值后的轴值
  // 插值结果顺序为：L0, L1, L2, R0, R1, R2
  float* interpolated = tcode.interpolate();
  float y_input = interpolated[0];      // L0
  float x_input = interpolated[1];      // L1
  float z_input = interpolated[2];      // L2
  float twist_input = interpolated[3];   // R0
  float roll_input = interpolated[4];    // R1
  float pitch_input = interpolated[5];   // R2

  // 处理twist输入
  float twist = map_(twist_input, 0.0f, 1.0f, m_setting->servo.R0_LEFT,
                     m_setting->servo.R0_RIGHT);
  if (m_setting->servo.R0_REVERSE) {
    twist = m_setting->servo.R0_LEFT + m_setting->servo.R0_RIGHT - twist;
  }
  twist = map_(twist, 0.0f, 1.0f, -PI/2, PI/2);
  twist *= m_setting->servo.R0_SCALE;

  // 处理x输入
  float x = map_(x_input, 0.0f, 1.0f, m_setting->servo.L1_LEFT,
                 m_setting->servo.L1_RIGHT);
  if (m_setting->servo.L1_REVERSE) {
    x = m_setting->servo.L1_LEFT + m_setting->servo.L1_RIGHT - x;
  }
  x = map_(x, 0.0f, 1.0f, -3000.0f, 3000.0f);
  x *= m_setting->servo.L1_SCALE;

  // 处理roll输入
  float roll = map_(roll_input, 0.0f, 1.0f, m_setting->servo.R1_LEFT,
                    m_setting->servo.R1_RIGHT);
  if (m_setting->servo.R1_REVERSE) {
    roll = m_setting->servo.R1_LEFT + m_setting->servo.R1_RIGHT - roll;
  }
  roll = map_(roll, 0.0f, 1.0f, -2500.0f, 2500.0f);
  roll *= m_setting->servo.R1_SCALE;

  // 处理pitch输入
  float pitch = map_(pitch_input, 0.0f, 1.0f, m_setting->servo.R2_LEFT,
                     m_setting->servo.R2_RIGHT);
  if (m_setting->servo.R2_REVERSE) {
    pitch = m_setting->servo.R2_LEFT + m_setting->servo.R2_RIGHT - pitch;
  }
  pitch = map_(pitch, 0.0f, 1.0f, -2500.0f, 2500.0f);
  pitch *= m_setting->servo.R2_SCALE * -1;

  // 处理y输入
  float y = map_(y_input, 0.0f, 1.0f, m_setting->servo.L0_LEFT,
                 m_setting->servo.L0_RIGHT);
  if (m_setting->servo.L0_REVERSE) {
    y = m_setting->servo.L0_LEFT + m_setting->servo.L0_RIGHT - y;
  }
  y = map_(y, 0.0f, 1.0f, -6000.0f, 6000.0f);
  y *= m_setting->servo.L0_SCALE * -1;

  // 处理z输入
  float z = map_(z_input, 0.0f, 1.0f, m_setting->servo.L2_LEFT,
                 m_setting->servo.L2_RIGHT);
  if (m_setting->servo.L2_REVERSE) {
    z = m_setting->servo.L2_LEFT + m_setting->servo.L2_RIGHT - z;
  }
  z = map_(z, 0.0f, 1.0f, -3000.0f, 3000.0f);
  z *= m_setting->servo.L2_SCALE;

  // 计算roll的sin值
  float roll_sin = sinf(roll / 100.0f / 180.0f * M_PI);

  // 计算距离d
  float d = 13700.0f / 2.0f;

  //   // 准备7轴到6轴的转换参数
  //   float x7 = x;
  //   float y7 = y + EXTENSION_LENGTH;
  //   float z7 = z;
  //   float roll7 = roll / 100.0f;
  //   float pitch7 = pitch / 100.0f;
  //   float twist7 = 0.0f;

  //   // 执行7轴到6轴的转换
  //   float x6, y6, z6, roll6, pitch6, twist6;
  //   axis7_to_axis6(x7, y7, z7, roll7, pitch7, twist7, x6, y6, z6, roll6,
  //   pitch6,
  //                  twist6);

  // 转换回1/100单位
  //   roll = roll6 * 100.0f;
  //   pitch = pitch6 * 100.0f;

  // 计算主舵机值
  float lowerLeftValue = setMainServo(16248.0f - x, 1500.0f + y + d * roll_sin);
  float lowerRightValue =
      setMainServo(16248.0f - x, 1500.0f + y - d * roll_sin);
  float upperLeftValue = setMainServo(16248.0f - x, 1500.0f - y - d * roll_sin);
  float upperRightValue =
      setMainServo(16248.0f - x, 1500.0f - y + d * roll_sin);

  // 计算俯仰舵机值
  float pitchLeftValue = setPitchServo(16248.0f - x, 4500.0f - y - d * roll_sin,
                                       z - (5500.0f * roll_sin), -pitch);
  float pitchRightValue =
      setPitchServo(16248.0f - x, 4500.0f - y + d * roll_sin,
                    -(z - (5500.0f * roll_sin)), -pitch);

  // 将舵机值转换为占空比（-1到1范围）
  m_servo_a_duty = map_(lowerLeftValue, -PI / 2, PI / 2, -1.0f, 1.0f);
  m_servo_b_duty = -map_(lowerRightValue, -PI / 2, PI / 2, -1.0f, 1.0f);
  m_servo_c_duty = -map_(upperLeftValue, -PI / 2, PI / 2, -1.0f, 1.0f);
  m_servo_d_duty = map_(upperRightValue, -PI / 2, PI / 2, -1.0f, 1.0f);
  m_servo_e_duty = map_(pitchLeftValue, -PI / 2, PI / 2, -1.0f, 1.0f);
  m_servo_f_duty = -map_(pitchRightValue, -PI / 2, PI / 2, -1.0f, 1.0f);
  m_servo_g_duty = map_(twist, -PI / 2, PI / 2, -1.0f, 1.0f);
}

void SR6Executor::execute() {
  std::lock_guard<std::mutex> lock(m_compute_mutex);
//   static int t = 0;
//   auto flag = t++ % m_setting->servo.A_SERVO_PWM_FREQ == 0;
//   if (flag) {
//     ESP_LOGI(TAG,
//              "a: %.3f, b: %.3f, c: %.3f, d: %.3f, e: %.3f, f: %.3f, g: %.3f",
//              m_servo_a_duty, m_servo_b_duty, m_servo_c_duty, m_servo_d_duty,
//              m_servo_e_duty, m_servo_f_duty, m_servo_g_duty);
//   }

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
  if (m_servo_e) {
    m_servo_e->setTarget(m_servo_e_duty);
  }
  if (m_servo_f) {
    m_servo_f->setTarget(m_servo_f_duty);
  }
  if (m_servo_g) {
    m_servo_g->setTarget(m_servo_g_duty);
  }
}

float SR6Executor::setMainServo(float x, float y) {
  x /= 100.0f;
  y /= 100.0f; // Convert to mm
  float gamma =
      atan2f(x, y); // Angle of line from servo pivot to receiver pivot
  float csq =
      x * x +
      y * y; // Square of distance between servo pivot and receiver pivot
  float c = sqrtf(csq); // Distance between servo pivot and receiver pivot
  float beta = acosf(std::max(
      -1.0f,
      std::min(1.0f, (csq - 28125.0f) /
                         (100.0f * c)))); // Angle between c-line and servo arm
  return gamma + beta - M_PI;             // Servo signal output, from neutral
}

float SR6Executor::setPitchServo(float x, float y, float z, float pitch) {
  pitch *= 0.0001745f; // Convert to radians
  x += 5500.0f * sinf(0.2618f + pitch);
  y -= 5500.0f * cosf(0.2618f + pitch);
  x /= 100.0f;
  y /= 100.0f;
  z /= 100.0f;                                      // Convert to mm
  float bsq = 36250.0f - (75.0f + z) * (75.0f + z); // Equivalent arm length
  float gamma =
      atan2f(x, y); // Angle of line from servo pivot to receiver pivot
  float csq =
      x * x +
      y * y; // Square of distance between servo pivot and receiver pivot
  float c = sqrtf(csq); // Distance between servo pivot and receiver pivot
  float beta = acosf(std::max(
      -1.0f, std::min(1.0f, (csq + 75.0f * 75.0f - bsq) /
                                (2.0f * 75.0f *
                                 c)))); // Angle between c-line and servo arm
  return gamma + beta - M_PI;           // Servo signal output, from neutral
}
