#include "twai/mit.hpp"
#include "esp_log.h"
#include "twai/twai.hpp"
#include <mutex>

// 静态成员变量定义
bool MIT::initialized = false;
std::recursive_mutex MIT::mutex;

static const char* TAG = "MIT";

esp_err_t MIT::init(int tx_pin, int rx_pin, uint32_t bitrate) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (initialized) {
        ESP_LOGW(TAG, "MIT already initialized");
        return ESP_OK;
    }

    // 初始化TWAI
    esp_err_t ret = TWAI::init(tx_pin, rx_pin, bitrate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TWAI: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动TWAI
    ret = TWAI::start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized = true;
    ESP_LOGI(TAG, "MIT initialized successfully with bitrate=%d", bitrate);
    return ESP_OK;
}

esp_err_t MIT::get_fault(uint8_t nodeid, uint64_t& fault_code) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 查询异常使用 CMD_GET_ERROR 命令
    uint8_t data[8] = {};
    data[0] = 0;  // Error_Type: 0=获取电机异常
    uint32_t can_id = calculate_can_id(nodeid, CMD_GET_ERROR);

    // 发送命令
    esp_err_t ret = TWAI::send(can_id, data, 8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send get error command: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // 接收响应
    uint8_t response_data[8];
    uint8_t response_len;
    ret =
        wait_response(nodeid, CMD_GET_ERROR, 1000, response_data, response_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive error response: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // 解析响应 - 根据手册，错误代码在响应数据中
    if (response_len == 8) {
        swap_endian(response_data, 8);
        memcpy(&fault_code, response_data, 8);
        ESP_LOGI(TAG, "Fault code: %llu (%s)", fault_code,
                 get_fault_description(fault_code).c_str());
    } else {
        ESP_LOGE(TAG, "Invalid error response length: %d", response_len);
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

esp_err_t MIT::clear_fault(uint8_t nodeid) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 清除异常使用 CMD_CLEAR_ERRORS 命令
    uint8_t data[8] = {0};
    uint32_t can_id = calculate_can_id(nodeid, CMD_CLEAR_ERRORS);
    esp_err_t ret = TWAI::send(can_id, data, 8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send clear errors command: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // 等待响应
    uint8_t response_data[8];
    uint8_t response_len;
    ret = wait_response(nodeid, CMD_CLEAR_ERRORS, 1000, response_data,
                        response_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive clear errors response: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Errors cleared successfully");
    return ESP_OK;
}

// esp_err_t MIT::get_clear_fault(uint8_t nodeid,
//                                bool clear_fault,
//                                uint64_t& fault_code) {
//     // 兼容性函数，调用新的分离函数
//     if (clear_fault) {
//         esp_err_t ret = MIT::clear_fault(nodeid);
//         if (ret == ESP_OK) {
//             fault_code = 0;  // 清除后无异常
//         }
//         return ret;
//     } else {
//         return MIT::get_fault(nodeid, fault_code);
//     }
// }

esp_err_t MIT::start_motor(uint8_t nodeid) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    // 使用set_state函数（可重入锁允许重复加锁）
    return set_state(nodeid, AXIS_STATE_CLOSED_LOOP_CONTROL);
}

esp_err_t MIT::stop_motor(uint8_t nodeid) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    // 使用set_state函数（可重入锁允许重复加锁）
    return set_state(nodeid, AXIS_STATE_IDLE);
}

esp_err_t MIT::set_state(uint8_t nodeid, AxisState state) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 准备命令数据 - 设置轴状态
    uint8_t data[8] = {0};
    data[0] = (uint8_t)state;

    // 计算CAN ID: (node_id << 5) | cmd_id
    uint32_t can_id = calculate_can_id(nodeid, CMD_SET_AXIS_STATE);

    // 发送命令
    esp_err_t ret = TWAI::send(can_id, data, 8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send set state command: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Motor (Node ID: %d) set state to %d command sent", nodeid,
             state);
    return ESP_OK;
}

// esp_err_t MIT::set_zero_point(uint8_t nodeid) {
//     // 兼容性函数，调用新的set_state函数
//     return set_state(nodeid, AXIS_STATE_ENCODER_OFFSET_CALIBRATION);
// }

esp_err_t MIT::dynamic_control(uint8_t nodeid, const MotorControl& control) {
    // printf("dynamic_control()\n");
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 打包控制数据
    uint8_t data[8];
    pack_dynamic_control_data(control, data);

    // 计算CAN ID: (node_id << 5) | cmd_id
    uint32_t can_id = calculate_can_id(nodeid, CMD_DYNAMIC_CONTROL);

    // 发送命令
    esp_err_t ret = TWAI::send(can_id, data, 8);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "Failed to send dynamic control command: %s",
        //          esp_err_to_name(ret));
        // printf("E\n");
        return ret;
    }

    // 等待响应
    // uint8_t response_data[8];
    // uint8_t response_len;
    // ret = wait_response(nodeid, CMD_DYNAMIC_CONTROL, 1000, response_data,
    //                     response_len);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to receive dynamic control response: %s",
    //              esp_err_to_name(ret));
    //     return ret;
    // }

    // // 解析响应数据
    // MotorStatus status;
    // unpack_dynamic_control_response(response_data, response_len, status);

    // ESP_LOGD(TAG,
    //          "Motor (Node ID: %d) dynamic control command sent, response: "
    //          "pos=%.3f, vel=%.3f, torque=%.3f",
    //          nodeid, status.position, status.velocity, status.torque);
    return ESP_OK;
}

esp_err_t MIT::receive_status(int timeout_ms, MotorStatus& status) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 接收TWAI消息
    uint32_t id;
    uint8_t data[8];
    uint8_t data_len;
    bool is_extended;

    esp_err_t ret =
        TWAI::receive(&id, data, &data_len, &is_extended, timeout_ms);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Failed to receive status: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    // 解包状态数据
    unpack_status_data(data, data_len, status);
    // 从CAN ID中提取node_id部分
    status.can_id = (uint8_t)(id >> 5);

    ESP_LOGD(TAG,
             "Motor status received: ID=%d, pos=%.3f, vel=%.3f, torque=%.3f, "
             "fault=0x%02X",
             status.can_id, status.position, status.velocity, status.torque,
             status.fault_code);

    return ESP_OK;
}

esp_err_t MIT::wait_response(uint8_t nodeid,
                             Command command,
                             int timeout_ms,
                             uint8_t* response_data,
                             uint8_t& response_len) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 接收响应
    uint32_t id;
    bool is_extended;

    esp_err_t ret = TWAI::receive(&id, response_data, &response_len,
                                  &is_extended, timeout_ms);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Failed to receive command response: %s",
                     esp_err_to_name(ret));
        }
        return ret;
    }

    // 检查响应ID是否匹配 - 从CAN ID中提取node_id部分进行比较
    uint8_t response_nodeid = (uint8_t)(id >> 5);
    if (response_nodeid != nodeid) {
        ESP_LOGW(TAG, "Response Node ID mismatch: expected %d, got %d", nodeid,
                 response_nodeid);
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

std::string MIT::get_fault_description(uint64_t fault_code) {
    std::string description = "";

    if (fault_code == FAULT_NONE) {
        description = "无异常";
    } else {
        if (fault_code & FAULT_PHASE_RESISTANCE_OUT_OF_RANGE)
            description += "相间电阻超出正常范围; ";
        if (fault_code & FAULT_PHASE_INDUCTANCE_OUT_OF_RANGE)
            description += "相间电感超出正常范围; ";
        if (fault_code & FAULT_CONTROL_DEADLINE_MISSED)
            description += "FOC频率太高; ";
        if (fault_code & FAULT_MODULATION_MAGNITUDE)
            description += "SVM调制异常; ";
        if (fault_code & FAULT_CURRENT_SENSE_SATURATION)
            description += "相电流饱和; ";
        if (fault_code & FAULT_CURRENT_LIMIT_VIOLATION)
            description += "电机电流过大; ";
        if (fault_code & FAULT_MOTOR_THERMISTOR_OVER_TEMP)
            description += "电机温度过高; ";
        if (fault_code & FAULT_FET_THERMISTOR_OVER_TEMP)
            description += "驱动器温度过高; ";
        if (fault_code & FAULT_TIMER_UPDATE_MISSED)
            description += "FOC处理不及时; ";
        if (fault_code & FAULT_CURRENT_MEASUREMENT_UNAVAILABLE)
            description += "相电流采样丢失; ";
        if (fault_code & FAULT_CONTROLLER_FAILED)
            description += "控制异常; ";
        if (fault_code & FAULT_I_BUS_OUT_OF_RANGE)
            description += "母线电流超限; ";
        if (fault_code & FAULT_BRAKE_RESISTOR_DISARMED)
            description += "刹车电阻驱动异常; ";
        if (fault_code & FAULT_SYSTEM_LEVEL)
            description += "系统级异常; ";
        if (fault_code & FAULT_BAD_TIMING)
            description += "相电流采样不及时; ";
        if (fault_code & FAULT_UNKNOWN_PHASE_ESTIMATE)
            description += "电机位置未知; ";
        if (fault_code & FAULT_UNKNOWN_PHASE_VEL)
            description += "电机速度未知; ";
        if (fault_code & FAULT_UNKNOWN_TORQUE)
            description += "力矩未知; ";
        if (fault_code & FAULT_UNKNOWN_CURRENT_COMMAND)
            description += "力矩控制未知; ";
        if (fault_code & FAULT_UNKNOWN_CURRENT_MEASUREMENT)
            description += "电流采样值未知; ";
        if (fault_code & FAULT_UNKNOWN_VBUS_VOLTAGE)
            description += "电压采样值未知; ";
        if (fault_code & FAULT_UNKNOWN_VOLTAGE_COMMAND)
            description += "电压控制未知; ";
        if (fault_code & FAULT_UNKNOWN_GAINS)
            description += "电流环增益未知; ";
        if (fault_code & FAULT_CONTROLLER_INITIALIZING)
            description += "控制器初始化异常; ";
        if (fault_code & FAULT_UNBALANCED_PHASES)
            description += "三相不平衡; ";
    }

    return description;
}

uint16_t MIT::position_to_int(double position) {
    // pos_int = (pos_double + 12.5) * 65535 / 25
    return (uint16_t)((position + 15.91) * 65535.0 / 31.82);
}

double MIT::int_to_position(uint16_t pos_int) {
    // pos_double = pos_int * 31.82 / 65535 - 15.91
    return 2 * ((double)pos_int * 31.82 / 65535.0 - 15.91);
}

int16_t MIT::velocity_to_int(double velocity) {
    // vel_int = (vel_double + 65) * 4095 / 130
    return (int16_t)((velocity + 82.73) * 4095.0 / 165.46);
}

double MIT::int_to_velocity(int16_t vel_int) {
    // vel_double = vel_int * 165.46 / 4095 - 82.73
    return (double)((uint16_t)vel_int) * 165.46 / 4095.0 - 82.73;
}

int16_t MIT::kp_to_int(double kp) {
    // kp_int = kp_double * 4095 / 500
    return (int16_t)(kp * 4095.0 / 500.0);
}

double MIT::int_to_kp(int16_t kp_int) {
    // kp_double = kp_int * 500 / 4095
    return (double)kp_int * 500.0 / 4095.0;
}

int16_t MIT::kd_to_int(double kd) {
    // kd_int = kd_double * 4095 / 5
    return (int16_t)(kd * 4095.0 / 5.0);
}

double MIT::int_to_kd(int16_t kd_int) {
    // kd_double = kd_int * 5 / 4095
    return (double)kd_int * 5.0 / 4095.0;
}

int16_t MIT::torque_to_int(double torque) {
    // t_int = (t_double + 225 * 转矩常数 * 减速比) * 4095 / (450 * 转矩常数 *
    // 减速比)
    // double factor = torque_constant * reduction_ratio;
    return (int16_t)((torque + 6.24) * 4095.0 / (12.48));
}

double MIT::int_to_torque(int16_t t_int) {
    // t_double = t_int * (450 * 转矩常数 * 减速比) / 4095 - 225 * 转矩常数 *
    // 减速比
    // double factor = torque_constant * reduction_ratio;
    return (double)t_int * (12.48) / 4095.0 - 6.24;
}

void MIT::pack_dynamic_control_data(const MotorControl& control,
                                    uint8_t data[8]) {
    // 位置：16位，BYTE0为高8位，BYTE1为低8位
    uint16_t pos_int = position_to_int(control.position);
    data[0] = (uint8_t)(pos_int >> 8);
    data[1] = (uint8_t)(pos_int & 0xFF);

    // 速度：12位，BYTE2为高8位，BYTE3[7-4]为低4位
    int16_t vel_int = velocity_to_int(control.velocity);
    data[2] = (uint8_t)(vel_int >> 4);
    data[3] = (uint8_t)((vel_int & 0x0F) << 4);

    // KP值：12位，BYTE3[3-0]为高4位，BYTE4为低8位
    int16_t kp_int = kp_to_int(control.kp);
    data[3] |= (uint8_t)((kp_int >> 8) & 0x0F);
    data[4] = (uint8_t)(kp_int & 0xFF);

    // KD值：12位，BYTE5为高8位，BYTE6[7-4]为低4位
    int16_t kd_int = kd_to_int(control.kd);
    data[5] = (uint8_t)(kd_int >> 4);
    data[6] = (uint8_t)((kd_int & 0x0F) << 4);

    // 力矩：12位，BYTE6[3-0]为高4位，BYTE7为低8位
    int16_t torque_int = torque_to_int(control.torque);
    data[6] |= (uint8_t)((torque_int >> 8) & 0x0F);
    data[7] = (uint8_t)(torque_int & 0xFF);
}

void MIT::unpack_status_data(const uint8_t* data,
                             uint8_t data_len,
                             MotorStatus& status) {
    if (data_len < 6) {
        ESP_LOGE(TAG, "Invalid status data length: %d (expected >= 6)",
                 data_len);
        return;
    }

    // 根据手册，MIT控制响应格式：
    // BYTE0: node id
    // 位置：16位，BYTE1为高8位，BYTE2为低8位
    uint16_t pos_int = (uint16_t)((data[1] << 8) | data[2]);
    printf("pos_int:%hu\n", pos_int);
    status.position = int_to_position(pos_int);

    // 速度：12位，BYTE3为高8位，BYTE4[7-4]为低4位
    int16_t vel_int = (int16_t)((data[3] << 4) | (data[4] >> 4));
    printf("vel_int:%hu\n", vel_int);
    status.velocity = int_to_velocity(vel_int);

    // 力矩：12位，BYTE4[3-0]为高4位，BYTE5为低8位
    int16_t torque_int = (int16_t)(((data[4] & 0x0F) << 8) | data[5]);
    status.torque = int_to_torque(torque_int);

    // 异常代码：如果是获取异常命令的响应，需要特殊处理
    // 这里暂时设为0，具体解析需要根据命令类型
    status.fault_code = 0;
}

void MIT::unpack_dynamic_control_response(const uint8_t* data,
                                          uint8_t data_len,
                                          MotorStatus& status) {
    if (data_len < 5) {
        ESP_LOGE(TAG,
                 "Invalid dynamic control response length: %d (expected >= 5)",
                 data_len);
        return;
    }

    // 根据任务描述，动态控制响应格式：
    // BYTE0: 电机ID
    status.can_id = data[0];

    // 位置：16位，BYTE1为高8位，BYTE2为低8位
    int16_t pos_int = (int16_t)((data[1] << 8) | data[2]);
    // printf("pos_int: %d\n", pos_int);
    status.position = int_to_position(pos_int);

    // 速度：12位，BYTE3为高8位，BYTE4[7-4]为低4位
    int16_t vel_int = (int16_t)((data[3] << 4) | (data[4] >> 4));
    // printf("vel_int: %d\n", vel_int);
    status.velocity = int_to_velocity(vel_int);

    // 力矩：12位，BYTE4[3-0]为高4位，BYTE5为低8位
    int16_t torque_int = (int16_t)(((data[4] & 0x0F) << 8) | data[5]);
    // printf("torque_int: %d\n", torque_int);
    status.torque = int_to_torque(torque_int);

    // 异常代码：动态控制响应中不包含异常代码，设为0
    status.fault_code = 0;
}

esp_err_t MIT::dynamic_control_with_response(uint8_t nodeid,
                                             const MotorControl& control,
                                             MotorStatus& status) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 打包控制数据
    uint8_t data[8];
    pack_dynamic_control_data(control, data);

    // 计算CAN ID: (node_id << 5) | cmd_id
    uint32_t can_id = calculate_can_id(nodeid, CMD_DYNAMIC_CONTROL);

    // 发送命令
    esp_err_t ret = TWAI::send(can_id, data, 8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send dynamic control command: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // 等待响应
    uint8_t response_data[8];
    uint8_t response_len;
    ret = wait_response(nodeid, CMD_DYNAMIC_CONTROL, 1000, response_data,
                        response_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive dynamic control response: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // 解析响应数据
    unpack_dynamic_control_response(response_data, response_len, status);

    ESP_LOGD(TAG, "Motor (Node ID: %d) dynamic control with response completed",
             nodeid);
    return ESP_OK;
}

esp_err_t MIT::setPos(uint8_t nodeid, float position) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!initialized) {
        ESP_LOGE(TAG, "MIT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 准备命令数据 - float32位置值
    uint8_t data[8] = {0};
    // memcpy(data, &position, sizeof(float));

    // 如果需要大小端转换，可以使用swap_endian函数
    // swap_endian(data, sizeof(float));

    // 计算CAN ID: (node_id << 5) | cmd_id
    uint32_t can_id = calculate_can_id(nodeid, CMD_SET_POSITION);

    // 发送命令
    esp_err_t ret = TWAI::send(can_id, data, sizeof(float));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send setpos command: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Motor (Node ID: %d) set position to %.3f command sent",
             nodeid, position);
    return ESP_OK;
}

uint32_t MIT::calculate_can_id(uint8_t nodeid, uint8_t cmdid) {
    // CAN Simple协议：CAN ID = (node_id << 5) | cmd_id
    // node_id: Bit10~Bit5 (6位，范围0-63)
    // cmd_id: Bit4~Bit0 (5位，范围0-31)
    return ((uint32_t)nodeid << 5) | (cmdid & 0x1F);
}

void MIT::swap_endian(void* data, size_t length) {
    if (data == nullptr || length <= 1) {
        return;
    }

    uint8_t* bytes = (uint8_t*)data;

    // 逐字节翻转
    for (size_t i = 0; i < length / 2; i++) {
        uint8_t temp = bytes[i];
        bytes[i] = bytes[length - 1 - i];
        bytes[length - 1 - i] = temp;
    }
}