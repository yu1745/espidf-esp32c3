#pragma once

#include "esp_event_base.h"
#include "esp_err.h"

// WiFi配置
// #define WIFI_SSID "ZTE-Y55AcX"
// #define WIFI_PASS "asdk7788"

// HTTP服务器初始化函数
esp_err_t http_server_init(void);

// WiFi初始化函数（可重复调用）
void wifi_init(void);

// 重新配置WiFi连接（用于修改setting后重新连接）
esp_err_t wifi_reconfigure(void);

// WiFi事件处理器
void wifi_event_handler(void* arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data);