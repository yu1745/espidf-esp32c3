#pragma once

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// WiFi配置
#define WIFI_SSID "ZTE-Y55AcX"
#define WIFI_PASS "asdk7788"

// HTTP服务器初始化函数
esp_err_t http_server_init(void);

// WiFi初始化函数
void wifi_init(void);

// WiFi事件处理器
void wifi_event_handler(void* arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data);