#include "setting_config.hpp"
#include "esp_log.h"
#include <sdkconfig.h>

static const char* TAG = "SettingConfig";

// ESP32 C3 开发板配置实现
Setting_Servo getOSRConfig_C3() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 0;
    config.B_SERVO_PIN = 1;
    config.C_SERVO_PIN = 2;
    config.D_SERVO_PIN = 3;
    config.E_SERVO_PIN = 8;
    config.F_SERVO_PIN = 10;
    config.G_SERVO_PIN = 9;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 0.0f;
    
    return config;
}

Setting_Servo getTrRConfig_C3() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 0;
    config.B_SERVO_PIN = 1;
    config.C_SERVO_PIN = 2;
    config.D_SERVO_PIN = 3;
    config.E_SERVO_PIN = 8;
    config.F_SERVO_PIN = 10;
    config.G_SERVO_PIN = 9;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 0.72f;
    config.R2_SCALE = 0.72f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 1.0f;
    
    return config;
}

Setting_Servo getTrRMaxConfig_C3() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 0;
    config.B_SERVO_PIN = 1;
    config.C_SERVO_PIN = 2;
    config.D_SERVO_PIN = 3;
    config.E_SERVO_PIN = 8;
    config.F_SERVO_PIN = 10;
    config.G_SERVO_PIN = 9;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 0.8f;
    config.R2_SCALE = 0.8f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 6.0f;
    
    return config;
}

Setting_Servo getZDTConfig_C3() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 0;
    config.B_SERVO_PIN = 1;
    config.C_SERVO_PIN = 2;
    config.D_SERVO_PIN = 3;
    config.E_SERVO_PIN = 8;
    config.F_SERVO_PIN = 10;
    config.G_SERVO_PIN = 9;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.32f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.02f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 0.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 0.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 2.0f;
    
    return config;
}

Setting_Servo getSR6Config_C3() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 0;
    config.B_SERVO_PIN = 1;
    config.C_SERVO_PIN = 2;
    config.D_SERVO_PIN = 3;
    config.E_SERVO_PIN = 8;
    config.F_SERVO_PIN = 10;
    config.G_SERVO_PIN = 9;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 3.0f;
    
    return config;
}

Setting_Servo getSR6SHALIConfig_C3() {
    Setting_Servo config = Setting_Servo_init_default;

    // 引脚配置
    config.A_SERVO_PIN = 0;
    config.B_SERVO_PIN = 2;
    config.C_SERVO_PIN = 6;
    config.D_SERVO_PIN = 10;
    config.E_SERVO_PIN = 3;
    config.F_SERVO_PIN = 1;
    config.G_SERVO_PIN = 5;

    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 150;
    config.B_SERVO_PWM_FREQ = 150;
    config.C_SERVO_PWM_FREQ = 150;
    config.D_SERVO_PWM_FREQ = 150;
    config.E_SERVO_PWM_FREQ = 150;
    config.F_SERVO_PWM_FREQ = 150;
    config.G_SERVO_PWM_FREQ = 150;

    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;

    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;

    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;

    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = true;
    config.R0_REVERSE = false;
    config.R1_REVERSE = true;
    config.R2_REVERSE = false;

    // 模式配置
    config.MODE = 3.0f;

    return config;
}

Setting_Servo getSR6CANConfig_C3() {
    Setting_Servo config = Setting_Servo_init_default;

    // 引脚配置 (SR6CAN 使用 CAN 总线, 不使用 PWM 引脚, 设置为 -1 表示未使用)
    config.A_SERVO_PIN = -1;
    config.B_SERVO_PIN = -1;
    config.C_SERVO_PIN = -1;
    config.D_SERVO_PIN = -1;
    config.E_SERVO_PIN = -1;
    config.F_SERVO_PIN = -1;
    config.G_SERVO_PIN = -1;

    // PWM频率配置 (SR6CAN 不使用 PWM, 频率设为 0)
    config.A_SERVO_PWM_FREQ = 200;
    config.B_SERVO_PWM_FREQ = 0;
    config.C_SERVO_PWM_FREQ = 0;
    config.D_SERVO_PWM_FREQ = 0;
    config.E_SERVO_PWM_FREQ = 0;
    config.F_SERVO_PWM_FREQ = 0;
    config.G_SERVO_PWM_FREQ = 0;

    // 零点配置 (SR6CAN 通过 CAN 总线控制, 零点值不影响)
    config.A_SERVO_ZERO = 0;
    config.B_SERVO_ZERO = 0;
    config.C_SERVO_ZERO = 0;
    config.D_SERVO_ZERO = 0;
    config.E_SERVO_ZERO = 0;
    config.F_SERVO_ZERO = 0;
    config.G_SERVO_ZERO = 0;

    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;

    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;

    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;

    // 模式配置 (SR6CAN mode = 8)
    config.MODE = 8.0f;

    return config;
}

Setting_Servo getO6Config_C3() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 0;
    config.B_SERVO_PIN = 1;
    config.C_SERVO_PIN = 2;
    config.D_SERVO_PIN = 3;
    config.E_SERVO_PIN = 8;
    config.F_SERVO_PIN = 10;
    config.G_SERVO_PIN = 9;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 9;
    
    return config;
}

// ESP32 开发板配置实现
Setting_Servo getOSRConfig_ESP32() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 15;
    config.B_SERVO_PIN = 13;
    config.C_SERVO_PIN = 4;
    config.D_SERVO_PIN = 27;
    config.E_SERVO_PIN = 26;
    config.F_SERVO_PIN = 25;
    config.G_SERVO_PIN = 12;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 0.0f;
    
    return config;
}

Setting_Servo getTrRConfig_ESP32() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 13;
    config.B_SERVO_PIN = 12;
    config.C_SERVO_PIN = 14;
    config.D_SERVO_PIN = 27;
    config.E_SERVO_PIN = 26;
    config.F_SERVO_PIN = 25;
    config.G_SERVO_PIN = 12;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 0.72f;
    config.R2_SCALE = 0.72f;
    
    // 左右限制配置
    config.L0_LEFT = 0.24f;
    config.L0_RIGHT = 0.58f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 1.0f;
    
    return config;
}

Setting_Servo getTrRMaxConfig_ESP32() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 13;
    config.B_SERVO_PIN = 12;
    config.C_SERVO_PIN = 14;
    config.D_SERVO_PIN = 27;
    config.E_SERVO_PIN = 26;
    config.F_SERVO_PIN = 25;
    config.G_SERVO_PIN = 12;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 6.0f;
    
    return config;
}

Setting_Servo getZDTConfig_ESP32() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 13;
    config.B_SERVO_PIN = 12;
    config.C_SERVO_PIN = 14;
    config.D_SERVO_PIN = 27;
    config.E_SERVO_PIN = 26;
    config.F_SERVO_PIN = 25;
    config.G_SERVO_PIN = 12;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 50;
    config.B_SERVO_PWM_FREQ = 50;
    config.C_SERVO_PWM_FREQ = 50;
    config.D_SERVO_PWM_FREQ = 50;
    config.E_SERVO_PWM_FREQ = 50;
    config.F_SERVO_PWM_FREQ = 50;
    config.G_SERVO_PWM_FREQ = 50;
    
    // 零点配置
    config.A_SERVO_ZERO = 1500;
    config.B_SERVO_ZERO = 1500;
    config.C_SERVO_ZERO = 1500;
    config.D_SERVO_ZERO = 1500;
    config.E_SERVO_ZERO = 1500;
    config.F_SERVO_ZERO = 1500;
    config.G_SERVO_ZERO = 1500;
    
    // 缩放比例配置
    config.L0_SCALE = 1.32f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 0.020f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 0.004f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 0.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 2.0f;
    
    return config;
}

Setting_Servo getSR6PulseConfig_ESP32() {
    Setting_Servo config = Setting_Servo_init_default;
    
    // 引脚配置
    config.A_SERVO_PIN = 13;
    config.B_SERVO_PIN = 12;
    config.C_SERVO_PIN = 14;
    config.D_SERVO_PIN = 27;
    config.E_SERVO_PIN = 26;
    config.F_SERVO_PIN = 25;
    config.G_SERVO_PIN = 12;
    
    // PWM频率配置
    config.A_SERVO_PWM_FREQ = 99;
    config.B_SERVO_PWM_FREQ = 99;
    config.C_SERVO_PWM_FREQ = 99;
    config.D_SERVO_PWM_FREQ = 99;
    config.E_SERVO_PWM_FREQ = 99;
    config.F_SERVO_PWM_FREQ = 99;
    config.G_SERVO_PWM_FREQ = 99;
    
    // 零点配置
    config.A_SERVO_ZERO = 0;
    config.B_SERVO_ZERO = 0;
    config.C_SERVO_ZERO = 0;
    config.D_SERVO_ZERO = 0;
    config.E_SERVO_ZERO = 0;
    config.F_SERVO_ZERO = 0;
    config.G_SERVO_ZERO = 0;
    
    // 缩放比例配置
    config.L0_SCALE = 1.0f;
    config.L1_SCALE = 1.0f;
    config.L2_SCALE = 1.0f;
    config.R0_SCALE = 1.0f;
    config.R1_SCALE = 1.0f;
    config.R2_SCALE = 1.0f;
    
    // 左右限制配置
    config.L0_LEFT = 0.0f;
    config.L0_RIGHT = 1.0f;
    config.L1_LEFT = 0.0f;
    config.L1_RIGHT = 1.0f;
    config.L2_LEFT = 0.0f;
    config.L2_RIGHT = 1.0f;
    config.R0_LEFT = 0.0f;
    config.R0_RIGHT = 1.0f;
    config.R1_LEFT = 0.0f;
    config.R1_RIGHT = 1.0f;
    config.R2_LEFT = 0.0f;
    config.R2_RIGHT = 1.0f;
    
    // 反转标志配置
    config.L0_REVERSE = false;
    config.L1_REVERSE = false;
    config.L2_REVERSE = false;
    config.R0_REVERSE = false;
    config.R1_REVERSE = false;
    config.R2_REVERSE = false;
    
    // 模式配置
    config.MODE = 7.0f;
    
    return config;
}

// 主配置工厂函数
Setting_Servo getDefaultServoConfig() {
#if CONFIG_IDF_TARGET_ESP32C3
#ifdef CONFIG_SERVO_MODE_OSR
    ESP_LOGI(TAG, "Using OSR config for ESP32-C3");
    return getOSRConfig_C3();
#elif defined(CONFIG_SERVO_MODE_TRR)
    ESP_LOGI(TAG, "Using TrR config for ESP32-C3");
    return getTrRConfig_C3();
#elif defined(CONFIG_SERVO_MODE_TRR_MAX)
    ESP_LOGI(TAG, "Using TrR_MAX config for ESP32-C3");
    return getTrRMaxConfig_C3();
#elif defined(CONFIG_SERVO_MODE_ZDT)
    ESP_LOGI(TAG, "Using ZDT config for ESP32-C3");
    return getZDTConfig_C3();
#elif defined(CONFIG_SERVO_MODE_SR6)
    ESP_LOGI(TAG, "Using SR6 config for ESP32-C3");
    return getSR6Config_C3();
#elif defined(CONFIG_SERVO_MODE_SR6_SHALI)
    ESP_LOGI(TAG, "Using SR6_SHALI config for ESP32-C3");
    return getSR6SHALIConfig_C3();
#elif defined(CONFIG_SERVO_MODE_SR6CAN)
    ESP_LOGI(TAG, "Using SR6CAN config for ESP32-C3");
    return getSR6CANConfig_C3();
#elif defined(CONFIG_SERVO_MODE_O6)
    ESP_LOGI(TAG, "Using O6 config for ESP32-C3");
    return getO6Config_C3();
#else  // OSR (默认)
    ESP_LOGI(TAG, "Using OSR config for ESP32-C3 (default)");
    return getOSRConfig_C3();
#endif
#elif CONFIG_IDF_TARGET_ESP32
#ifdef CONFIG_SERVO_MODE_OSR
    ESP_LOGI(TAG, "Using OSR config for ESP32");
    return getOSRConfig_ESP32();
#elif defined(CONFIG_SERVO_MODE_TRR)
    ESP_LOGI(TAG, "Using TrR config for ESP32");
    return getTrRConfig_ESP32();
#elif defined(CONFIG_SERVO_MODE_TRR_MAX)
    ESP_LOGI(TAG, "Using TrR_MAX config for ESP32");
    return getTrRMaxConfig_ESP32();
#elif defined(CONFIG_SERVO_MODE_ZDT)
    ESP_LOGI(TAG, "Using ZDT config for ESP32");
    return getZDTConfig_ESP32();
#elif defined(CONFIG_SERVO_MODE_SR6_PULSE)
    ESP_LOGI(TAG, "Using SR6_PULSE config for ESP32");
    return getSR6PulseConfig_ESP32();
#elif defined(CONFIG_SERVO_MODE_O6)
    ESP_LOGI(TAG, "Using O6 config for ESP32");
    return getO6Config_ESP32();
#else  // 默认情况
    ESP_LOGI(TAG, "Using OSR config for ESP32 (default)");
    return getOSRConfig_ESP32();
#endif
#else
    // 默认配置，如果未定义任何开发板
    ESP_LOGI(TAG, "Using OSR config for ESP32-C3 (default unknown board)");
    return getOSRConfig_C3();
#endif
}

