
#include "executor/sr6can_executor.hpp"
#include "def.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "utils.hpp"
#include <cmath>

#define BIAS 4.0f / M_PI
#define STABLE_SAMPLE_COUNT 100

// 平方函数
inline float sq(float x) { return x * x; }

// micros函数：返回微秒数
inline unsigned long micros() { return (unsigned long)(esp_timer_get_time()); }

// 静态成员变量定义
const char *SR6CANExecutor::TAG = "SR6CANExecutor";
bool SR6CANExecutor::ctw_initialized_ = false;
std::mutex SR6CANExecutor::init_mutex_;

SR6CANExecutor::SR6CANExecutor(const SettingWrapper &setting)
    : Executor(setting) {
  ESP_LOGI(TAG, "SR6CANExecutor构造()，SR6CANServoNum: %d\n", SR6CANServoNum);
  for (int i = 0; i < SR6CANServoNum; i++) {
    motor_position[i] = 0.0;
    // 初始化PID控制器，只使用I控制
    // if (i == 2 || i == 3) {
    //     pid_init(&motor_pid[i], 0.0f, m_setting->mit.Ki_c, 0.0f, 2.0f);
    // } else {
    //     pid_init(&motor_pid[i], 0.0f, 20.0f, 0.0f, 2.0f);
    // }
  }

  esp_log_level_set(TAG, ESP_LOG_INFO);
  ESP_LOGI(TAG, "构造()");

  // 先启动CAN
  // 初始化CTW协议（会自动启动接收任务）
  if (!ctw_initialized_) {
    esp_err_t ret = CTW::init(2, 3, 500000); // TX=GPIO2, RX=GPIO3, 500kbps
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize CTW: %s", esp_err_to_name(ret));
    } else {
      ctw_initialized_ = true;
    }
  }
  // 初始化电机参数
  initMotorParams();
  // 初始化电机
  initMotors();
  // 等待电机使能
  vTaskDelay(pdMS_TO_TICKS(1000));
  // 记录初始化开始时间（用于等待1秒获取正确位置）
  init_start_time_ = ((uint32_t)(esp_timer_get_time() / 1000));
}

SR6CANExecutor::~SR6CANExecutor() {
  ESP_LOGI(TAG, "SR6CANExecutor析构()，SR6CANServoNum: %d\n", SR6CANServoNum);

  // 停止所有电机
  // for (int i = 1; i <= 6; i++) {w
  //     MIT::stop_motor(i);
  // }
}

void SR6CANExecutor::compute() {
  // return;
  std::lock_guard<std::mutex> lock(compute_mutex_);
  auto now1 = micros();

  // 从tcode获取插值后的值
  float *interpolated = tcode.interpolate();
  float L0 = interpolated[0];
  float L1 = interpolated[1];
  float L2 = interpolated[2];
  float R0 = interpolated[3];
  float R1 = interpolated[4];
  float R2 = interpolated[5];

  float roll, pitch, x, y, z;

  // 处理 thrust (L0)
  y = map_(L0, 0.0f, 1.0f, m_setting->servo.L0_LEFT, m_setting->servo.L0_RIGHT);
  if (m_setting->servo.L0_REVERSE) {
    y = m_setting->servo.L0_LEFT + m_setting->servo.L0_RIGHT - y;
  }
  y = map_(y, 0.0f, 1.0f, -6000.0f, 6000.0f);
  y *= m_setting->servo.L0_SCALE;

  // 处理 roll (R1)
  roll =
      map_(R1, 0.0f, 1.0f, m_setting->servo.R1_LEFT, m_setting->servo.R1_RIGHT);
  if (m_setting->servo.R1_REVERSE) {
    roll = m_setting->servo.R1_LEFT + m_setting->servo.R1_RIGHT - roll;
  }
  roll = map_(roll, 0.0f, 1.0f, -2500.0f, 2500.0f);
  roll *= m_setting->servo.R1_SCALE;

  // 处理 pitch (R2)
  pitch =
      map_(R2, 0.0f, 1.0f, m_setting->servo.R2_LEFT, m_setting->servo.R2_RIGHT);
  if (m_setting->servo.R2_REVERSE) {
    pitch = m_setting->servo.R2_LEFT + m_setting->servo.R2_RIGHT - pitch;
  }
  pitch = map_(pitch, 0.0f, 1.0f, -2500.0f, 2500.0f);
  pitch *= m_setting->servo.R2_SCALE;

  // 处理 fwd (L1)
  x = map_(L1, 0.0f, 1.0f, m_setting->servo.L1_LEFT, m_setting->servo.L1_RIGHT);
  if (m_setting->servo.L1_REVERSE) {
    x = m_setting->servo.L1_LEFT + m_setting->servo.L1_RIGHT - x;
  }
  x = map_(x, 0.0f, 1.0f, -3000.0f, 3000.0f);
  x *= m_setting->servo.L1_SCALE;

  // 处理 side (L2)
  z = map_(L2, 0.0f, 1.0f, m_setting->servo.L2_LEFT, m_setting->servo.L2_RIGHT);
  if (m_setting->servo.L2_REVERSE) {
    z = m_setting->servo.L2_LEFT + m_setting->servo.L2_RIGHT - z;
  }
  z = map_(z, 0.0f, 1.0f, -3000.0f, 3000.0f);
  z *= m_setting->servo.L2_SCALE;

  auto roll_sin = sin(roll / 100.0f / 180.0f * (float)PI);
  auto roll_cos = cos(roll / 100.0f / 180.0f * (float)PI);
  auto d = (18000) / 2.0f;
  auto y__ = y;
  auto x__ = x;
  auto z__ = z;
  auto roll__ = roll;
  auto pitch__ = pitch;
  float rub;
  axis7_to_axis6(x__, y__ + EXTENSION_LENGTH, z__, roll__ / 100.0f,
                 pitch__ / 100.0f, 0, x, y, z, roll, pitch, rub);
  roll *= 100.0f;
  pitch *= 100.0f;
  static int t = 0;
  if (t++ % getExecuteFrequency() == 0) {
    printf("roll: %f, pitch: %f, x: %f, y: %f, z: %f\n", roll, pitch, x, y, z);
  }
  float lowerLeftValue, upperLeftValue, pitchLeftValue, pitchRightValue,
      upperRightValue, lowerRightValue;
  bool inverseStroke = false;
  // 95/2+105=152.5 270*270-152.5*152.5=72900-23256.25=49643.75
  lowerLeftValue = SetMainServo(22280 - x, 4750 + y + d * roll_sin);
  lowerRightValue = SetMainServo(22280 - x, 4750 + y - d * roll_sin);
  upperLeftValue = SetMainServo(22280 - x, 4750 - y - d * roll_sin);
  upperRightValue = SetMainServo(22280 - x, 4750 - y + d * roll_sin);
  pitchLeftValue = SetPitchServo(22280 - x, (4750 + 9500) - y - d * roll_sin,
                                 z - (8300 * roll_sin), -pitch);
  pitchRightValue = SetPitchServo(22280 - x, (4750 + 9500) - y + d * roll_sin,
                                  -z + (8300 * roll_sin), -pitch);
  // 直接使用弧度，用于MIT控制
  int i = 0;
  if (i++ < SR6CANServoNum)
    motor_position[0] = lowerLeftValue;
  if (i++ < SR6CANServoNum)
    motor_position[1] = upperLeftValue;
  if (i++ < SR6CANServoNum)
    motor_position[2] = pitchLeftValue;
  if (i++ < SR6CANServoNum)
    motor_position[3] = pitchRightValue;
  if (i++ < SR6CANServoNum)
    motor_position[4] = upperRightValue;
  if (i++ < SR6CANServoNum)
    motor_position[5] = lowerRightValue;

  if (t % getExecuteFrequency() == 0) {
    printf("lowerLeftValue: %f, upperLeftValue: %f, pitchLeftValue: %f, "
           "pitchRightValue: %f, upperRightValue: %f, lowerRightValue: %f\n",
           lowerLeftValue / PI * 180.0f, upperLeftValue / PI * 180.0f,
           pitchLeftValue / PI * 180.0f, pitchRightValue / PI * 180.0f,
           upperRightValue / PI * 180.0f, lowerRightValue / PI * 180.0f);
  }

  auto now2 = micros();
  auto diff = now2 - now1;
  // if (diff > COMPUTE_TIMEOUT * 1.5) {
  //     printf("[SR6CANExecutor]计算耗时:%d us\n", (int)diff);
  // }
  // static int t = 0;
  // if (t++ % getExecuteFrequency() == 0) {
  //     float temp[SR6CANServoNum];
  //     {
  //         std::lock_guard<std::mutex> feedback_lock(feedback_mutex_);
  //         memcpy(temp, motor_position_feedback,
  //                sizeof(float) * SR6CANServoNum);
  //     }
  //     for (size_t j = 0; j < SR6CANServoNum; j++) {
  //         printf("motor_position[%d]: %f\n", j, temp[j] * 180.0f / PI);
  //     }
  // }
}

void SR6CANExecutor::updateFeedbackFromCTW() {
  for (int i = 0; i < SR6CANServoNum; i++) {
    CTW::MotorFeedback feedback;
    if (CTW::get_cached_feedback(i + 1, feedback) == ESP_OK) {
      float pos = feedback.position;
      uint32_t last_update = feedback.last_update;

      // 检查数据是否更新（通过 last_update 时间戳判断）
      bool data_updated = (last_update != last_feedback_update_times[i]);

      if (data_updated) {
        last_feedback_update_times[i] = last_update;

        std::lock_guard<std::mutex> lock(feedback_mutex_);

        // 处理稳定计数和偏移（只在数据更新时处理）
        if (stable[i] < STABLE_SAMPLE_COUNT) {
          if (pos < -4) {
            motor_feedback_offset[i] = 8;
          } else if (pos > 4) {
            motor_feedback_offset[i] = -8;
          }
          stable[i] += 1;
          if (stable[i] == STABLE_SAMPLE_COUNT - 1) {
            printf("[SR6CANExecutor] 电机%d初始位置偏移: %f, "
                   "位置: %f\n",
                   i + 1, motor_feedback_offset[i], pos);
          }
        }

        // 更新位置反馈（加上偏移）
        motor_position_feedback[i] = pos + motor_feedback_offset[i];
      }
    }
  }
}

bool SR6CANExecutor::isAllMotorsStable() const {
  for (int i = 0; i < SR6CANServoNum; ++i) {
    if (stable[i] < STABLE_SAMPLE_COUNT) {
      return false;
    }
  }
  return true;
}

void SR6CANExecutor::printUnstableMotors() const {
  for (int i = 0; i < SR6CANServoNum; ++i) {
    if (stable[i] < STABLE_SAMPLE_COUNT) {
      printf("[SR6CANExecutor] 电机%d 尚未稳定: %d/%d\n", i + 1, stable[i],
             STABLE_SAMPLE_COUNT);
    }
  }
}

void SR6CANExecutor::execute() {
  // return;
  std::lock_guard<std::mutex> lock(compute_mutex_);

  // 从CTW更新反馈数据
  updateFeedbackFromCTW();

  // 状态机处理初始化流程
  switch (init_state_) {
  case InitState::WAITING_STABILITY: {
    // 等待1秒以获取正确的编码器位置
    unsigned long elapsed =
        ((uint32_t)(esp_timer_get_time() / 1000)) - init_start_time_;
    if (elapsed < 1000) {
      if (elapsed % 200 < 20) { // 每200ms打印一次
        ESP_LOGI(TAG, "等待电机稳定... %lu ms", elapsed);
      }
      return;
    }

    // 检查所有电机是否都已稳定
    if (!isAllMotorsStable()) {
      static int t = 0;
      if (t++ % 10 == 0) {
        printUnstableMotors();
      }
      return;
    }

    // 所有电机稳定，进入回原点阶段
    ESP_LOGI(TAG, "所有电机已稳定，开始回原点");
    init_state_ = InitState::HOMING;
    performHoming();
    return;
  }

  case InitState::HOMING: {
    // 检查回原点是否完成
    if (isHomingComplete()) {
      ESP_LOGI(TAG, "回原点完成，切换到正常运行模式");
      init_state_ = InitState::RUNNING;
      homing_completed_ = true;

      // 回零和正常执行使用相同模式，无需再次切换模式
      // 只需确保滤波器带宽设置正确（已在initMotors中设置）
      ESP_LOGI(TAG, "所有电机已处于位置模式+位置滤波，准备正常运行");
    } /* else{
         for(int i = 0; i < SR6CANServoNum; i++){
             //打印距离原点的距离
             float distance = fabs(motor_position[i] -
     homing_target_positions[i]); printf("电机%d距离原点的距离: %f\n", i + 1,
     distance);
         }
     } */
    return;
  }

  case InitState::RUNNING: {
    // 正常运行模式
    std::lock_guard<std::mutex> feedback_lock(feedback_mutex_);
    for (int i = 0; i < SR6CANServoNum; i++) {
      // 应用方向反转
      float target_pos = motor_position[i];
      if (i == 1 || i == 4) {
        target_pos *= -1.0f;
      }
      if (i > 2) {
        target_pos *= -1.0f;
      }

      // 计算最终目标位置（考虑offset和反馈偏移）
      float final_pos = (target_pos + motor_offset[i] / 180.0f * PI) * BIAS -
                        motor_feedback_offset[i];

      // 使用位置滤波模式，直接设置目标位置
      CTW::set_position(i + 1, final_pos);
    }

    static int t = 0;
    if (t++ % getExecuteFrequency() == 0) {
      for (size_t i = 0; i < SR6CANServoNum; i++) {
        printf("feedback:%f, feedback_raw: %f, diff:%f\n",
               motor_position_feedback[i],
               motor_position_feedback[i] - motor_feedback_offset[i],
               motor_position[i] - motor_position_feedback[i]);
      }
    }
    return;
  }
  }
}

void SR6CANExecutor::initMotorParams() {
  // 保存PID参数到本地数组（用于后续使用）
  motor_offset[0] = m_setting->mit.offset_a;
  motor_offset[1] = m_setting->mit.offset_b;
  motor_offset[2] = m_setting->mit.offset_c;
  motor_offset[3] = m_setting->mit.offset_d;
  motor_offset[4] = m_setting->mit.offset_e;
  motor_offset[5] = m_setting->mit.offset_f;

  // 位置环PID参数（所有电机使用相同的参数）
  float pos_kp = m_setting->mit.Kp_a;
  float pos_ki = m_setting->mit.Ki_a;
  float pos_kd = m_setting->mit.Kd_a;

  // 速度环PID参数（所有电机使用相同的参数）
  float vel_kp = m_setting->mit.Kp_b;
  float vel_ki = m_setting->mit.Ki_b;
  float vel_kd = m_setting->mit.Kd_b;

  // 通过write_endpoint_float将PID参数设置到电机
  for (int i = 0; i < SR6CANServoNum; i++) {
    uint8_t node_id = i + 1; // 电机节点ID从1开始

    // 设置位置环Kp参数
    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_POS_GAIN, pos_kp));
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置位置环Ki参数
    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_POS_INTEGRATOR_GAIN, pos_ki));
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置位置环Kd参数
    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_POS_DIFF_GAIN, pos_kd));
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置速度环Kp参数
    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_VEL_GAIN, vel_kp));
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置速度环Ki参数
    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_VEL_INTEGRATOR_GAIN, vel_ki));
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置速度环Kd参数
    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_VEL_DIFF_GAIN, vel_kd));
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置惯量为0
    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_INERTIA, 0.0f));
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_ERROR_CHECK(CTW::write_endpoint_float(
        node_id, CTW::EID_AXIS0_CONTROLLER_CONFIG_POS_INTEGRATOR_LIMIT, 1.0f));
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG,
             "电机%d PID参数已设置: 位置环 Kp=%.3f, Ki=%.3f, Kd=%.3f; 速度环 "
             "Kp=%.3f, Ki=%.3f, Kd=%.3f; 惯量=0.0",
             node_id, pos_kp, pos_ki, pos_kd, vel_kp, vel_ki, vel_kd);
  }

  printf("位置环 PID: Kp=%f, Ki=%f, Kd=%f\n", pos_kp, pos_ki, pos_kd);
  printf("速度环 PID: Kp=%f, Ki=%f, Kd=%f\n", vel_kp, vel_ki, vel_kd);
  printf("OFFSET: %f, %f, %f, %f, %f, %f\n", motor_offset[0], motor_offset[1],
         motor_offset[2], motor_offset[3], motor_offset[4], motor_offset[5]);
}

void SR6CANExecutor::initMotors() {
  // 启动所有电机
  for (int i = 1; i <= SR6CANServoNum; i++) {
    // 设置控制器模式为位置模式 + 位置滤波（与正常执行使用相同模式）
    ESP_ERROR_CHECK(
        CTW::set_controller_mode(i, CTRL_MODE_POSITION, INPUT_MODE_POS_FILTER));
    vTaskDelay(pdMS_TO_TICKS(10));
    // 设置滤波器带宽
    CTW::set_filter_bandwidth(i, getExecuteFrequency() * 0.5f);
    vTaskDelay(pdMS_TO_TICKS(10));

    for (size_t j = 0; j < 10; j++) {
      CTW::write_endpoint_float(1, 280 + i, 0.277 * (i + 1));
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(CTW::start_motor(i));
    vTaskDelay(pdMS_TO_TICKS(100));

    // for (size_t j = 0; j < 20; j++) {

    // }
    // ESP_LOGE("TEST", "TEST");
    ESP_LOGI(TAG, "电机%d已设置为位置模式+位置滤波", i);
  }
}

float SR6CANExecutor::SetMainServo(float x, float y) {
  x /= 100;
  y /= 100;
  float gamma = atan2(x, y);
  float csq = sq(x) + sq(y);
  float c = sqrt(csq);
  float beta = acos((csq + 105 * 105 - 270 * 270) / (2 * 105 * c));
  float result = (gamma + beta - M_PI);
  return result;
}

float SR6CANExecutor::SetPitchServo(float x, float y, float z, float pitch) {
  pitch *= 0.0001745;            // 角度转换为弧度
  x += 8300 * sin(0.05 + pitch); // 0.2618rad=15度
  y -= 8300 * cos(0.05 + pitch);
  x /= 100;
  y /= 100;
  z /= 100;
  float bsq = 280 * 280 - sq(63 + z);
  float gamma = atan2(x, y);
  float csq = sq(x) + sq(y);
  float c = sqrt(csq);
  float acos_arg = (csq + 105 * 105 - bsq) / (2 * 105 * c);
  // 约束acos参数到[-1, 1]范围，避免nan
  if (acos_arg > 1.0f) {
    acos_arg = 1.0f;
  } else if (acos_arg < -1.0f) {
    acos_arg = -1.0f;
  }
  float beta = acos(acos_arg);
  float result = (gamma + beta - M_PI);
  return result;
}

void SR6CANExecutor::performHoming() {
  return;
  ESP_LOGI(TAG, "开始回原点操作");
  homing_start_time_ = ((uint32_t)(esp_timer_get_time() / 1000));

  // 计算回原点的目标位置（将每个电机移动到0位置）
  // 使用梯形轨迹模式，需要设置目标位置为0（考虑offset）
  for (int i = 0; i < SR6CANServoNum; i++) {
    // 回原点目标：位置为0
    float target_pos = 0.0f;

    // 应用方向反转
    if (i == 1 || i == 4) {
      target_pos *= -1.0f;
    }
    if (i > 2) {
      target_pos *= -1.0f;
    }

    // 计算最终目标位置（只考虑offset，不考虑反馈偏移）
    // float final_pos = (target_pos + motor_offset[i] / 180.0f * PI) * BIAS;
    float final_pos = 0;
    homing_target_positions[i] = final_pos;

    // 发送回原点指令
    CTW::set_position(i + 1, final_pos);

    ESP_LOGI(TAG, "电机%d回原点: 目标位置=%f", i + 1, final_pos);
  }
}

bool SR6CANExecutor::isHomingComplete() {
  // 检查是否超时（最多等待5秒）
  //   if (((uint32_t)(esp_timer_get_time() / 1000)) - homing_start_time_ >
  //   5000) {
  //     ESP_LOGW(TAG, "回原点超时，强制进入运行模式");
  //     return true;
  //   }
  for (int i = 0; i < SR6CANServoNum; i++) {
    CTW::write_endpoint_float(i + 1, CTW::EID_AXIS0_CONTROLLER_INPUT_POS, 0);
  }
  // 检查所有电机是否到达目标位置
  bool all_arrived = true;
  const float POSITION_TOLERANCE = 0.01f; // 位置容差（弧度）

  for (int i = 0; i < SR6CANServoNum; i++) {
    // 计算当前位置与目标位置的距离
    float current_pos = motor_position_feedback[i];
    float target_pos = homing_target_positions[i];
    float diff = fabsf(current_pos - target_pos);

    if (diff > POSITION_TOLERANCE) {
      all_arrived = false;
      break;
    }
  }

  if (all_arrived) {
    ESP_LOGI(TAG, "所有电机已到达原点位置");
    return true;
  }

  // 定期打印回原点进度
  static int print_count = 0;
  if (print_count++ % 20 == 0) {
    for (int i = 0; i < SR6CANServoNum; i++) {
      float diff =
          fabsf(motor_position_feedback[i] - homing_target_positions[i]);
      printf("电机%d 距离原点: %.4f rad\n", i + 1, diff);
      printf("feedback:%f, feedback_raw: %f, diff:%f\n",
             motor_position_feedback[i],
             motor_position_feedback[i] - motor_feedback_offset[i],
             motor_position[i] - motor_position_feedback[i]);
    }
  }

  return false;
}

int SR6CANExecutor::getExecuteFrequency() {
  return m_setting->servo.A_SERVO_PWM_FREQ;
}