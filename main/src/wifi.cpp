#include "wifi.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "globals.h"
#include "http/http_router.hpp"
#include "http/websocket_server.h"
#include "nvs_flash.h"

static const char* TAG = "wifi";

// HTTP服务器配置
#define HTTP_SERVER_PORT 80

// 启动HTTP服务器
esp_err_t http_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.lru_purge_enable = true;  // 启用LRU清理
    config.uri_match_fn = httpd_uri_match_wildcard;
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
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi连接断开，尝试重连...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "获取到IP地址: " IPSTR, IP2STR(&event->ip_info.ip));

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

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = WIFI_SSID,
                .password = WIFI_PASS,
                .threshold =
                    {
                        .authmode = WIFI_AUTH_WPA2_PSK,
                    },
            },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi初始化完成");
}