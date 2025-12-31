#pragma once

#include "proto/setting.pb.h"

// ESP32 C3 开发板配置函数声明
Setting_Servo getOSRConfig_C3();
Setting_Servo getTrRConfig_C3();
Setting_Servo getTrRMaxConfig_C3();
Setting_Servo getZDTConfig_C3();
Setting_Servo getSR6Config_C3();
Setting_Servo getSR6SHALIConfig_C3();
Setting_Servo getSR6CANConfig_C3();

// ESP32 开发板配置函数声明
Setting_Servo getOSRConfig_ESP32();
Setting_Servo getTrRConfig_ESP32();
Setting_Servo getTrRMaxConfig_ESP32();
Setting_Servo getZDTConfig_ESP32();
Setting_Servo getSR6PulseConfig_ESP32();

// 主配置工厂函数
Setting_Servo getDefaultServoConfig();

