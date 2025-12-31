#pragma once

#include <driver/twai.h>
#include <esp_err.h>
#include <string>
#include <mutex>

/**
 * @brief MIT协议实现类
 *
 * 基于SteadyWin®GIM电驱的MIT开源CAN协议
 * 支持电机控制、状态查询和异常处理功能
 */
class MIT {
public:
  /**
   * @brief MIT协议命令定义
   */
  enum Command {
    CMD_ESTOP = 0x002,                 // 紧急停止
    CMD_GET_ERROR = 0x003,             // 获取错误
    CMD_SET_AXIS_NODE_ID = 0x006,      // 设置轴节点ID
    CMD_SET_AXIS_STATE = 0x007,        // 设置轴状态
    CMD_MIT_CONTROL = 0x008,           // MIT控制
    CMD_GET_ENCODER_ESTIMATES = 0x009, // 获取编码器估算值
    CMD_SET_CONTROLLER_MODE = 0x00B,   // 设置控制器模式
    CMD_SET_INPUT_POS = 0x00C,         // 设置输入位置
    CMD_SET_INPUT_VEL = 0x00D,         // 设置输入速度
    CMD_SET_INPUT_TORQUE = 0x00E,      // 设置输入力矩
    CMD_SET_LIMITS = 0x00F,            // 设置限制
    CMD_CLEAR_ERRORS = 0x018,          // 清除错误
    CMD_SET_POSITION = 0x019,          // 设置位置 (setpos)
    CMD_SAVE_CONFIGURATION = 0x01F,    // 保存配置

    // 兼容性定义 - 保持向后兼容
    CMD_GET_CLEAR_FAULT = CMD_GET_ERROR,  // 获取/消除异常
    CMD_START_MOTOR = CMD_SET_AXIS_STATE, // 启动电机 (状态=8)
    CMD_STOP_MOTOR = CMD_SET_AXIS_STATE,  // 停止电机 (状态=1)
    CMD_SET_ZERO = CMD_SET_AXIS_STATE,    // 设置零点 (状态=7)
    CMD_DYNAMIC_CONTROL = CMD_MIT_CONTROL // 动态控制
  };

  /**
   * @brief 异常代码定义
   */
  enum FaultCode {
    FAULT_NONE = 0x00000000,                            // 无异常
    FAULT_PHASE_RESISTANCE_OUT_OF_RANGE = 0x00000001,   // 相间电阻超出正常范围
    FAULT_PHASE_INDUCTANCE_OUT_OF_RANGE = 0x00000002,   // 相间电感超出正常范围
    FAULT_CONTROL_DEADLINE_MISSED = 0x00000010,         // FOC频率太高
    FAULT_MODULATION_MAGNITUDE = 0x00000080,            // SVM调制异常
    FAULT_CURRENT_SENSE_SATURATION = 0x00000400,        // 相电流饱和
    FAULT_CURRENT_LIMIT_VIOLATION = 0x00001000,         // 电机电流过大
    FAULT_MOTOR_THERMISTOR_OVER_TEMP = 0x00020000,      // 电机温度过高
    FAULT_FET_THERMISTOR_OVER_TEMP = 0x00040000,        // 驱动器温度过高
    FAULT_TIMER_UPDATE_MISSED = 0x00080000,             // FOC处理不及时
    FAULT_CURRENT_MEASUREMENT_UNAVAILABLE = 0x00100000, // 相电流采样丢失
    FAULT_CONTROLLER_FAILED = 0x00200000,               // 控制异常
    FAULT_I_BUS_OUT_OF_RANGE = 0x00400000,              // 母线电流超限
    FAULT_BRAKE_RESISTOR_DISARMED = 0x00800000,         // 刹车电阻驱动异常
    FAULT_SYSTEM_LEVEL = 0x01000000,                    // 系统级异常
    FAULT_BAD_TIMING = 0x02000000,                      // 相电流采样不及时
    FAULT_UNKNOWN_PHASE_ESTIMATE = 0x04000000,          // 电机位置未知
    FAULT_UNKNOWN_PHASE_VEL = 0x08000000,               // 电机速度未知
    FAULT_UNKNOWN_TORQUE = 0x10000000,                  // 力矩未知
    FAULT_UNKNOWN_CURRENT_COMMAND = 0x20000000,         // 力矩控制未知
    FAULT_UNKNOWN_CURRENT_MEASUREMENT = 0x40000000,     // 电流采样值未知
    FAULT_UNKNOWN_VBUS_VOLTAGE = 0x80000000,            // 电压采样值未知
    FAULT_UNKNOWN_VOLTAGE_COMMAND = 0x100000000,        // 电压控制未知
    FAULT_UNKNOWN_GAINS = 0x200000000,                  // 电流环增益未知
    FAULT_CONTROLLER_INITIALIZING = 0x400000000,        // 控制器初始化异常
    FAULT_UNBALANCED_PHASES = 0x800000000               // 三相不平衡
  };

  /**
   * @brief 轴状态定义
   */
  enum AxisState {
    AXIS_STATE_UNDEFINED = 0, // 未定义状态
    AXIS_STATE_IDLE = 1,      // 空闲状态
    // AXIS_STATE_STARTUP_SEQUENCE = 2,           // 启动序列
    AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3, // 完整校准序列
    AXIS_STATE_MOTOR_CALIBRATION = 4,         // 电机校准
    // AXIS_STATE_ENCODER_INDEX_SEARCH = 6,       // 编码器索引搜索
    AXIS_STATE_ENCODER_OFFSET_CALIBRATION = 7, // 编码器偏移校准（设置零点）
    AXIS_STATE_CLOSED_LOOP_CONTROL = 8,        // 闭环控制
    // AXIS_STATE_LOCKIN_SPIN = 9,          // 锁定旋转
    // AXIS_STATE_ENCODER_DIR_FIND = 10     // 编码器方向查找
  };

  /**
   * @brief 电机控制参数结构体
   */
  struct MotorControl {
    double position; // 位置 (弧度)
    double velocity; // 速度 (弧度/秒)
    double kp;       // 位置增益
    double kd;       // 速度增益
    double torque;   // 力矩 (N.m)

    MotorControl()
        : position(0.0), velocity(0.0), kp(0.0), kd(0.0), torque(0.0) {}
  };

  /**
   * @brief 电机状态反馈结构体
   */
  struct MotorStatus {
    uint8_t can_id;      // CAN ID
    double position;     // 位置 (弧度)
    double velocity;     // 速度 (弧度/秒)
    double torque;       // 力矩 (N.m)
    uint64_t fault_code; // 异常代码

    MotorStatus()
        : can_id(0), position(0.0), velocity(0.0), torque(0.0), fault_code(0) {}
  };
  /**
   * @brief 初始化MIT协议
   * @param tx_pin TWAI TX引脚
   * @param rx_pin TWAI RX引脚
   * @param bitrate 波特率
   * @return esp_err_t 错误码
   */
  static esp_err_t init(int tx_pin = 2, int rx_pin = 3,
                        uint32_t bitrate = 500000);

  /**
   * @brief 获取电机异常
   * @param nodeid 电机节点ID
   * @param fault_code 输出参数：异常代码
   * @return esp_err_t 错误码
   */
  static esp_err_t get_fault(uint8_t nodeid, uint64_t &fault_code);

  /**
   * @brief 清除电机异常
   * @param nodeid 电机节点ID
   * @return esp_err_t 错误码
   */
  static esp_err_t clear_fault(uint8_t nodeid);

  /**
   * @brief 启动电机
   * @param nodeid 电机节点ID
   * @return esp_err_t 错误码
   */
  static esp_err_t start_motor(uint8_t nodeid);

  /**
   * @brief 停止电机
   * @param nodeid 电机节点ID
   * @return esp_err_t 错误码
   */
  static esp_err_t stop_motor(uint8_t nodeid);

  /**
   * @brief 设置电机轴状态
   * @param nodeid 电机节点ID
   * @param state 轴状态
   * @return esp_err_t 错误码
   */
  static esp_err_t set_state(uint8_t nodeid, AxisState state);

  /**
   * @brief 动态控制电机
   * @param nodeid 电机节点ID
   * @param control 控制参数
   * @return esp_err_t 错误码
   */
  static esp_err_t dynamic_control(uint8_t nodeid, const MotorControl &control);

  /**
   * @brief 设置电机位置 (setpos)
   * @param nodeid 电机节点ID
   * @param position 位置值 (float32)
   * @return esp_err_t 错误码
   */
  static esp_err_t setPos(uint8_t nodeid, float position);

  /**
   * @brief 动态控制电机并获取响应
   * @param nodeid 电机节点ID
   * @param control 控制参数
   * @param status 输出参数：电机状态响应
   * @return esp_err_t 错误码
   */
  static esp_err_t dynamic_control_with_response(uint8_t nodeid,
                                                 const MotorControl &control,
                                                 MotorStatus &status);

  /**
   * @brief 接收电机状态反馈
   * @param timeout_ms 超时时间(毫秒)
   * @param status 输出参数：电机状态
   * @return esp_err_t 错误码
   */
  static esp_err_t receive_status(int timeout_ms, MotorStatus &status);

  /**
   * @brief 发送命令并等待响应
   * @param nodeid 电机节点ID
   * @param command 命令
   * @param timeout_ms 超时时间(毫秒)
   * @param response_data 响应数据
   * @param response_len 响应数据长度
   * @return esp_err_t 错误码
   */
  static esp_err_t wait_response(uint8_t nodeid, Command command,
                                 int timeout_ms, uint8_t *response_data,
                                 uint8_t &response_len);

  /**
   * @brief 获取异常代码描述
   * @param fault_code 异常代码
   * @return String 异常描述
   */
  static std::string get_fault_description(uint64_t fault_code);

  /**
   * @brief 大小端翻转函数
   * @param data 数据指针
   * @param length 数据长度（字节数）
   */
  static void swap_endian(void *data, size_t length);

private:
  static bool initialized;
  static std::recursive_mutex mutex;  // 可重入互斥锁，保护静态成员访问

  /**
   * @brief 根据nodeid和cmdid计算CAN ID
   * @param nodeid 节点ID (0-63)
   * @param cmdid 命令ID (0-31)
   * @return uint32_t 计算后的CAN ID
   */
  static uint32_t calculate_can_id(uint8_t nodeid, uint8_t cmdid);

  /**
   * @brief 将double位置值转换为16位int
   * @param position 位置值 (弧度)
   * @return int16_t 转换后的16位整数
   */
  static uint16_t position_to_int(double position);

  /**
   * @brief 将16位int位置值转换为double
   * @param pos_int 16位整数位置值
   * @return double 转换后的位置值 (弧度)
   */
  static double int_to_position(uint16_t pos_int);

  /**
   * @brief 将double速度值转换为12位int
   * @param velocity 速度值 (弧度/秒)
   * @return int16_t 转换后的12位整数
   */
  static int16_t velocity_to_int(double velocity);

  /**
   * @brief 将12位int速度值转换为double
   * @param vel_int 12位整数速度值
   * @return double 转换后的速度值 (弧度/秒)
   */
  static double int_to_velocity(int16_t vel_int);

  /**
   * @brief 将double KP值转换为12位int
   * @param kp KP值
   * @return int16_t 转换后的12位整数
   */
  static int16_t kp_to_int(double kp);

  /**
   * @brief 将12位int KP值转换为double
   * @param kp_int 12位整数KP值
   * @return double 转换后的KP值
   */
  static double int_to_kp(int16_t kp_int);

  /**
   * @brief 将double KD值转换为12位int
   * @param kd KD值
   * @return int16_t 转换后的12位整数
   */
  static int16_t kd_to_int(double kd);

  /**
   * @brief 将12位int KD值转换为double
   * @param kd_int 12位整数KD值
   * @return double 转换后的KD值
   */
  static double int_to_kd(int16_t kd_int);

  /**
   * @brief 将double力矩值转换为12位int
   * @param torque 力矩值 (N.m)
   * @return int16_t 转换后的12位整数
   */
  static int16_t torque_to_int(double torque);

  /**
   * @brief 将12位int力矩值转换为double
   * @param t_int 12位整数力矩值
   * @return double 转换后的力矩值 (N.m)
   */
  static double int_to_torque(int16_t t_int);

  /**
   * @brief 打包动态控制数据
   * @param control 控制参数
   * @param data 输出参数：打包后的数据
   */
  static void pack_dynamic_control_data(const MotorControl &control,
                                        uint8_t data[8]);

  /**
   * @brief 解包状态反馈数据
   * @param data 接收到的数据
   * @param data_len 数据长度
   * @param status 输出参数：解析后的状态
   */
  static void unpack_status_data(const uint8_t *data, uint8_t data_len,
                                 MotorStatus &status);

  /**
   * @brief 解包动态控制响应数据（5字节）
   * @param data 接收到的数据
   * @param data_len 数据长度
   * @param status 输出参数：解析后的状态
   */
  static void unpack_dynamic_control_response(const uint8_t *data,
                                              uint8_t data_len,
                                              MotorStatus &status);
};