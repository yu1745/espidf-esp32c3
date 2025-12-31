#include "executor/sr6can_executor.hpp"
#include "esp_log_config.h"
#include "utils.hpp"
#include "esp_log.h"
#include "freertos/task.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include "twai/mit.hpp"
#include "twai/twai.hpp"

// 静态成员变量定义
const char* SR6CANExecutor::TAG = "SR6CANExecutor";
bool SR6CANExecutor::mit_initialized_ = false;
std::mutex SR6CANExecutor::init_mutex_;

SR6CANExecutor::SR6CANExecutor(const SettingWrapper& setting)
    : Executor(setting),
      can_receive_task_handle_(nullptr),
      init_done(false) {
    try {
        ESP_LOGI(TAG, "SR6CANExecutor构造()，SR6CANServoNum: %d", SR6CANServoNum);
        
        // 初始化电机位置数组
        for (int i = 0; i < SR6CANServoNum; i++) {
            motor_position[i] = 0.0f;
            motor_position_feedback[i] = 0.0f;
            
            // 初始化PID控制器，只使用I控制
            if (i == 2 || i == 3) {
                pid_init(&motor_pid[i], 0.0f, 40.0f, 0.0f, 2.0f);
            } else {
                pid_init(&motor_pid[i], 0.0f, 20.0f, 0.0f, 2.0f);
            }
        }
        
        esp_log_level_set(TAG, ESP_LOG_INFO);
        ESP_LOGI(TAG, "构造()");

        // 从setting.mit中初始化每个电机的MIT控制参数
        motor_kp[0] = setting->mit.Kp_a;
        motor_ki[0] = setting->mit.Ki_a;
        motor_kd[0] = setting->mit.Kd_a;
        motor_offset[0] = setting->mit.offset_a;

        motor_kp[1] = setting->mit.Kp_b;
        motor_ki[1] = setting->mit.Ki_b;
        motor_kd[1] = setting->mit.Kd_b;
        motor_offset[1] = setting->mit.offset_b;

        motor_kp[2] = setting->mit.Kp_c;
        motor_ki[2] = setting->mit.Ki_c;
        motor_kd[2] = setting->mit.Kd_c;
        motor_offset[2] = setting->mit.offset_c;

        motor_kp[3] = setting->mit.Kp_d;
        motor_ki[3] = setting->mit.Ki_d;
        motor_kd[3] = setting->mit.Kd_d;
        motor_offset[3] = setting->mit.offset_d;

        motor_kp[4] = setting->mit.Kp_e;
        motor_ki[4] = setting->mit.Ki_e;
        motor_kd[4] = setting->mit.Kd_e;
        motor_offset[4] = setting->mit.offset_e;

        motor_kp[5] = setting->mit.Kp_f;
        motor_ki[5] = setting->mit.Ki_f;
        motor_kd[5] = setting->mit.Kd_f;
        motor_offset[5] = setting->mit.offset_f;

        ESP_LOGI(TAG, "KP: %f, %f, %f, %f, %f, %f", motor_kp[0], motor_kp[1],
                 motor_kp[2], motor_kp[3], motor_kp[4], motor_kp[5]);
        ESP_LOGI(TAG, "KI: %f, %f, %f, %f, %f, %f", motor_ki[0], motor_ki[1],
                 motor_ki[2], motor_ki[3], motor_ki[4], motor_ki[5]);
        ESP_LOGI(TAG, "KD: %f, %f, %f, %f, %f, %f", motor_kd[0], motor_kd[1],
                 motor_kd[2], motor_kd[3], motor_kd[4], motor_kd[5]);
        ESP_LOGI(TAG, "OFFSET: %f, %f, %f, %f, %f, %f", motor_offset[0], motor_offset[1],
                 motor_offset[2], motor_offset[3], motor_offset[4], motor_offset[5]);

        // 初始化MIT协议
        {
            std::lock_guard<std::mutex> lock(init_mutex_);
            if (!mit_initialized_) {
                esp_err_t ret = MIT::init();
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "MIT初始化失败: %s", esp_err_to_name(ret));
                    throw std::runtime_error("MIT初始化失败");
                }
                mit_initialized_ = true;
                ESP_LOGI(TAG, "MIT初始化完成");
            }
        }

        // 初始化电机
        initMotors();

        // 创建CAN接收任务
        BaseType_t xReturn = xTaskCreate(
            canReceiveTask, "CANReceiveTask", 4096, this, 5,  // 使用优先级5
            &can_receive_task_handle_);
        if (xReturn != pdPASS) {
            ESP_LOGE(TAG, "Failed to create CAN receive task: %d", xReturn);
            throw std::runtime_error("Failed to create CAN receive task");
        }

        init_done = true;
        ESP_LOGI(TAG, "SR6CANExecutor初始化完成");
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "SR6CANExecutor构造失败: %s", e.what());
        // 清理已分配的资源
        if (can_receive_task_handle_ != nullptr) {
            vTaskDelete(can_receive_task_handle_);
            can_receive_task_handle_ = nullptr;
        }
        throw;
    }
}

SR6CANExecutor::~SR6CANExecutor() {
    ESP_LOGI(TAG, "SR6CANExecutor析构()，SR6CANServoNum: %d", SR6CANServoNum);

    // 停止CAN接收任务
    if (can_receive_task_handle_ != nullptr) {
        vTaskDelete(can_receive_task_handle_);
        can_receive_task_handle_ = nullptr;
    }

    // 停止所有电机
    for (int i = 1; i <= SR6CANServoNum; i++) {
        MIT::stop_motor(i);
    }
}

void SR6CANExecutor::compute() {
    std::lock_guard<std::mutex> lock(compute_mutex_);
    // auto now1 = esp_timer_get_time();
    
    // 从tcode中获取插值后的轴值
    // 插值结果顺序为：L0, L1, L2, R0, R1, R2
    float* interpolated = tcode.interpolate();
    float roll, pitch, x, y, z;

    // 处理 thrust (L0)
    y = map_(interpolated[0], 0.0f, 1.0f, m_setting->servo.L0_LEFT, m_setting->servo.L0_RIGHT);
    if (m_setting->servo.L0_REVERSE) {
        y = m_setting->servo.L0_LEFT + m_setting->servo.L0_RIGHT - y;
    }
    y = map_(y, 0.0f, 1.0f, -6000.0f, 6000.0f);
    y *= m_setting->servo.L0_SCALE;

    // 处理 roll (R1)
    roll = map_(interpolated[4], 0.0f, 1.0f, m_setting->servo.R1_LEFT, m_setting->servo.R1_RIGHT);
    if (m_setting->servo.R1_REVERSE) {
        roll = m_setting->servo.R1_LEFT + m_setting->servo.R1_RIGHT - roll;
    }
    roll = map_(roll, 0.0f, 1.0f, -2500.0f, 2500.0f);
    roll *= m_setting->servo.R1_SCALE;

    // 处理 pitch (R2)
    pitch = map_(interpolated[5], 0.0f, 1.0f, m_setting->servo.R2_LEFT, m_setting->servo.R2_RIGHT);
    if (m_setting->servo.R2_REVERSE) {
        pitch = m_setting->servo.R2_LEFT + m_setting->servo.R2_RIGHT - pitch;
    }
    pitch = map_(pitch, 0.0f, 1.0f, -2500.0f, 2500.0f);
    pitch *= m_setting->servo.R2_SCALE;

    // 处理 fwd (L1)
    x = map_(interpolated[1], 0.0f, 1.0f, m_setting->servo.L1_LEFT, m_setting->servo.L1_RIGHT);
    if (m_setting->servo.L1_REVERSE) {
        x = m_setting->servo.L1_LEFT + m_setting->servo.L1_RIGHT - x;
    }
    x = map_(x, 0.0f, 1.0f, -3000.0f, 3000.0f);
    x *= m_setting->servo.L1_SCALE;

    // 处理 side (L2)
    z = map_(interpolated[2], 0.0f, 1.0f, m_setting->servo.L2_LEFT, m_setting->servo.L2_RIGHT);
    if (m_setting->servo.L2_REVERSE) {
        z = m_setting->servo.L2_LEFT + m_setting->servo.L2_RIGHT - z;
    }
    z = map_(z, 0.0f, 1.0f, -3000.0f, 3000.0f);
    z *= m_setting->servo.L2_SCALE;

    auto roll_sin = sinf(roll / 100.0f / 180.0f * M_PI);
    auto d = (18000.0f) / 2.0f;
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
    float lowerLeftValue, upperLeftValue, pitchLeftValue, pitchRightValue,
        upperRightValue, lowerRightValue;
    // 95/2+105=152.5 270*270-152.5*152.5=72900-23256.25=49643.75
    lowerLeftValue = SetMainServo(22280.0f - x, 4750.0f + y + d * roll_sin);
    lowerRightValue = SetMainServo(22280.0f - x, 4750.0f + y - d * roll_sin);
    upperLeftValue = SetMainServo(22280.0f - x, 4750.0f - y - d * roll_sin);
    upperRightValue = SetMainServo(22280.0f - x, 4750.0f - y + d * roll_sin);
    pitchLeftValue = SetPitchServo(22280.0f - x, (4750.0f + 9500.0f) - y - d * roll_sin,
                                   z - (8300.0f * roll_sin), -pitch);
    pitchRightValue = SetPitchServo(22280.0f - x, (4750.0f + 9500.0f) - y + d * roll_sin,
                                    -z + (8300.0f * roll_sin), -pitch);
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

}

void SR6CANExecutor::execute() {
    static double last_motor_position[SR6CANServoNum] = {0.0};
    std::lock_guard<std::mutex> lock(compute_mutex_);
    std::lock_guard<std::mutex> feedback_lock(feedback_mutex_);

    for (int i = 0; i < SR6CANServoNum; i++) {
        motor_position[i] *= 1.33f;
        if (i == 1 || i == 4) {
            motor_position[i] *= -1.0f;
        }
        if (i > 2) {
            motor_position[i] *= -1.0f;
        }
        // 使用PID控制器计算I控制输出（前馈）
        // float pid_output = pid_update(&motor_pid[i], motor_position[i],
        //                               motor_position_feedback[i]);
        // static int j = 0;
        // if (j++ % 100 == 0)
        //     ESP_LOGD(TAG, "motor_pid[0].integral: %f", motor_pid[0].integral);

        // ESP_LOGD(TAG, "motor_position[%d]: %f, feedback: %f, pid_output: %f",
        //          i, motor_position[i] / M_PI * 180.0f,
        //          motor_position_feedback[i] / M_PI * 180.0f,
        //          pid_output);

        MIT::MotorControl status;
        status.position =
            motor_position[i] +
            motor_offset[i] / 180.0f * M_PI;  // 使用原始目标位置加上offset
        status.velocity = 0.0f;
        status.kp = motor_kp[i];  // 从数组中获取PD控制的P参数
        status.kd = motor_kd[i];  // 从数组中获取PD控制的D参数
        // status.torque = pid_output / 1;  // PID的I输出作为前馈
        status.torque = 0.0f;
        MIT::dynamic_control(i + 1, status);
        last_motor_position[i] = motor_position[i];
    }
    static int t = 0;
    if (t++ % getExecuteFrequency() == 0) {
        for (size_t i = 0; i < SR6CANServoNum; i++) {
            ESP_LOGD(TAG, "motor_position[%d]: %f", i,
                     motor_position[i] / 1.33f * 180.0f / M_PI + motor_offset[i]);
        }
    }
}

int SR6CANExecutor::getExecuteFrequency() const {
    return m_setting->servo.A_SERVO_PWM_FREQ;
}

void SR6CANExecutor::initMotors() {
    // 启动所有电机
    for (int i = 1; i <= SR6CANServoNum; i++) {
        // esp_err_t ret = MIT::start_motor(i);
        esp_err_t ret = MIT::stop_motor(i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "启动电机 %d 失败: %s", i, esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

float SR6CANExecutor::SetMainServo(float x, float y) {
    x /= 100.0f;
    y /= 100.0f;
    float gamma = atan2f(x, y);
    float csq = x * x + y * y;
    float c = sqrtf(csq);
    float beta = acosf((csq + 105.0f * 105.0f - 270.0f * 270.0f) / (2.0f * 105.0f * c));
    float result = (gamma + beta - M_PI);
    return result;
}

float SR6CANExecutor::SetPitchServo(float x, float y, float z, float pitch) {
    pitch *= 0.0001745f;            // 角度转换为弧度
    x += 8300.0f * sinf(0.0f + pitch);  // 0.2618rad=15度
    y -= 8300.0f * cosf(0.0f + pitch);
    x /= 100.0f;
    y /= 100.0f;
    z /= 100.0f;
    float bsq = 280.0f * 280.0f - (63.0f + z) * (63.0f + z);
    float gamma = atan2f(x, y);
    float csq = x * x + y * y;
    float c = sqrtf(csq);
    float acos_arg = (csq + 105.0f * 105.0f - bsq) / (2.0f * 105.0f * c);
    // 约束acos参数到[-1, 1]范围，避免nan
    if (acos_arg > 1.0f) {
        acos_arg = 1.0f;
    } else if (acos_arg < -1.0f) {
        acos_arg = -1.0f;
    }
    float beta = acosf(acos_arg);
    float result = (gamma + beta - M_PI);
    return result;
}

void SR6CANExecutor::canReceiveTask(void* param) {
    SR6CANExecutor* self = static_cast<SR6CANExecutor*>(param);

    while (!self->init_done) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(self->TAG, "CAN接收任务启动");
    
    // 检查TWAI状态
    if (!TWAI::is_initialized() || !TWAI::is_started()) {
        ESP_LOGE(self->TAG, "TWAI未初始化或未启动，CAN接收任务退出");
        vTaskDelete(nullptr);
        return;
    }
    
    // int count = 0;
    // int64_t last_time = esp_timer_get_time() / 1000;  // 记录上一次统计时间（毫秒）
    int timeout_count = 0;  // 超时计数器，避免频繁打印
    int loop_count = 0;  // 循环计数器，用于调试
    while (1) {
        loop_count++;
        uint32_t id;
        uint8_t data[8];
        uint8_t data_len;
        bool is_extended;

        // 调用twai::receive函数接收CAN消息
        esp_err_t result =
            TWAI::receive(&id, data, &data_len, &is_extended, 100);
        
        // 每1000次循环打印一次，确认任务在运行
        if (loop_count % 1000 == 0) {
            ESP_LOGD(self->TAG, "CAN接收任务运行中，循环次数: %d", loop_count);
        }

        if (result == ESP_OK) {
            timeout_count = 0;  // 重置超时计数器
            int node_id = id >> 5;
            // ESP_LOGD(self->TAG, "node_id: %d", node_id);
            int cmd_id = id & 0x1F;
            switch (cmd_id) {
                case 9:
                    // count++;
                    // 位置速度
                    float pos;
                    memcpy(&pos, data, 4);
                    float vel;
                    memcpy(&vel, data + 4, 4);
                    {
                        std::lock_guard<std::mutex> lock(self->feedback_mutex_);
                        if (node_id >= 1 && node_id <= SR6CANServoNum) {
                            self->motor_position_feedback[node_id - 1] = pos;
                        }
                    }
                    break;
            }
        } else if (result == ESP_ERR_TIMEOUT) {
            // 超时是正常的，继续循环
            timeout_count++;
            // 每10次超时打印一次，避免频繁打印
            if (timeout_count % 10 == 0) {
                ESP_LOGI(self->TAG, "CAN接收超时 (连续%d次)", timeout_count);
            }
            // 让出CPU，避免饿死其他任务
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            // 其他错误
            timeout_count = 0;  // 重置超时计数器
            ESP_LOGE(self->TAG, "CAN接收错误: %s", esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(10));  // 错误时稍作延时
        }
    }
}

