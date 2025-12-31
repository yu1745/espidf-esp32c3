#include "wifi.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "globals.hpp"
#include "http/http_router.hpp"
#include "http/websocket_server.h"
#include "nvs_flash.h"
#include "setting.hpp"
#include <cstring>

static const char* TAG = "wifi";

// HTTP服务器配置
#define HTTP_SERVER_PORT 80

// 启动HTTP服务器
esp_err_t http_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 32;
    config.server_port = HTTP_SERVER_PORT;
    config.lru_purge_enable = true;  // 启用LRU清理
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    // 启动HTTP服务器
    esp_err_t ret = httpd_start(&g_http_server, &config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HTTP服务器启动成功，端口: %d", HTTP_SERVER_PORT);
        g_http_server_running = true;
    } else {
        ESP_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(ret));
    }
    return ret;
}

// WiFi事件处理器
void wifi_event_handler(void* arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data) {
    static bool http_server_started = false;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // STA模式启动
        ESP_LOGI(TAG, "STA模式启动，尝试连接...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // STA模式连接断开
        ESP_LOGI(TAG, "WiFi连接断开，尝试重连...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        // AP模式启动
        ESP_LOGI(TAG, "AP模式启动");

        // 检查是否是纯AP模式（没有STA配置）
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode == WIFI_MODE_AP && !http_server_started) {
            ESP_LOGI(TAG, "纯AP模式，启动HTTP服务器");

            // 启动HTTP服务器
            if (http_server_init() == ESP_OK) {
                ESP_LOGI(TAG, "HTTP服务器启动成功");
                // 启动WebSocket服务器
                if (websocket_server_init(g_http_server) == ESP_OK) {
                    ESP_LOGI(TAG, "WebSocket服务器启动成功");
                } else {
                    ESP_LOGE(TAG, "WebSocket服务器启动失败");
                }
                // 注册路由
                HttpRouter::getInstance().registerAllEndpoints(g_http_server);
                http_server_started = true;
            } else {
                ESP_LOGE(TAG, "HTTP服务器启动失败");
            }
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        // 作为AP时终端连接成功
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "站点连接: " MACSTR, MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        // 作为AP时终端断开连接
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "站点断开: " MACSTR, MAC2STR(event->mac));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // STA模式获取到IP地址
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "获取到IP地址: " IPSTR, IP2STR(&event->ip_info.ip));

        if (!http_server_started) {
            // 启动HTTP服务器
            if (http_server_init() == ESP_OK) {
                ESP_LOGI(TAG, "HTTP服务器启动成功");
            } else {
                ESP_LOGE(TAG, "HTTP服务器启动失败");
            }
            // 启动WebSocket服务器
            if (websocket_server_init(g_http_server) == ESP_OK) {
                ESP_LOGI(TAG, "WebSocket服务器启动成功");
            } else {
                ESP_LOGE(TAG, "WebSocket服务器启动失败");
            }
            // 最后注册/*
            HttpRouter::getInstance().registerAllEndpoints(g_http_server);
            http_server_started = true;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        // 作为AP为终端分配IP
        ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*)event_data;
        ESP_LOGI(TAG, "为站点分配IP: " IPSTR, IP2STR(&event->ip));
    }
}

// 初始化WiFi
void wifi_init(void) {
    // 如果NVS初始化失败，则擦除后重新初始化
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 加载setting配置
    SettingWrapper setting;
    try {
        setting.loadFromFile();
        ESP_LOGI(TAG, "成功加载WiFi配置");
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "加载WiFi配置失败: %s", e.what());
        ESP_LOGE(TAG, "使用默认配置");
        return;
    }

    // 检查配置是否有效
    bool has_ap_config = strlen(setting->wifi.soft_ap_ssid) > 0;
    bool has_sta_config = strlen(setting->wifi.ssid) > 0;
    bool enable_soft_ap = setting->wifi.enable_soft_ap;

    ESP_LOGI(TAG, "WiFi配置状态:");
    ESP_LOGI(TAG, "  AP SSID: %s", has_ap_config ? setting->wifi.soft_ap_ssid : "未配置");
    ESP_LOGI(TAG, "  STA SSID: %s", has_sta_config ? setting->wifi.ssid : "未配置");
    ESP_LOGI(TAG, "  Enable Soft AP: %s", enable_soft_ap ? "true" : "false");

    // 确定WiFi模式
    wifi_mode_t wifi_mode = WIFI_MODE_NULL;
    bool enable_ap = has_ap_config && enable_soft_ap;
    bool enable_sta = has_sta_config;

    if (enable_ap && enable_sta) {
        wifi_mode = WIFI_MODE_APSTA;
        ESP_LOGI(TAG, "使用AP+STA共存模式");
    } else if (enable_ap) {
        wifi_mode = WIFI_MODE_AP;
        ESP_LOGI(TAG, "使用AP模式");
    } else if (enable_sta) {
        wifi_mode = WIFI_MODE_STA;
        ESP_LOGI(TAG, "使用STA模式");
    } else {
        ESP_LOGE(TAG, "没有有效的WiFi配置");
        return;
    }

    // 创建对应的netif
    if (enable_sta) {
        esp_netif_create_default_wifi_sta();
    }
    if (enable_ap) {
        esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 设置WiFi模式（必须在配置之前设置）
    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));

    // 注册事件处理器
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
        &instance_any_id));

    // 根据WiFi模式注册相应的IP事件
    if (enable_sta) {
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
            &instance_got_ip));
    }
    if (enable_ap) {
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_event_handler, NULL,
            &instance_got_ip));
    }

    // 配置STA
    if (enable_sta) {
        wifi_config_t sta_config = {};
        strncpy((char*)sta_config.sta.ssid, setting->wifi.ssid, sizeof(sta_config.sta.ssid) - 1);
        strncpy((char*)sta_config.sta.password, setting->wifi.password, sizeof(sta_config.sta.password) - 1);
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        ESP_LOGI(TAG, "STA配置完成，SSID: %s", sta_config.sta.ssid);
    }

    // 配置AP
    if (enable_ap) {
        wifi_config_t ap_config = {};
        strncpy((char*)ap_config.ap.ssid, setting->wifi.soft_ap_ssid, sizeof(ap_config.ap.ssid) - 1);
        ap_config.ap.ssid_len = strlen(setting->wifi.soft_ap_ssid);
        if (strlen(setting->wifi.soft_ap_password) > 0) {
            strncpy((char*)ap_config.ap.password, setting->wifi.soft_ap_password, sizeof(ap_config.ap.password) - 1);
            ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        } else {
            ap_config.ap.authmode = WIFI_AUTH_OPEN;
        }
        ap_config.ap.max_connection = 4;

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_LOGI(TAG, "AP配置完成，SSID: %s, 密码保护: %s", ap_config.ap.ssid,
                 ap_config.ap.authmode == WIFI_AUTH_OPEN ? "否" : "是");

        // 如果只有AP模式，需要在AP启动后启动HTTP服务器
        if (!enable_sta) {
            // 这里需要等待AP启动，通常在WIFI_EVENT_AP_START事件中处理
            // 但为了简化，我们也可以在AP启动事件中启动服务器
        }
    }

    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi初始化完成，模式: %d", wifi_mode);
}