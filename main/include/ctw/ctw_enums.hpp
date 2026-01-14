#pragma once

/**
 * @file ctw_enums.hpp
 * @brief CTW 枚举类型定义
 * 
 * 定义 CTW 协议库使用的所有枚举类型
 * 这些枚举类型在全局命名空间中，供 ctw.hpp 中的 CTW 类使用
 */

/**
 * @brief Axis 状态（兼容 ODrive 0.5.14）
 */
enum AxisState {
    AXIS_STATE_UNDEFINED = 0,
    AXIS_STATE_IDLE = 1,
    AXIS_STARTUP_SEQUENCE = 2,
    AXIS_FULL_CALIBRATION_SEQUENCE = 3,
    AXIS_MOTOR_CALIBRATION = 4,
    AXIS_SENSORLESS_CONTROL = 5,
    AXIS_ENCODER_INDEX_SEARCH = 6,
    AXIS_ENCODER_OFFSET_CALIBRATION = 7,
    AXIS_CLOSED_LOOP_CONTROL = 8,
    AXIS_LOCKIN_SPIN = 9,
    AXIS_ENCODER_DIR_FIND = 10,
    AXIS_HOMING = 11,
    AXIS_ENCODER_HALL_POLARITY_CALIBRATION = 12,
    AXIS_ENCODER_HALL_PHASE_CALIBRATION = 13
};

/**
 * @brief ?????
 */
enum ControllerMode {
    CTRL_MODE_VOLTAGE = 0,
    CTRL_MODE_CURRENT = 1,
    CTRL_MODE_VELOCITY = 2,
    CTRL_MODE_POSITION = 3,
    CTRL_MODE_MISMATCH = 4
};

/**
 * @brief ????
 */
enum InputMode {
    INPUT_MODE_INACTIVE = 0,
    INPUT_MODE_PASSTHROUGH = 1,
    INPUT_MODE_VEL_RAMP = 2,
    INPUT_MODE_POS_FILTER = 3,
    INPUT_MODE_MIXED = 4,
    INPUT_MODE_TRAP_TRAJ = 5,
    INPUT_MODE_TORQUE_RAMP = 6,
    INPUT_MODE_MIRROR = 7
};

/**
 * @brief ODrive 0.6.0 Endpoint ID ??
 *
 * ??? Endpoint ID ????? ODrive ?? 0.6.0
 * ?? ID ?????? endpoints_0.6.0_MW.json
 */
enum EndpointID {
    // ========== ????? ==========
    EID_ERROR = 1,                          // uint8, rw
    EID_VBUS_VOLTAGE = 2,                   // float, r
    EID_IBUS = 3,                           // float, r
    EID_IBUS_REPORT_FILTER_K = 4,           // float, rw
    EID_SERIAL_NUMBER = 5,                  // uint64, r
    EID_HW_VERSION_MAJOR = 6,               // uint8, r
    EID_HW_VERSION_MINOR = 7,               // uint8, r
    EID_HW_VERSION_VARIANT = 8,             // uint8, r
    EID_FW_VERSION_MAJOR = 9,               // uint8, r
    EID_FW_VERSION_MINOR = 10,              // uint8, r
    EID_FW_VERSION_REVISION = 11,           // uint8, r
    EID_FW_VERSION_UNRELEASED = 12,         // uint8, r
    EID_BRAKE_RESISTOR_ARMED = 13,          // bool, r
    EID_BRAKE_RESISTOR_SATURATED = 14,      // bool, r
    EID_BRAKE_RESISTOR_CURRENT = 15,        // float, r
    EID_N_EVT_SAMPLING = 16,                // uint32, r
    EID_N_EVT_CONTROL_LOOP = 17,            // uint32, r
    EID_TASK_TIMERS_ARMED = 18,             // bool, rw

    // ========== ???? ==========
    EID_CONFIG_ENABLE_SWD = 78,             // bool, rw
    EID_CONFIG_ENABLE_UART_A = 79,          // bool, rw
    EID_CONFIG_ENABLE_UART_B = 80,          // bool, rw
    EID_CONFIG_ENABLE_UART_C = 81,          // bool, rw
    EID_CONFIG_UART_A_BAUDRATE = 82,        // uint32, rw
    EID_CONFIG_UART_B_BAUDRATE = 83,        // uint32, rw
    EID_CONFIG_UART_C_BAUDRATE = 84,        // uint32, rw
    EID_CONFIG_ENABLE_CAN_A = 85,           // bool, rw
    EID_CONFIG_COMM_INTF_MUX = 86,          // uint8, rw
    EID_CONFIG_ENABLE_I2C_A = 87,           // bool, rw
    EID_CONFIG_USB_CDC_PROTOCOL = 88,       // uint8, rw
    EID_CONFIG_UART0_PROTOCOL = 89,         // uint8, rw
    EID_CONFIG_UART1_PROTOCOL = 90,         // uint8, rw
    EID_CONFIG_UART2_PROTOCOL = 91,         // uint8, rw
    EID_CONFIG_MAX_REGEN_CURRENT = 92,      // float, rw
    EID_CONFIG_BRAKE_RESISTANCE = 93,       // float, rw
    EID_CONFIG_ENABLE_BRAKE_RESISTOR = 94,  // bool, rw
    EID_CONFIG_DC_BUS_UNDERVOLTAGE_TRIP_LEVEL = 95,    // float, rw
    EID_CONFIG_DC_BUS_OVERVOLTAGE_TRIP_LEVEL = 96,     // float, rw
    EID_CONFIG_ENABLE_DC_BUS_OVERVOLTAGE_RAMP = 97,    // bool, rw
    EID_CONFIG_DC_BUS_OVERVOLTAGE_RAMP_START = 98,     // float, rw
    EID_CONFIG_DC_BUS_OVERVOLTAGE_RAMP_END = 99,       // float, rw
    EID_CONFIG_DC_MAX_POSITIVE_CURRENT = 100,          // float, rw
    EID_CONFIG_DC_MAX_NEGATIVE_CURRENT = 101,          // float, rw
    EID_CONFIG_ERROR_GPIO_PIN = 102,                   // uint32, rw

    // ========== CAN ?? ==========
    EID_CAN_ERROR = 68,                     // uint8, rw
    EID_CAN_CONFIG_BAUD_RATE = 69,          // uint32, rw
    EID_CAN_CONFIG_PROTOCOL = 70,           // uint8, rw
    EID_CAN_CONFIG_R120_GPIO_NUM = 71,      // uint16, rw
    EID_CAN_CONFIG_ENABLE_R120 = 72,        // bool, rw
    EID_CAN_CONFIG_BREAK_TIMEOUT = 73,      // uint16, rw
    EID_CAN_CONFIG_AUTO_BUS_OFF = 74,       // bool, rw
    EID_CAN_CONFIG_AUTO_RETRANSMISSION = 75, // bool, rw

    // ========== Axis0 ???? ==========
    EID_AXIS0_ERROR = 137,                  // uint32, rw
    EID_AXIS0_STEP_DIR_ACTIVE = 138,        // bool, r
    EID_AXIS0_LAST_DRV_FAULT = 139,         // uint32, r
    EID_AXIS0_STEPS = 140,                  // int64, r
    EID_AXIS0_CURRENT_STATE = 141,          // uint8, r
    EID_AXIS0_REQUESTED_STATE = 142,        // uint8, rw
    EID_AXIS0_IS_HOMED = 143,               // bool, rw

    // ========== Axis0 ?? ==========
    EID_AXIS0_CONFIG_STARTUP_MOTOR_CALIBRATION = 144,       // bool, rw
    EID_AXIS0_CONFIG_STARTUP_ENCODER_INDEX_SEARCH = 145,    // bool, rw
    EID_AXIS0_CONFIG_STARTUP_ENCODER_OFFSET_CALIBRATION = 146, // bool, rw
    EID_AXIS0_CONFIG_STARTUP_CLOSED_LOOP_CONTROL = 147,     // bool, rw
    EID_AXIS0_CONFIG_STARTUP_HOMING = 148,                  // bool, rw
    EID_AXIS0_CONFIG_ENABLE_STEP_DIR = 149,                 // bool, rw
    EID_AXIS0_CONFIG_STEP_DIR_ALWAYS_ON = 150,              // bool, rw
    EID_AXIS0_CONFIG_ENABLE_SENSORLESS_MODE = 151,          // bool, rw
    EID_AXIS0_CONFIG_WATCHDOG_TIMEOUT = 152,                // float, rw
    EID_AXIS0_CONFIG_ENABLE_WATCHDOG = 153,                 // bool, rw
    EID_AXIS0_CONFIG_STEP_GPIO_PIN = 154,                   // uint16, rw
    EID_AXIS0_CONFIG_DIR_GPIO_PIN = 155,                    // uint16, rw

    // ========== Axis0 ?? - ?? Lockin ==========
    EID_AXIS0_CONFIG_CALIB_LOCKIN_CURRENT = 156,            // float, rw
    EID_AXIS0_CONFIG_CALIB_LOCKIN_RAMP_TIME = 157,          // float, rw
    EID_AXIS0_CONFIG_CALIB_LOCKIN_RAMP_DISTANCE = 158,      // float, rw
    EID_AXIS0_CONFIG_CALIB_LOCKIN_ACCEL = 159,              // float, rw
    EID_AXIS0_CONFIG_CALIB_LOCKIN_VEL = 160,                // float, rw

    // ========== Axis0 ?? - Sensorless Ramp ==========
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_CURRENT = 161,         // float, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_RAMP_TIME = 162,       // float, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_RAMP_DISTANCE = 163,   // float, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_ACCEL = 164,           // float, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_VEL = 165,             // float, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_FINISH_DISTANCE = 166, // float, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_FINISH_ON_VEL = 167,   // bool, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_FINISH_ON_DISTANCE = 168, // bool, rw
    EID_AXIS0_CONFIG_SENSORLESS_RAMP_FINISH_ON_ENC_IDX = 169,   // bool, rw

    // ========== Axis0 ?? - General Lockin ==========
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_CURRENT = 170,          // float, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_RAMP_TIME = 171,        // float, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_RAMP_DISTANCE = 172,    // float, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_ACCEL = 173,            // float, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_VEL = 174,              // float, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_FINISH_DISTANCE = 175,  // float, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_FINISH_ON_VEL = 176,    // bool, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_FINISH_ON_DISTANCE = 177, // bool, rw
    EID_AXIS0_CONFIG_GENERAL_LOCKIN_FINISH_ON_ENC_IDX = 178, // bool, rw

    // ========== Axis0 CAN ?? ==========
    EID_AXIS0_CONFIG_CAN_NODE_ID = 179,          // uint32, rw
    EID_AXIS0_CONFIG_CAN_IS_EXTENDED = 180,      // bool, rw
    EID_AXIS0_CONFIG_CAN_HEARTBEAT_RATE_MS = 181, // uint32, rw
    EID_AXIS0_CONFIG_CAN_ENCODER_RATE_MS = 182,  // uint32, rw
    EID_AXIS0_CONFIG_CAN_MOTOR_ERROR_RATE_MS = 183, // uint32, rw
    EID_AXIS0_CONFIG_CAN_ENCODER_ERROR_RATE_MS = 184, // uint32, rw
    EID_AXIS0_CONFIG_CAN_CONTROLLER_ERROR_RATE_MS = 185, // uint32, rw
    EID_AXIS0_CONFIG_CAN_SENSORLESS_ERROR_RATE_MS = 186, // uint32, rw
    EID_AXIS0_CONFIG_CAN_ENCODER_COUNT_RATE_MS = 187, // uint32, rw
    EID_AXIS0_CONFIG_CAN_IQ_RATE_MS = 188,         // uint32, rw
    EID_AXIS0_CONFIG_CAN_SENSORLESS_RATE_MS = 189,  // uint32, rw
    EID_AXIS0_CONFIG_CAN_BUS_VI_RATE_MS = 190,      // uint32, rw

    // ========== Axis0 ???? ==========
    EID_AXIS0_MOTOR_LAST_ERROR_TIME = 191,         // float, rw
    EID_AXIS0_MOTOR_ERROR = 192,                   // uint64, rw
    EID_AXIS0_MOTOR_IS_ARMED = 193,                // bool, r
    EID_AXIS0_MOTOR_IS_CALIBRATED = 194,           // bool, r
    EID_AXIS0_MOTOR_CURRENT_MEAS_PH_A = 195,       // float, r
    EID_AXIS0_MOTOR_CURRENT_MEAS_PH_B = 196,       // float, r
    EID_AXIS0_MOTOR_CURRENT_MEAS_PH_C = 197,       // float, r
    EID_AXIS0_MOTOR_DC_CALIB_PH_A = 198,           // float, rw
    EID_AXIS0_MOTOR_DC_CALIB_PH_B = 199,           // float, rw
    EID_AXIS0_MOTOR_DC_CALIB_PH_C = 200,           // float, rw
    EID_AXIS0_MOTOR_I_BUS = 201,                   // float, r
    EID_AXIS0_MOTOR_PHASE_CURRENT_REV_GAIN = 202,  // float, rw
    EID_AXIS0_MOTOR_EFFECTIVE_CURRENT_LIM = 203,   // float, r
    EID_AXIS0_MOTOR_MAX_ALLOWED_CURRENT = 204,     // float, r
    EID_AXIS0_MOTOR_MAX_DC_CALIB = 205,            // float, r

    // ========== Axis0 ?? FET ????? ==========
    EID_AXIS0_MOTOR_FET_THERMISTOR_TEMPERATURE = 206,               // float, r
    EID_AXIS0_MOTOR_FET_THERMISTOR_CONFIG_TEMP_LIMIT_LOWER = 207,   // float, rw
    EID_AXIS0_MOTOR_FET_THERMISTOR_CONFIG_TEMP_LIMIT_UPPER = 208,   // float, rw
    EID_AXIS0_MOTOR_FET_THERMISTOR_CONFIG_ENABLED = 209,            // bool, rw

    // ========== Axis0 ??????? ==========
    EID_AXIS0_MOTOR_THERMISTOR_TEMPERATURE = 210,                   // float, r
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_GPIO_PIN = 211,               // uint16, rw
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_POLY_COEFFICIENT_0 = 212,     // float, rw
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_POLY_COEFFICIENT_1 = 213,     // float, rw
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_POLY_COEFFICIENT_2 = 214,     // float, rw
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_POLY_COEFFICIENT_3 = 215,     // float, rw
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_TEMP_LIMIT_LOWER = 216,       // float, rw
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_TEMP_LIMIT_UPPER = 217,       // float, rw
    EID_AXIS0_MOTOR_THERMISTOR_CONFIG_ENABLED = 218,                // bool, rw

    // ========== Axis0 ???? ==========
    EID_AXIS0_MOTOR_CURRENT_CONTROL_P_GAIN = 219,                   // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_I_GAIN = 220,                   // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_I_MEASURED_REPORT_FILTER_K = 221, // float, rw
    EID_AXIS0_MOTOR_CURRENT_CONTROL_ID_SETPOINT = 222,              // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_IQ_SETPOINT = 223,              // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_VD_SETPOINT = 224,              // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_VQ_SETPOINT = 225,              // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_PHASE = 226,                    // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_PHASE_VEL = 227,                // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_IALPHA_MEASURED = 228,          // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_IBETA_MEASURED = 229,           // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_ID_MEASURED = 230,              // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_IQ_MEASURED = 231,              // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_POWER = 232,                    // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_V_CURRENT_CONTROL_INTEGRAL_D = 233, // float, rw
    EID_AXIS0_MOTOR_CURRENT_CONTROL_V_CURRENT_CONTROL_INTEGRAL_Q = 234, // float, rw
    EID_AXIS0_MOTOR_CURRENT_CONTROL_FINAL_V_ALPHA = 235,            // float, r
    EID_AXIS0_MOTOR_CURRENT_CONTROL_FINAL_V_BETA = 236,             // float, r

    // ========== Axis0 ?????? ==========
    EID_AXIS0_MOTOR_N_EVT_CURRENT_MEASUREMENT = 237,    // uint32, r
    EID_AXIS0_MOTOR_N_EVT_PWM_UPDATE = 238,             // uint32, r

    // ========== Axis0 ???? ==========
    EID_AXIS0_MOTOR_CONFIG_PRE_CALIBRATED = 239,        // bool, rw
    EID_AXIS0_MOTOR_CONFIG_POLE_PAIRS = 240,            // int32, rw
    EID_AXIS0_MOTOR_CONFIG_GEAR_RATIO = 241,            // float, rw
    EID_AXIS0_MOTOR_CONFIG_CALIBRATION_CURRENT = 242,   // float, rw
    EID_AXIS0_MOTOR_CONFIG_RESISTANCE_CALIB_MAX_VOLTAGE = 243, // float, rw
    EID_AXIS0_MOTOR_CONFIG_PHASE_INDUCTANCE = 244,      // float, rw
    EID_AXIS0_MOTOR_CONFIG_PHASE_RESISTANCE = 245,      // float, rw
    EID_AXIS0_MOTOR_CONFIG_TORQUE_CONSTANT = 246,       // float, rw
    EID_AXIS0_MOTOR_CONFIG_MOTOR_TYPE = 247,            // uint8, rw
    EID_AXIS0_MOTOR_CONFIG_CURRENT_LIM = 248,           // float, rw
    EID_AXIS0_MOTOR_CONFIG_CURRENT_LIM_MARGIN = 249,    // float, rw
    EID_AXIS0_MOTOR_CONFIG_TORQUE_LIM = 250,            // float, rw
    EID_AXIS0_MOTOR_CONFIG_INVERTER_TEMP_LIMIT_LOWER = 251, // float, rw
    EID_AXIS0_MOTOR_CONFIG_INVERTER_TEMP_LIMIT_UPPER = 252, // float, rw
    EID_AXIS0_MOTOR_CONFIG_REQUESTED_CURRENT_RANGE = 253, // float, rw
    EID_AXIS0_MOTOR_CONFIG_CURRENT_CONTROL_BANDWIDTH = 254, // float, rw
    EID_AXIS0_MOTOR_CONFIG_ACIM_GAIN_MIN_FLUX = 255,    // float, rw
    EID_AXIS0_MOTOR_CONFIG_ACIM_AUTOFLUX_MIN_ID = 256,  // float, rw
    EID_AXIS0_MOTOR_CONFIG_ACIM_AUTOFLUX_ENABLE = 257,  // bool, rw
    EID_AXIS0_MOTOR_CONFIG_ACIM_AUTOFLUX_ATTACK_GAIN = 258, // float, rw
    EID_AXIS0_MOTOR_CONFIG_ACIM_AUTOFLUX_DECAY_GAIN = 259,    // float, rw
    EID_AXIS0_MOTOR_CONFIG_R_WL_FF_ENABLE = 260,         // bool, rw
    EID_AXIS0_MOTOR_CONFIG_BEMF_FF_ENABLE = 261,         // bool, rw
    EID_AXIS0_MOTOR_CONFIG_I_BUS_HARD_MIN = 262,         // float, rw
    EID_AXIS0_MOTOR_CONFIG_I_BUS_HARD_MAX = 263,         // float, rw
    EID_AXIS0_MOTOR_CONFIG_I_LEAK_MAX = 264,             // float, rw
    EID_AXIS0_MOTOR_CONFIG_DC_CALIB_TAU = 265,           // float, rw

    // ========== Axis0 ??? ==========
    EID_AXIS0_CONTROLLER_ERROR = 266,                    // uint8, rw
    EID_AXIS0_CONTROLLER_LAST_ERROR_TIME = 267,          // float, rw
    EID_AXIS0_CONTROLLER_INPUT_POS = 268,                // float, rw
    EID_AXIS0_CONTROLLER_INPUT_VEL = 269,                // float, rw
    EID_AXIS0_CONTROLLER_INPUT_TORQUE = 270,             // float, rw
    EID_AXIS0_CONTROLLER_INPUT_MIT_KP = 271,             // float, rw
    EID_AXIS0_CONTROLLER_INPUT_MIT_KD = 272,             // float, rw
    EID_AXIS0_CONTROLLER_POS_SETPOINT = 273,             // float, r
    EID_AXIS0_CONTROLLER_VEL_SETPOINT = 274,             // float, r
    EID_AXIS0_CONTROLLER_TORQUE_SETPOINT = 275,          // float, r
    EID_AXIS0_CONTROLLER_TRAJECTORY_DONE = 276,          // bool, r
    EID_AXIS0_CONTROLLER_VEL_INTEGRATOR_TORQUE = 277,    // float, rw
    EID_AXIS0_CONTROLLER_ANTICOGGING_VALID = 278,        // bool, rw
    EID_AXIS0_CONTROLLER_AUTOTUNING_PHASE = 279,         // float, rw

    // ========== Axis0 ????? ==========
    EID_AXIS0_CONTROLLER_CONFIG_GAIN_SCHEDULING_WIDTH = 280,   // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_ENABLE_VEL_LIMIT = 281,        // bool, rw
    EID_AXIS0_CONTROLLER_CONFIG_ENABLE_TORQUE_MODE_VEL_LIMIT = 282, // bool, rw
    EID_AXIS0_CONTROLLER_CONFIG_ENABLE_GAIN_SCHEDULING = 283,  // bool, rw
    EID_AXIS0_CONTROLLER_CONFIG_ENABLE_OVERSPEED_ERROR = 284,  // bool, rw
    EID_AXIS0_CONTROLLER_CONFIG_CONTROL_MODE = 285,           // uint8, rw
    EID_AXIS0_CONTROLLER_CONFIG_INPUT_MODE = 286,             // uint8, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_LOOP_PRESCALER = 287,     // uint16, rw
    EID_AXIS0_CONTROLLER_CONFIG_POS_LOOP_PRESCALER = 288,     // uint16, rw
    EID_AXIS0_CONTROLLER_CONFIG_POS_GAIN = 289,               // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_POS_INTEGRATOR_GAIN = 290,    // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_POS_INTEGRATOR_LIMIT = 291,   // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_POS_DIFF_GAIN = 292,          // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_GAIN = 293,               // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_INTEGRATOR_GAIN = 294,    // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_INTEGRATOR_LIMIT = 295,   // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_DIFF_GAIN = 296,          // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_LIMIT = 297,              // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_LIMIT_TOLERANCE = 298,    // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_VEL_RAMP_RATE = 299,          // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_TORQUE_RAMP_RATE = 300,       // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_CIRCULAR_SETPOINTS = 301,     // bool, rw
    EID_AXIS0_CONTROLLER_CONFIG_CIRCULAR_SETPOINT_RANGE = 302, // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_STEPS_PER_CIRCULAR_RANGE = 303, // int32, rw
    EID_AXIS0_CONTROLLER_CONFIG_HOMING_SPEED = 304,           // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_INERTIA = 305,                // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_AXIS_TO_MIRROR = 306,         // uint8, rw
    EID_AXIS0_CONTROLLER_CONFIG_MIRROR_RATIO = 307,           // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_TORQUE_MIRROR_RATIO = 308,    // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_LOAD_ENCODER_AXIS = 309,      // uint8, rw
    EID_AXIS0_CONTROLLER_CONFIG_INPUT_FILTER_BANDWIDTH = 310, // float, rw

    // ========== Axis0 ??? Anticogging ?? ==========
    EID_AXIS0_CONTROLLER_CONFIG_ANTICOGGING_INDEX = 311,              // uint32, r
    EID_AXIS0_CONTROLLER_CONFIG_ANTICOGGING_PRE_CALIBRATED = 312,     // bool, rw
    EID_AXIS0_CONTROLLER_CONFIG_ANTICOGGING_CALIB_ANTICOGGING = 313, // bool, r
    EID_AXIS0_CONTROLLER_CONFIG_ANTICOGGING_CALIB_POS_THRESHOLD = 314, // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_ANTICOGGING_CALIB_VEL_THRESHOLD = 315,   // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_ANTICOGGING_COGGING_RATIO = 316,    // float, r
    EID_AXIS0_CONTROLLER_CONFIG_ANTICOGGING_ANTICOGGING_ENABLED = 317, // bool, rw

    // ========== Axis0 ??? Spinout ???? ==========
    EID_AXIS0_CONTROLLER_CONFIG_MECHANICAL_POWER_BANDWIDTH = 318,    // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_ELECTRICAL_POWER_BANDWIDTH = 319,    // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_SPINOUT_MECHANICAL_POWER_THRESHOLD = 320, // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_SPINOUT_ELECTRICAL_POWER_THRESHOLD = 321, // float, rw

    // ========== Axis0 ??? MIT ???? ==========
    EID_AXIS0_CONTROLLER_CONFIG_MIT_MAX_POS = 322,             // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_MIT_MAX_VEL = 323,             // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_MIT_MAX_TORQUE = 324,          // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_MIT_MAX_KP = 325,              // float, rw
    EID_AXIS0_CONTROLLER_CONFIG_MIT_MAX_KD = 326,              // float, rw

    // ========== Axis0 ??? Autotuning ==========
    EID_AXIS0_CONTROLLER_AUTOTUNING_FREQUENCY = 327,           // float, rw
    EID_AXIS0_CONTROLLER_AUTOTUNING_POS_AMPLITUDE = 328,       // float, rw
    EID_AXIS0_CONTROLLER_AUTOTUNING_VEL_AMPLITUDE = 329,       // float, rw
    EID_AXIS0_CONTROLLER_AUTOTUNING_TORQUE_AMPLITUDE = 330,    // float, rw
    EID_AXIS0_CONTROLLER_MECHANICAL_POWER = 331,               // float, r
    EID_AXIS0_CONTROLLER_ELECTRICAL_POWER = 332,               // float, r

    // ========== Axis0 ??? ==========
    EID_AXIS0_ENCODER_ERROR = 342,                   // uint16, rw
    EID_AXIS0_ENCODER_IS_READY = 343,                // bool, r
    EID_AXIS0_ENCODER_INDEX_FOUND = 344,             // bool, r
    EID_AXIS0_ENCODER_SHADOW_COUNT = 345,            // int32, r
    EID_AXIS0_ENCODER_COUNT_IN_CPR = 346,            // int32, r
    EID_AXIS0_ENCODER_INTERPOLATION = 347,           // float, r
    EID_AXIS0_ENCODER_PHASE = 348,                   // float, r
    EID_AXIS0_ENCODER_POS_ESTIMATE = 349,            // float, r
    EID_AXIS0_ENCODER_POS_ESTIMATE_COUNTS = 350,     // float, r
    EID_AXIS0_ENCODER_POS_CIRCULAR = 351,             // float, r
    EID_AXIS0_ENCODER_POS_CPR_COUNTS = 352,          // float, r
    EID_AXIS0_ENCODER_DELTA_POS_CPR_COUNTS = 353,    // float, r
    EID_AXIS0_ENCODER_HALL_STATE = 354,              // uint8, r
    EID_AXIS0_ENCODER_VEL_ESTIMATE = 355,            // float, r
    EID_AXIS0_ENCODER_VEL_ESTIMATE_COUNTS = 356,     // float, r
    EID_AXIS0_ENCODER_CALIB_SCAN_RESPONSE = 357,     // float, r
    EID_AXIS0_ENCODER_POS_ABS = 358,                 // int32, rw
    EID_AXIS0_ENCODER_SPI_ERROR_RATE = 359,          // float, r

    // ========== Axis0 ????? ==========
    EID_AXIS0_ENCODER_CONFIG_MODE = 360,             // uint16, rw
    EID_AXIS0_ENCODER_CONFIG_USE_INDEX = 361,        // bool, rw
    EID_AXIS0_ENCODER_CONFIG_INDEX_OFFSET = 362,     // float, rw
    EID_AXIS0_ENCODER_CONFIG_USE_INDEX_OFFSET = 363, // bool, rw
    EID_AXIS0_ENCODER_CONFIG_FIND_IDX_ON_LOCKIN_ONLY = 364, // bool, rw
    EID_AXIS0_ENCODER_CONFIG_ABS_SPI_CS_GPIO_PIN = 365, // uint16, rw
    EID_AXIS0_ENCODER_CONFIG_CPR = 366,              // int32, rw
    EID_AXIS0_ENCODER_CONFIG_PHASE_OFFSET = 367,     // int32, rw
    EID_AXIS0_ENCODER_CONFIG_PHASE_OFFSET_FLOAT = 368, // float, rw
    EID_AXIS0_ENCODER_CONFIG_DIRECTION = 369,        // int32, rw
    EID_AXIS0_ENCODER_CONFIG_PRE_CALIBRATED = 370,   // bool, rw
    EID_AXIS0_ENCODER_CONFIG_ENABLE_PHASE_INTERPOLATION = 371, // bool, rw
    EID_AXIS0_ENCODER_CONFIG_BANDWIDTH = 372,        // float, rw
    EID_AXIS0_ENCODER_CONFIG_CALIB_RANGE = 373,      // float, rw
    EID_AXIS0_ENCODER_CONFIG_CALIB_SCAN_DISTANCE = 374, // float, rw
    EID_AXIS0_ENCODER_CONFIG_CALIB_SCAN_OMEGA = 375, // float, rw
    EID_AXIS0_ENCODER_CONFIG_IGNORE_ILLEGAL_HALL_STATE = 376, // bool, rw
    EID_AXIS0_ENCODER_CONFIG_HALL_POLARITY = 377,    // uint8, rw
    EID_AXIS0_ENCODER_CONFIG_HALL_POLARITY_CALIBRATED = 378, // bool, rw
    EID_AXIS0_ENCODER_CONFIG_SINCOS_GPIO_PIN_SIN = 379, // uint16, rw
    EID_AXIS0_ENCODER_CONFIG_SINCOS_GPIO_PIN_COS = 380, // uint16, rw
    EID_AXIS0_ENCODER_CONFIG_SEC_ENC_CPR = 381,       // int32, rw

    // ========== Axis0 Sensorless ??? ==========
    EID_AXIS0_SENSORLESS_ESTIMATOR_ERROR = 392,      // uint8, rw
    EID_AXIS0_SENSORLESS_ESTIMATOR_PHASE = 393,      // float, r
    EID_AXIS0_SENSORLESS_ESTIMATOR_PLL_POS = 394,    // float, r
    EID_AXIS0_SENSORLESS_ESTIMATOR_PHASE_VEL = 395,  // float, r
    EID_AXIS0_SENSORLESS_ESTIMATOR_VEL_ESTIMATE = 396, // float, r
    EID_AXIS0_SENSORLESS_ESTIMATOR_CONFIG_OBSERVER_GAIN = 397, // float, rw
    EID_AXIS0_SENSORLESS_ESTIMATOR_CONFIG_PLL_BANDWIDTH = 398, // float, rw
    EID_AXIS0_SENSORLESS_ESTIMATOR_CONFIG_PM_FLUX_LINKAGE = 399, // float, rw

    // ========== Axis0 ?????? ==========
    EID_AXIS0_TRAP_TRAJ_CONFIG_VEL_LIMIT = 400,      // float, rw
    EID_AXIS0_TRAP_TRAJ_CONFIG_ACCEL_LIMIT = 401,    // float, rw
    EID_AXIS0_TRAP_TRAJ_CONFIG_DECEL_LIMIT = 402,    // float, rw

    // ========== Axis0 ?????? ==========
    EID_AXIS0_MIN_ENDSTOP_ENDSTOP_STATE = 403,       // bool, r
    EID_AXIS0_MIN_ENDSTOP_CONFIG_GPIO_NUM = 404,     // uint16, rw
    EID_AXIS0_MIN_ENDSTOP_CONFIG_ENABLED = 405,      // bool, rw
    EID_AXIS0_MIN_ENDSTOP_CONFIG_OFFSET = 406,       // float, rw
    EID_AXIS0_MIN_ENDSTOP_CONFIG_IS_ACTIVE_HIGH = 407, // bool, rw
    EID_AXIS0_MIN_ENDSTOP_CONFIG_DEBOUNCE_MS = 408,  // uint32, rw

    EID_AXIS0_MAX_ENDSTOP_ENDSTOP_STATE = 409,       // bool, r
    EID_AXIS0_MAX_ENDSTOP_CONFIG_GPIO_NUM = 410,     // uint16, rw
    EID_AXIS0_MAX_ENDSTOP_CONFIG_ENABLED = 411,      // bool, rw
    EID_AXIS0_MAX_ENDSTOP_CONFIG_OFFSET = 412,       // float, rw
    EID_AXIS0_MAX_ENDSTOP_CONFIG_IS_ACTIVE_HIGH = 413, // bool, rw
    EID_AXIS0_MAX_ENDSTOP_CONFIG_DEBOUNCE_MS = 414,  // uint32, rw

    // ========== Axis0 ?????? ==========
    EID_AXIS0_MECHANICAL_BRAKE_CONFIG_GPIO_NUM = 415, // uint16, rw
    EID_AXIS0_MECHANICAL_BRAKE_CONFIG_IS_ACTIVE_LOW = 416, // bool, rw

    // ========== ?????? ==========
    EID_SAVE_CONFIGURATION = 478,                 // function, ???
    EID_ERASE_CONFIGURATION = 480,                // function, ???
    EID_REBOOT = 481,                            // function, ???
    EID_ENTER_DFU_MODE = 482,                    // function, ???
    EID_CLEAR_ERRORS = 493                       // function, ???
};
