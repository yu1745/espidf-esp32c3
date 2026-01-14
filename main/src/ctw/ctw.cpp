#include "ctw/ctw.hpp"
#include "esp_timer.h"
#include <cinttypes>

// 静态成员变量初始化
bool CTW::initialized_ = false;
bool CTW::started_ = false;
uint32_t CTW::current_bitrate_ = 0;
std::mutex CTW::init_mutex_;
std::mutex CTW::send_mutex_;
TaskHandle_t CTW::receive_task_handle_ = nullptr;
bool CTW::receive_task_running_ = false;
CTW::MotorFeedback CTW::feedback_cache_[8];
std::mutex CTW::feedback_mutex_;

// CAN 占用率统计变量初始化
uint64_t CTW::total_bits_sent_ = 0;
uint64_t CTW::total_bits_received_ = 0;
int64_t CTW::last_stats_time_ = 0;
std::mutex CTW::stats_mutex_;

// 事件基定义
ESP_EVENT_DEFINE_BASE(CTW_CAN_USAGE_EVENT);

static const char* TAG = "CTW";

// ========== CAN 占用率事件处理器 ==========

/**
 * @brief CAN 占用率事件处理器
 * @param handler_arg 处理器参数（未使用）
 * @param event_base 事件基
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
static void can_usage_event_handler(void* handler_arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void* event_data) {
    if (event_base == CTW_CAN_USAGE_EVENT && 
        event_id == CTW_CAN_USAGE_EVENT_BUS_UTILIZATION) {
        ctw_can_usage_event_data_t* data = 
            (ctw_can_usage_event_data_t*)event_data;
        
        ESP_LOGI(TAG, 
                 "CAN总线占用率: %.2f%%, 发送: %" PRIu64 " bits, 接收: %" PRIu64 " bits, 波特率: %lu bps",
                 data->bus_utilization_percent,
                 data->total_bits_sent,
                 data->total_bits_received,
                 data->bitrate);
    }
}

// ========== 初始化和反初始化 ==========

esp_err_t CTW::init(int tx_pin, int rx_pin, uint32_t bitrate) {
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (initialized_) {
        ESP_LOGW(TAG, "CTW already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing CTW with TX=%d, RX=%d, bitrate=%lu", tx_pin,
             rx_pin, bitrate);

    // 配置TWAI通用配置
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)tx_pin, (gpio_num_t)rx_pin,
        TWAI_MODE_NORMAL);

    // 配置TWAI时序配置
    twai_timing_config_t t_config;
    if (bitrate == 1000000) {
        t_config = TWAI_TIMING_CONFIG_1MBITS();
    } else if (bitrate == 800000) {
        t_config = TWAI_TIMING_CONFIG_800KBITS();
    } else if (bitrate == 500000) {
        t_config = TWAI_TIMING_CONFIG_500KBITS();
    } else if (bitrate == 250000) {
        t_config = TWAI_TIMING_CONFIG_250KBITS();
    } else if (bitrate == 125000) {
        t_config = TWAI_TIMING_CONFIG_125KBITS();
    } else if (bitrate == 100000) {
        t_config = TWAI_TIMING_CONFIG_100KBITS();
    } else if (bitrate == 50000) {
        t_config = TWAI_TIMING_CONFIG_50KBITS();
    } else if (bitrate == 25000) {
        t_config = TWAI_TIMING_CONFIG_25KBITS();
    } else {
        // 自定义波特率
        t_config.brp = 80; // 默认值，需要根据实际计算
        ESP_LOGW(TAG, "Using default timing for custom bitrate %lu", bitrate);
    }

    // 配置TWAI过滤器配置（接收所有消息）
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // 安装TWAI驱动
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // 保存波特率
    current_bitrate_ = bitrate;

    // 初始化反馈缓存
    for (int i = 0; i < 6; i++) {
        feedback_cache_[i] = MotorFeedback();
    }

    // 设置 initialized_ = true，以便 start() 可以正常执行
    // 如果后续步骤失败，会在错误处理中重置
    initialized_ = true;

    // 启动驱动 
    ret = start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(ret));
        initialized_ = false;
        twai_driver_uninstall();
        return ret;
    }

    // 自动启动接收任务
    ret = start_receive_task(5);  // 使用固定优先级5
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start receive task: %s", esp_err_to_name(ret));
        stop();
        initialized_ = false;
        started_ = false;
        twai_driver_uninstall();
        return ret;
    }

    // 重置统计计数器和时间戳
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_bits_sent_ = 0;
        total_bits_received_ = 0;
        last_stats_time_ = esp_timer_get_time();
    }

    // 确保默认事件循环已创建
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        stop_receive_task();
        stop();
        initialized_ = false;
        started_ = false;
        twai_driver_uninstall();
        return ret;
    }

    // 注册 CAN 占用率事件处理器到全局默认事件循环
    ret = esp_event_handler_register(CTW_CAN_USAGE_EVENT,
                                     CTW_CAN_USAGE_EVENT_BUS_UTILIZATION,
                                     can_usage_event_handler,
                                     nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register CAN usage event handler: %s", esp_err_to_name(ret));
        stop_receive_task();
        stop();
        initialized_ = false;
        started_ = false;
        twai_driver_uninstall();
        return ret;
    }

    ESP_LOGI(TAG, "CTW initialized successfully (bitrate=%d)", bitrate);
    return ESP_OK;
}

esp_err_t CTW::deinit() {
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (!initialized_) {
        ESP_LOGW(TAG, "CTW not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 停止接收任务
    stop_receive_task();

    // 注销 CAN 占用率事件处理器
    esp_event_handler_unregister(CTW_CAN_USAGE_EVENT,
                                 CTW_CAN_USAGE_EVENT_BUS_UTILIZATION,
                                 can_usage_event_handler);

    // 停止驱动
    if (started_) {
        esp_err_t ret = stop();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop TWAI driver: %s", esp_err_to_name(ret));
        }
    }

    // 卸载驱动
    esp_err_t ret = twai_driver_uninstall();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to uninstall TWAI driver: %s", esp_err_to_name(ret));
    }

    // 重置统计计数器
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_bits_sent_ = 0;
        total_bits_received_ = 0;
    }

    initialized_ = false;
    started_ = false;
    current_bitrate_ = 0;
    ESP_LOGI(TAG, "CTW deinitialized");
    return ESP_OK;
}

// ========== 电机控制函数 ==========

esp_err_t CTW::set_position(uint8_t node_id, float position) {
    // if (!initialized_) {
    //     return ESP_ERR_INVALID_STATE;
    // }

    // if (node_id < 1 || node_id > 8) {
    //     ESP_LOGE(TAG, "Invalid node_id: %d (must be 1-8)", node_id);
    //     return ESP_ERR_INVALID_ARG;
    // }

    // // 使用 CANSimple 协议 CmdID 0x003 (Set_Input_Pos)
    // uint8_t data[8] = {0};
    // memcpy(data, &position, sizeof(float));

    // // CAN ID = (NodeID << 5) | CmdID
    // uint32_t can_id = ((uint32_t)node_id << 5) | 0x003;

    // return send_can_message(can_id, data, 8);
    return write_endpoint_float(node_id, EID_AXIS0_CONTROLLER_INPUT_POS, position);
}

esp_err_t CTW::set_velocity(uint8_t node_id, float velocity) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    // 使用 CANSimple 协议 CmdID 0x004 (Set_Input_Vel)
    uint8_t data[8] = {0};
    memcpy(data, &velocity, sizeof(float));

    // CAN ID = (NodeID << 5) | CmdID
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x004;

    return send_can_message(can_id, data, 8);
}

esp_err_t CTW::set_torque(uint8_t node_id, float torque) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    // 使用 CANSimple 协议 CmdID 0x005 (Set_Input_Torque)
    uint8_t data[8] = {0};
    memcpy(data, &torque, sizeof(float));

    // CAN ID = (NodeID << 5) | CmdID
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x005;

    return send_can_message(can_id, data, 8);
}

// ========== 状态读取函数 ==========

esp_err_t CTW::get_position(uint8_t node_id, float& position, int timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    return read_endpoint(node_id, EID_AXIS0_ENCODER_POS_ESTIMATE, &position,
                         sizeof(float), timeout_ms);
}

esp_err_t CTW::get_velocity(uint8_t node_id, float& velocity, int timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    return read_endpoint(node_id, EID_AXIS0_ENCODER_VEL_ESTIMATE, &velocity,
                         sizeof(float), timeout_ms);
}

esp_err_t CTW::get_current_state(uint8_t node_id,
                                 uint8_t& state,
                                 int timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    return read_endpoint(node_id, EID_AXIS0_CURRENT_STATE, &state,
                         sizeof(uint8_t), timeout_ms);
}

esp_err_t CTW::get_feedback(uint8_t node_id,
                            MotorFeedback& feedback,
                            int timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;

    // 读取位置
    ret = get_position(node_id, feedback.position, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    // 读取速度
    ret = get_velocity(node_id, feedback.velocity, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    // 读取状态
    ret = get_current_state(node_id, feedback.axis_state, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    // 读取电机使能状态（使用 bool 专用函数）
    ret = read_endpoint_bool(node_id, EID_AXIS0_MOTOR_IS_ARMED,
                             feedback.motor_armed, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read motor armed state: %s",
                 esp_err_to_name(ret));
        // 不影响整体反馈，继续执行
    }

    feedback.last_update = ((uint32_t)(esp_timer_get_time() / 1000));
    return ESP_OK;
}

// ========== Endpoint 读写函数 ==========

esp_err_t CTW::write_endpoint_float(uint8_t node_id,
                                    uint16_t endpoint_id,
                                    float value) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[8];
    pack_endpoint_data(1, endpoint_id, data, &value,
                       sizeof(float));  // opcode=1 (写)
    // for (int i = 0; i < 8; i++) {
    //     printf("0x%02x ", data[i]);
    // }
    // printf("\n");
    // 计算 CAN ID（SteadyWin 自定义 SDO 协议）
    // CAN ID = (NodeID << 5) | CmdID
    // RxSDO (写): CmdID = 4
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x04;

    return send_can_message(can_id, data, 8);
}

esp_err_t CTW::write_endpoint_uint32(uint8_t node_id,
                                     uint16_t endpoint_id,
                                     uint32_t value) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[8];
    pack_endpoint_data(1, endpoint_id, data, &value,
                       sizeof(uint32_t));  // opcode=1 (写)

    // RxSDO (写): CmdID = 4
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x04;

    return send_can_message(can_id, data, 8);
}

esp_err_t CTW::write_endpoint_int32(uint8_t node_id,
                                    uint16_t endpoint_id,
                                    int32_t value) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[8];
    pack_endpoint_data(1, endpoint_id, data, &value,
                       sizeof(int32_t));  // opcode=1 (写)

    // RxSDO (写): CmdID = 4
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x04;

    return send_can_message(can_id, data, 8);
}

esp_err_t CTW::write_endpoint_uint8(uint8_t node_id,
                                    uint16_t endpoint_id,
                                    uint8_t value) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[8];
    pack_endpoint_data(1, endpoint_id, data, &value,
                       sizeof(uint8_t));  // opcode=1 (写)

    // RxSDO (写): CmdID = 4
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x04;

    return send_can_message(can_id, data, 8);
}

esp_err_t CTW::write_endpoint_bool(uint8_t node_id,
                                   uint16_t endpoint_id,
                                   bool value) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    // bool 转换为 uint8 (0 或 1)
    uint8_t bool_value = value ? 1 : 0;
    return write_endpoint_uint8(node_id, endpoint_id, bool_value);
}

esp_err_t CTW::read_endpoint(uint8_t node_id,
                             uint16_t endpoint_id,
                             void* value,
                             size_t value_size,
                             int timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    // 准备读取命令（opcode=0 表示读）
    uint8_t data[8] = {0};
    pack_endpoint_data(0, endpoint_id, data, nullptr, 0);  // opcode=0 (读)

    // 计算 CAN ID（SteadyWin 自定义 SDO 协议）
    // TxSDO (读): CmdID = 5
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x05;

    // 发送读取请求
    esp_err_t ret = send_can_message(can_id, data, 8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send read request: %s", esp_err_to_name(ret));
        return ret;
    }

    // 接收响应
    uint8_t response_data[8];
    uint8_t response_len;
    ret =
        receive_can_message(&can_id, response_data, &response_len, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive read response: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // 检查响应长度（至少需要 4 字节头部 + value_size 字节数据）
    if (response_len < 4 + value_size) {
        ESP_LOGE(TAG, "Invalid response length: %d (expected >= %d)",
                 response_len, 4 + value_size);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // 解包数据（跳过前 4 字节：opcode + endpoint_id + reserved）
    unpack_endpoint_data(response_data, value, value_size);

    ESP_LOGD(TAG, "Node %d: read endpoint 0x%03X", node_id, endpoint_id);
    return ESP_OK;
}

esp_err_t CTW::read_endpoint_bool(uint8_t node_id,
                                  uint16_t endpoint_id,
                                  bool& value,
                                  int timeout_ms) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    // 读取为 uint8，然后转换为 bool
    uint8_t temp_value;
    esp_err_t ret = read_endpoint(node_id, endpoint_id, &temp_value,
                                  sizeof(uint8_t), timeout_ms);
    if (ret == ESP_OK) {
        value = (temp_value != 0);
    }

    return ret;
}

// ========== 电机配置函数 ==========

esp_err_t CTW::set_axis_state(uint8_t node_id, AxisState state) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    // 使用 CANSimple 协议 CmdID 0x007 (Set_Axis_State)
    // 数据格式：Byte 0-3: requested_state (uint32)
    uint8_t data[8] = {0};
    uint32_t state_val = (uint32_t)state;
    memcpy(data, &state_val, sizeof(uint32_t));

    // CAN ID = (NodeID << 5) | CmdID
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x007;

    return send_can_message(can_id, data, 8);
}

esp_err_t CTW::start_motor(uint8_t node_id) {
    return set_axis_state(node_id, AXIS_CLOSED_LOOP_CONTROL);
}

esp_err_t CTW::stop_motor(uint8_t node_id) {
    return set_axis_state(node_id, AXIS_STATE_IDLE);
}

esp_err_t CTW::set_controller_mode(uint8_t node_id,
                                   ControllerMode control_mode,
                                   InputMode input_mode) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    // 使用 CANSimple 协议 CmdID 0x013 (Set_Controller_Mode)
    // 数据格式：Byte 0-3: Control_Mode (uint32), Byte 4-7: Input_Mode (uint32)
    uint8_t data[8] = {0};
    uint32_t control_mode_val = (uint32_t)control_mode;
    uint32_t input_mode_val = (uint32_t)input_mode;

    memcpy(data, &control_mode_val, sizeof(uint32_t));
    memcpy(data + 4, &input_mode_val, sizeof(uint32_t));

    // CAN ID = (NodeID << 5) | CmdID
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x0B;

    return send_can_message(can_id, data, 8);
}

// esp_err_t CTW::set_motor_config(uint8_t node_id, const MotorConfig& config) {
//     esp_err_t ret;

//     ret = write_endpoint_float(node_id, EID_AXIS0_CONTROLLER_CONFIG_POS_GAIN,
//                                config.pos_gain);
//     if (ret != ESP_OK)
//         return ret;

//     ret = write_endpoint_float(node_id, EID_AXIS0_CONTROLLER_CONFIG_VEL_GAIN,
//                                config.vel_gain);
//     if (ret != ESP_OK)
//         return ret;

//     ret = write_endpoint_float(node_id,
//                                EID_AXIS0_CONTROLLER_CONFIG_VEL_INTEGRATOR_GAIN,
//                                config.vel_integrator_gain);
//     if (ret != ESP_OK)
//         return ret;

//     ret = write_endpoint_float(node_id, EID_AXIS0_CONTROLLER_CONFIG_VEL_LIMIT,
//                                config.vel_limit);
//     if (ret != ESP_OK)
//         return ret;

//     return write_endpoint_float(node_id, EID_AXIS0_MOTOR_CONFIG_CURRENT_LIMIT,
//                                 config.current_limit);
// }

esp_err_t CTW::set_filter_bandwidth(uint8_t node_id, float bandwidth) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    return write_endpoint_float(
        node_id, EID_AXIS0_CONTROLLER_CONFIG_INPUT_FILTER_BANDWIDTH, bandwidth);
}

esp_err_t CTW::clear_errors(uint8_t node_id) {
    // 调用清除错误函数（endpoint ID 0x1E0）
    uint8_t data[8] = {0};
    pack_endpoint_data(1, 0x1E0, data, nullptr, 0);  // opcode=1 (写)

    // RxSDO (写): CmdID = 4
    uint32_t can_id = ((uint32_t)node_id << 5) | 0x04;

    return send_can_message(can_id, data, 8);
}

// ========== 接收任务管理 ==========

esp_err_t CTW::start_receive_task(uint8_t task_priority) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (receive_task_running_) {
        ESP_LOGW(TAG, "Receive task already running");
        return ESP_OK;
    }

    receive_task_running_ = true;

    BaseType_t ret = xTaskCreate(receive_task, "CTW_Receive", 4096, nullptr,
                                 task_priority, &receive_task_handle_);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receive task");
        receive_task_running_ = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receive task started");
    return ESP_OK;
}

esp_err_t CTW::stop_receive_task() {
    if (!receive_task_running_) {
        return ESP_OK;
    }

    receive_task_running_ = false;

    // 等待任务自然结束
    vTaskDelay(pdMS_TO_TICKS(100));

    if (receive_task_handle_ != nullptr) {
        vTaskDelete(receive_task_handle_);
        receive_task_handle_ = nullptr;
    }

    ESP_LOGI(TAG, "Receive task stopped");
    return ESP_OK;
}

void CTW::receive_task(void* param) {
    ESP_LOGI(TAG, "Receive task running");

    uint32_t id;
    uint8_t data[8];
    uint8_t data_len;
    bool is_extended;

    while (receive_task_running_) {
        // 接收 CAN 消息
        esp_err_t ret = receive(&id, data, &data_len, &is_extended, 100);

        if (ret == ESP_OK) {
            // 提取 node_id 和 cmd_id
            // CAN ID = (NodeID << 5) | CmdID
            uint8_t node_id = (uint8_t)(id >> 5);
            uint8_t cmd_id = id & 0x1F;

            switch (cmd_id) {
                case 9: {
                    // 位置速度反馈 (cmd_id=9)
                    if (node_id >= 1 && node_id <= 8 && data_len >= 8) {
                        float pos;
                        memcpy(&pos, data, 4);
                        float vel;
                        memcpy(&vel, data + 4, 4);

                        std::lock_guard<std::mutex> lock(feedback_mutex_);
                        feedback_cache_[node_id - 1].position = pos;
                        feedback_cache_[node_id - 1].velocity = vel;
                        feedback_cache_[node_id - 1].last_update = ((uint32_t)(esp_timer_get_time() / 1000));
                    }
                    break;
                }
                case 5: {
                    // TxSDO (cmd_id=5) 的响应消息
                    if (node_id >= 1 && node_id <= 8 && data_len >= 6) {
                        // 从数据域提取 endpoint_id（Byte 1-2，小端序）
                        uint16_t endpoint_id = data[1] | ((uint16_t)data[2] << 8);

                        std::lock_guard<std::mutex> lock(feedback_mutex_);

                        // 根据不同的 endpoint ID 更新缓存
                        if (endpoint_id == EID_AXIS0_ENCODER_POS_ESTIMATE &&
                            data_len >= 8) {
                            // 位置反馈（跳过前 4 字节：opcode + endpoint_id +
                            // reserved）
                            unpack_endpoint_data(data,
                                                 &feedback_cache_[node_id - 1].position,
                                                 sizeof(float));
                            feedback_cache_[node_id - 1].last_update = ((uint32_t)(esp_timer_get_time() / 1000));
                        } else if (endpoint_id == EID_AXIS0_ENCODER_VEL_ESTIMATE &&
                                   data_len >= 8) {
                            unpack_endpoint_data(data,
                                                 &feedback_cache_[node_id - 1].velocity,
                                                 sizeof(float));
                            feedback_cache_[node_id - 1].last_update = ((uint32_t)(esp_timer_get_time() / 1000));
                        } else if (endpoint_id == EID_AXIS0_CURRENT_STATE &&
                                   data_len >= 5) {
                            unpack_endpoint_data(
                                data, &feedback_cache_[node_id - 1].axis_state,
                                sizeof(uint8_t));
                            feedback_cache_[node_id - 1].last_update = ((uint32_t)(esp_timer_get_time() / 1000));
                        }
                    }
                    break;
                }
                case 1:
                case 10:
                    // 心跳等其他消息，忽略
                    break;
                default: {
                    // 打印未处理的CAN消息
                    ESP_LOGD(TAG, "NodeID: %d, CmdID: 0x%02x, Data: ", node_id, cmd_id);
                    for (int j = 0; j < data_len; j++) {
                        ESP_LOGD(TAG, "%02x ", data[j]);
                    }
                    ESP_LOGD(TAG, "\n");
                    break;
                }
            }
        } else if (ret != ESP_ERR_TIMEOUT) {
            // 非超时错误，记录日志
            ESP_LOGW(TAG, "Receive error: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    ESP_LOGI(TAG, "Receive task exiting");
    vTaskDelete(nullptr);
}

esp_err_t CTW::get_cached_feedback(uint8_t node_id, MotorFeedback& feedback) {
    if (node_id < 1 || node_id > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    std::lock_guard<std::mutex> lock(feedback_mutex_);
    feedback = feedback_cache_[node_id - 1];

    return ESP_OK;
}

// ========== 辅助函数 ==========

void CTW::pack_endpoint_data(uint8_t opcode,
                             uint16_t endpoint_id,
                             uint8_t data[8],
                             const void* value,
                             size_t value_size) {
    memset(data, 0, 8);

    // Byte 0: opcode (0=读, 1=写)
    data[0] = opcode;

    // Byte 1-2: Endpoint_ID (大端序)
    memcpy(data + 1, &endpoint_id, sizeof(uint16_t));
    // swap_endian(data + 1, sizeof(uint16_t));

    // Byte 3: 预留（已为 0）

    // Byte 4-7: Value（如果提供）
    if (value && value_size > 0 && value_size <= 4) {
        memcpy(data + 4, value, value_size);
        // swap_endian(data + 4, value_size);
    }
}

void CTW::unpack_endpoint_data(const uint8_t data[8],
                               void* value,
                               size_t value_size) {
    // 跳过前 4 字节：opcode(1) + endpoint_id(2) + reserved(1)
    // ESP32 和 SteadyWin 都是小端序，不需要字节序转换
    memcpy(value, data + 4, value_size);
}

esp_err_t CTW::send_can_message(uint32_t id,
                                const uint8_t* data,
                                uint8_t data_len) {
    std::lock_guard<std::mutex> lock(send_mutex_);

    esp_err_t ret = send(id, data, data_len, false, 20);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send CAN message (ID=0x%03X): %s", id,
                 esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t CTW::receive_can_message(uint32_t* id,
                                   uint8_t* data,
                                   uint8_t* data_len,
                                   int timeout_ms) {
    bool is_extended;

    esp_err_t ret = receive(id, data, data_len, &is_extended, timeout_ms);
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Failed to receive CAN message: %s",
                 esp_err_to_name(ret));
    }

    return ret;
}

void CTW::swap_endian(void* data, size_t length) {
    uint8_t* bytes = (uint8_t*)data;
    for (size_t i = 0; i < length / 2; i++) {
        uint8_t temp = bytes[i];
        bytes[i] = bytes[length - 1 - i];
        bytes[length - 1 - i] = temp;
    }
}

const char* CTW::get_axis_state_string(AxisState state) {
    switch (state) {
        case AXIS_STATE_UNDEFINED:
            return "Undefined";
        case AXIS_STATE_IDLE:
            return "Idle";
        case AXIS_STARTUP_SEQUENCE:
            return "Startup Sequence";
        case AXIS_FULL_CALIBRATION_SEQUENCE:
            return "Full Calibration";
        case AXIS_MOTOR_CALIBRATION:
            return "Motor Calibration";
        case AXIS_SENSORLESS_CONTROL:
            return "Sensorless Control";
        case AXIS_ENCODER_INDEX_SEARCH:
            return "Encoder Index Search";
        case AXIS_ENCODER_OFFSET_CALIBRATION:
            return "Encoder Offset Calibration";
        case AXIS_CLOSED_LOOP_CONTROL:
            return "Closed Loop Control";
        case AXIS_LOCKIN_SPIN:
            return "Lock-in Spin";
        case AXIS_ENCODER_DIR_FIND:
            return "Encoder Dir Find";
        case AXIS_HOMING:
            return "Homing";
        case AXIS_ENCODER_HALL_POLARITY_CALIBRATION:
            return "Hall Polarity Calibration";
        case AXIS_ENCODER_HALL_PHASE_CALIBRATION:
            return "Hall Phase Calibration";
        default:
            return "Unknown";
    }
}

bool CTW::is_initialized() {
    return initialized_;
}

bool CTW::is_started() {
    return started_;
}

esp_err_t CTW::start() {
    if (!initialized_) {
        ESP_LOGE(TAG, "CTW not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (started_) {
        ESP_LOGW(TAG, "CTW already started");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting TWAI driver");
    esp_err_t ret = twai_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }

    started_ = true;
    ESP_LOGI(TAG, "TWAI driver started successfully");
    return ESP_OK;
}

esp_err_t CTW::stop() {
    if (!initialized_ || !started_) {
        ESP_LOGW(TAG, "CTW not started");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping TWAI driver");
    esp_err_t ret = twai_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }

    started_ = false;
    ESP_LOGI(TAG, "TWAI driver stopped successfully");
    return ESP_OK;
}

esp_err_t CTW:: send(uint32_t id, const uint8_t *data, uint8_t data_len,
                    bool is_extended, int timeout_ms) {
    if (!initialized_ || !started_) {
        ESP_LOGE(TAG, "CTW not initialized or not started");
        return ESP_ERR_INVALID_STATE;
    }

    if (data_len > 8) {
        ESP_LOGE(TAG, "Data length too long: %d (max 8)", data_len);
        return ESP_ERR_INVALID_ARG;
    }

    // 准备发送帧
    twai_message_t tx_msg;
    tx_msg.identifier = id;
    tx_msg.flags = is_extended ? TWAI_MSG_FLAG_EXTD : TWAI_MSG_FLAG_NONE;
    tx_msg.data_length_code = data_len;

    // 复制数据
    for (int i = 0; i < data_len; i++) {
        tx_msg.data[i] = data[i];
    }

    // 发送消息
    esp_err_t ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(timeout_ms));
    if (ret != ESP_OK) {
        return ret;
    }

    // 记录发送的帧大小
    uint32_t frame_bits = calculate_frame_bits(data_len, is_extended);
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_bits_sent_ += frame_bits;
    }

    // 检查并发布占用率事件（如果超过1秒）
    check_and_publish_usage();

    ESP_LOGD(TAG, "CAN message sent: ID=0x%lx, Len=%d", id, data_len);
    return ESP_OK;
}

esp_err_t CTW::receive(uint32_t *id, uint8_t *data, uint8_t *data_len,
                       bool *is_extended, int timeout_ms) {
    if (!initialized_ || !started_) {
        ESP_LOGE(TAG, "CTW not initialized or not started");
        return ESP_ERR_INVALID_STATE;
    }

    // 接收消息
    twai_message_t rx_msg;
    esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(timeout_ms));
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Failed to receive CAN message: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    // 填充输出参数
    *id = rx_msg.identifier;
    *data_len = rx_msg.data_length_code;
    *is_extended = rx_msg.extd;

    // 复制数据
    memcpy(data, rx_msg.data, *data_len);

    // 记录接收的帧大小
    uint32_t frame_bits = calculate_frame_bits(*data_len, *is_extended);
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_bits_received_ += frame_bits;
    }

    // 检查并发布占用率事件（如果超过1秒）
    check_and_publish_usage();

    ESP_LOGD(TAG, "CAN message received: ID=0x%lx, Len=%d", *id, *data_len);
    return ESP_OK;
}

uint32_t CTW::calculate_message_transmission_time(uint32_t bitrate,
                                                   uint8_t data_len,
                                                   bool is_extended) {
    // CAN帧位数计算公式：
    // 帧位数 = 1(起始位) + 11/29(ID位数) + 1(RTR位) + 1(IDE位) + 6(控制位) +
    //           8*data_len(数据位) + 2(CRC分隔位) + 16(CRC位) + 2(ACK位) + 7(EOF)

    // 标准帧或扩展帧的位数
    uint32_t header_bits = is_extended ? 58 : 32; // 包含ID、控制等位
    uint32_t data_bits = 8 * data_len;
    uint32_t footer_bits = 25; // CRC、ACK、EOF等
    uint32_t stuff_bits =
        (header_bits + data_bits + footer_bits) / 5; // 位填充大约20%

    uint32_t total_bits = header_bits + data_bits + footer_bits + stuff_bits;
    uint32_t time_us = (total_bits * 1000000) / bitrate;

    return time_us;
}

uint32_t CTW::calculate_frame_bits(uint8_t data_len, bool is_extended) {
    // CAN帧位数计算公式（更精确的版本）：
    // 标准帧：1(SOF) + 11(ID) + 1(RTR) + 1(IDE) + 1(r0) + 4(DLC) + 
    //         8*data_len(数据) + 16(CRC) + 1(CRC分隔) + 1(ACK) + 1(ACK分隔) + 7(EOF) + 3(IFS)
    // 扩展帧：1(SOF) + 11(ID) + 1(SRR) + 1(IDE) + 18(扩展ID) + 1(RTR) + 1(r0) + 4(DLC) +
    //         8*data_len(数据) + 16(CRC) + 1(CRC分隔) + 1(ACK) + 1(ACK分隔) + 7(EOF) + 3(IFS)
    
    // 基础帧结构位数（不包括位填充）
    uint32_t base_bits;
    if (is_extended) {
        // 扩展帧：SOF(1) + 仲裁域(32) + 控制域(6) + 数据域(8*data_len) + CRC域(17) + ACK域(2) + EOF(7) + IFS(3)
        base_bits = 1 + 32 + 6 + (8 * data_len) + 17 + 2 + 7 + 3;
    } else {
        // 标准帧：SOF(1) + 仲裁域(12) + 控制域(6) + 数据域(8*data_len) + CRC域(17) + ACK域(2) + EOF(7) + IFS(3)
        base_bits = 1 + 12 + 6 + (8 * data_len) + 17 + 2 + 7 + 3;
    }
    
    // 位填充：每5个连续相同位插入1个填充位
    // 简化计算：大约每5位需要1位填充，即总位数约为 base_bits * 1.2
    // 更精确：仲裁域和控制域大约需要 (仲裁域+控制域) / 5 的填充位
    uint32_t arbitration_control_bits = is_extended ? 38 : 18;
    uint32_t data_crc_bits = (8 * data_len) + 17;
    
    // 位填充估算：仲裁域和控制域填充 + 数据域和CRC域填充
    uint32_t stuff_bits = (arbitration_control_bits / 5) + (data_crc_bits / 5);
    
    uint32_t total_bits = base_bits + stuff_bits;
    
    return total_bits;
}

void CTW::check_and_publish_usage() {
    int64_t current_time = esp_timer_get_time();
    int64_t elapsed_time = 0;
    uint64_t bits_sent = 0;
    uint64_t bits_received = 0;
    uint32_t bitrate = 0;
    
    // 检查是否超过1秒
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        elapsed_time = current_time - last_stats_time_;
        
        // 如果还没超过1秒，直接返回
        if (elapsed_time < 1000000) {  // 1秒 = 1000000微秒
            return;
        }
        
        // 获取并重置统计计数器
        bits_sent = total_bits_sent_;
        bits_received = total_bits_received_;
        total_bits_sent_ = 0;
        total_bits_received_ = 0;
        bitrate = current_bitrate_;
        last_stats_time_ = current_time;
    }
    
    // 计算总位数
    uint64_t total_bits = bits_sent + bits_received;
    
    // 计算占用率：总位数 / 波特率 * 100%
    float utilization_percent = 0.0f;
    if (bitrate > 0) {
        utilization_percent = ((float)total_bits / (float)bitrate) * 100.0f;
    }
    
    // 准备事件数据
    ctw_can_usage_event_data_t event_data = {
        .bus_utilization_percent = utilization_percent,
        .total_bits_sent = bits_sent,
        .total_bits_received = bits_received,
        .bitrate = bitrate
    };
    
    // 发送事件到全局默认事件循环
    esp_err_t ret = esp_event_post(CTW_CAN_USAGE_EVENT, 
                                   CTW_CAN_USAGE_EVENT_BUS_UTILIZATION,
                                   &event_data, 
                                   sizeof(event_data), 
                                   pdMS_TO_TICKS(100));
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post CAN usage event: %s", esp_err_to_name(ret));
    }
}
