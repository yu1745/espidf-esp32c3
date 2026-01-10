#include "http/event_websocket.hpp"
#include "analysis.h"
#include "executor/executor.hpp"
#include "uart/uart.h"
#include "uart/usb_monitor.hpp"
#include "voltage.hpp"
#include "globals.hpp"
#include "http/websocket_server.h"
#include "utils.hpp"
#include "wifi.hpp"
#include <cstring>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <memory>
#include <mutex>
#include <vector>


static const char *TAG = "event_websocket";

// WebSocket客户端列表
static std::vector<int> event_clients;

// 客户端列表互斥锁
static std::mutex clients_mutex;

// 事件处理器是否已注册
static bool event_handler_registered = false;

/**
 * @brief 将CPU占用事件转换为JSON字符串
 */
static void cpu_usage_event_to_json(char *buffer, size_t buffer_size,
                                    analysis_cpu_usage_event_data_t *data) {
  snprintf(buffer, buffer_size,
           "{\"type\":\"cpu_usage\","
           "\"cpu_percent\":%.2f,"
           "\"free_heap\":%u,"
           "\"total_heap\":%u}",
           data->cpu_usage_percent, (unsigned int)data->free_heap_size,
           (unsigned int)data->total_heap_size);
}

/**
 * @brief 将动作统计事件转换为JSON字符串
 */
static void motion_stats_event_to_json(char *buffer, size_t buffer_size,
                                       motion_stats_event_data_t *data) {
  snprintf(buffer, buffer_size,
           "{\"type\":\"motion_stats\","
           "\"window\":%.1f,"
           "\"compute\":{\"avg_ms\":%.3f,\"stddev_ms\":%.3f,\"max_ms\":%.3f,\"freq\":%.2f},"
           "\"execute\":{\"avg_ms\":%.3f,\"stddev_ms\":%.3f,\"max_ms\":%.3f,\"freq\":%.2f}}",
           data->window_seconds,
           data->compute_avg_ms, data->compute_stddev_ms,
           data->compute_max_ms, data->compute_freq,
           data->execute_avg_ms, data->execute_stddev_ms,
           data->execute_max_ms, data->execute_freq);
}

/**
 * @brief 将USB事件转换为JSON字符串
 */
static void usb_event_to_json(char *buffer, size_t buffer_size,
                              int32_t event_id,
                              usb_monitor_event_data_t *data) {
  const char *event_name =
      (event_id == USB_MONITOR_EVENT_CONNECTED) ? "connected" : "disconnected";

  snprintf(buffer, buffer_size,
           "{\"type\":\"usb\","
           "\"event\":\"%s\","
           "\"connected\":%s,"
           "\"timestamp\":%lld}",
           event_name, data->connected ? "true" : "false", data->timestamp);
}

/**
 * @brief 将电压事件转换为JSON字符串
 */
static void voltage_event_to_json(char *buffer, size_t buffer_size,
                                  voltage_reading_event_data_t *data) {
  snprintf(buffer, buffer_size,
           "{\"type\":\"voltage\","
           "\"voltage\":%.2f,"
           "\"adc_raw\":%d,"
           "\"timestamp\":%lld}",
           data->voltage, data->adc_raw, data->timestamp);
}

/**
 * @brief 将版本信息转换为JSON字符串
 */
static void version_info_to_json(char *buffer, size_t buffer_size) {
  try {
    std::string version = getBuildParameters();
    // 将原有JSON作为data子项嵌套
    snprintf(buffer, buffer_size, "{\"type\":\"version\",\"data\":%s}",
             version.c_str());
  } catch (const std::exception &e) {
    snprintf(buffer, buffer_size,
             "{\"type\":\"version\",\"status\":\"error\",\"message\":\"%s\"}",
             e.what());
  }
}

/**
 * @brief 将IP信息转换为JSON字符串
 */
static void ipinfo_to_json(char *buffer, size_t buffer_size) {
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

  // 获取AP的IP信息
  esp_netif_ip_info_t ap_ip_info;
  bool ap_ip_valid = false;

  if (ap_netif) {
    esp_err_t ap_ret = esp_netif_get_ip_info(ap_netif, &ap_ip_info);
    if (ap_ret == ESP_OK && ap_ip_info.ip.addr != 0) {
      ap_ip_valid = true;
    }
  }

  if (sta_netif) {
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(sta_netif, &ip_info);

    if (ret == ESP_OK && ip_info.ip.addr != 0) {
      // STA已连接，获取DNS信息
      esp_netif_dns_info_t dns_info;
      ret = esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);

      if (ret == ESP_OK) {
        if (ap_ip_valid) {
          snprintf(buffer, buffer_size,
                   "{\"type\":\"ipinfo\","
                   "\"data\":{"
                   "\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"" IPSTR "\"}}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&dns_info.ip.u_addr.ip4),
                   IP2STR(&ap_ip_info.ip));
        } else {
          snprintf(buffer, buffer_size,
                   "{\"type\":\"ipinfo\","
                   "\"data\":{"
                   "\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"未初始化\"}}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&dns_info.ip.u_addr.ip4));
        }
      } else {
        // 获取DNS失败，使用IP作为默认DNS
        if (ap_ip_valid) {
          snprintf(buffer, buffer_size,
                   "{\"type\":\"ipinfo\","
                   "\"data\":{"
                   "\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"" IPSTR "\"}}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&ip_info.ip),
                   IP2STR(&ap_ip_info.ip));
        } else {
          snprintf(buffer, buffer_size,
                   "{\"type\":\"ipinfo\","
                   "\"data\":{"
                   "\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"未初始化\"}}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&ip_info.ip));
        }
      }
    } else {
      // STA未连接
      if (ap_ip_valid) {
        snprintf(buffer, buffer_size,
                 "{\"type\":\"ipinfo\","
                 "\"data\":{\"sta_ip\":\"未连接\",\"softap_ip\":\"" IPSTR "\"}}",
                 IP2STR(&ap_ip_info.ip));
      } else {
        snprintf(buffer, buffer_size,
                 "{\"type\":\"ipinfo\","
                 "\"data\":{\"sta_ip\":\"未连接\",\"softap_ip\":\"未初始化\"}}");
      }
    }
  } else {
    // STA netif不存在
    if (ap_ip_valid) {
      snprintf(buffer, buffer_size,
               "{\"type\":\"ipinfo\","
               "\"data\":{\"sta_ip\":\"未连接\",\"softap_ip\":\"" IPSTR "\"}}",
               IP2STR(&ap_ip_info.ip));
    } else {
      snprintf(buffer, buffer_size,
               "{\"type\":\"ipinfo\","
               "\"data\":{\"sta_ip\":\"未连接\",\"softap_ip\":\"未初始化\"}}");
    }
  }
}

/**
 * @brief 广播消息到所有连接的客户端
 */
static void broadcast_to_all_clients(const char *message) {
  std::lock_guard<std::mutex> lock(clients_mutex);

  for (auto it = event_clients.begin(); it != event_clients.end();) {
    int client_fd = *it;
    esp_err_t ret = websocket_send_to_client(g_http_server, client_fd, message,
                                             strlen(message));
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "发送到客户端 %d 失败，移除该客户端", client_fd);
      it = event_clients.erase(it);
    } else {
      ++it;
    }
  }
}

/**
 * @brief 通用事件处理器 - 监听所有事件并通过WebSocket发送
 */
static void universal_event_handler(void *handler_arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id, void *event_data) {
  char json_buffer[512];  // 增加缓冲区大小以容纳更复杂的JSON

  // 处理 ANALYSIS_EVENT
  if (event_base == ANALYSIS_EVENT) {
    if (event_id == ANALYSIS_EVENT_CPU_USAGE && event_data != nullptr) {
      analysis_cpu_usage_event_data_t *data =
          (analysis_cpu_usage_event_data_t *)event_data;
      cpu_usage_event_to_json(json_buffer, sizeof(json_buffer), data);
      broadcast_to_all_clients(json_buffer);
    }
  }
  // 处理 MOTION_EVENT
  else if (event_base == MOTION_EVENT) {
    if (event_id == MOTION_EVENT_STATS && event_data != nullptr) {
      motion_stats_event_data_t *data =
          (motion_stats_event_data_t *)event_data;
      motion_stats_event_to_json(json_buffer, sizeof(json_buffer), data);
      broadcast_to_all_clients(json_buffer);
    }
  }
  // 处理 USB_MONITOR_EVENT
  else if (event_base == USB_MONITOR_EVENT) {
    if (event_data != nullptr) {
      usb_monitor_event_data_t *data =
          (usb_monitor_event_data_t *)event_data;
      usb_event_to_json(json_buffer, sizeof(json_buffer), event_id, data);
      broadcast_to_all_clients(json_buffer);
    }
  }
  // 处理 VOLTAGE_EVENT
  else if (event_base == VOLTAGE_EVENT) {
    if (event_id == VOLTAGE_EVENT_READING && event_data != nullptr) {
      voltage_reading_event_data_t *data =
          (voltage_reading_event_data_t *)event_data;
      voltage_event_to_json(json_buffer, sizeof(json_buffer), data);
      broadcast_to_all_clients(json_buffer);
    }
  }
}

/**
 * @brief WebSocket处理器
 */
static esp_err_t event_websocket_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "事件WebSocket握手请求");
    // ESP-IDF会自动处理握手

    // 获取客户端套接字并添加到列表
    int client_fd = httpd_req_to_sockfd(req);
    if (client_fd >= 0) {
      std::lock_guard<std::mutex> lock(clients_mutex);
      // 检查是否已存在
      if (std::find(event_clients.begin(), event_clients.end(), client_fd) ==
          event_clients.end()) {
        event_clients.push_back(client_fd);
        ESP_LOGI(TAG, "客户端 %d 已添加到事件列表", client_fd);
      }
    }

    // 发送当前 USB 状态给新连接的客户端
    std::unique_ptr<char[]> usb_status_json(new (std::nothrow) char[256]);
    if (!usb_status_json) {
      ESP_LOGE(TAG, "分配USB状态缓冲区失败");
      return ESP_OK;
    }

    bool usb_connected = uart_is_usb_connected();
    int32_t event_id = usb_connected ? USB_MONITOR_EVENT_CONNECTED
                                      : USB_MONITOR_EVENT_DISCONNECTED;
    usb_monitor_event_data_t usb_data = {
        .connected = usb_connected,
        .timestamp = esp_timer_get_time(),
    };

    usb_event_to_json(usb_status_json.get(), 256, event_id, &usb_data);
    websocket_send_to_client(g_http_server, client_fd, usb_status_json.get(),
                             strlen(usb_status_json.get()));
    ESP_LOGI(TAG, "已发送当前USB状态给客户端 %d: %s", client_fd,
             usb_status_json.get());

    // 发送版本信息给新连接的客户端
    std::unique_ptr<char[]> version_json(new (std::nothrow) char[1024]);
    if (version_json) {
      version_info_to_json(version_json.get(), 1024);
      websocket_send_to_client(g_http_server, client_fd, version_json.get(),
                               strlen(version_json.get()));
      ESP_LOGI(TAG, "已发送版本信息给客户端 %d", client_fd);
    } else {
      ESP_LOGE(TAG, "分配版本信息缓冲区失败");
    }

    // 发送 IP 信息给新连接的客户端
    std::unique_ptr<char[]> ipinfo_json(new (std::nothrow) char[512]);
    if (ipinfo_json) {
      ipinfo_to_json(ipinfo_json.get(), 512);
      websocket_send_to_client(g_http_server, client_fd, ipinfo_json.get(),
                               strlen(ipinfo_json.get()));
      ESP_LOGI(TAG, "已发送IP信息给客户端 %d: %s", client_fd,
               ipinfo_json.get());
    } else {
      ESP_LOGE(TAG, "分配IP信息缓冲区失败");
    }

    return ESP_OK;
  }

  // 获取客户端套接字
  int client_fd = httpd_req_to_sockfd(req);
  if (client_fd < 0) {
    ESP_LOGE(TAG, "获取客户端套接字失败");
    return ESP_FAIL;
  }

  uint8_t buf[1024] = {0};
  httpd_ws_frame_t ws_pkt;

  // 清零结构体
  std::memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.payload = buf;

  // 接收WebSocket帧
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "接收WebSocket帧失败");
    return ret;
  }

  // 处理不同类型的帧
  if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
    ESP_LOGI(TAG, "收到来自客户端 %d 的消息: %.*s", client_fd, ws_pkt.len,
             ws_pkt.payload);

    // 可以在这里处理客户端发送的消息（如果需要）

  } else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
    ESP_LOGI(TAG, "收到来自客户端 %d 的PING帧", client_fd);
    // 自动回复PONG帧
    httpd_ws_frame_t pong;
    std::memset(&pong, 0, sizeof(httpd_ws_frame_t));
    pong.type = HTTPD_WS_TYPE_PONG;
    httpd_ws_send_frame(req, &pong);

  } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
    ESP_LOGI(TAG, "收到来自客户端 %d 的CLOSE帧", client_fd);
    // 移除客户端
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = std::find(event_clients.begin(), event_clients.end(), client_fd);
    if (it != event_clients.end()) {
      event_clients.erase(it);
    }
  }

  return ESP_OK;
}

// HTTP URI配置
static httpd_uri_t event_websocket_uri = {0};

/**
 * @brief 初始化事件WebSocket系统
 */
esp_err_t event_websocket_init(httpd_handle_t server) {
  if (!server) {
    ESP_LOGE(TAG, "无效的HTTP服务器句柄");
    return ESP_FAIL;
  }

  // 配置WebSocket URI
  event_websocket_uri.uri = "/event";
  event_websocket_uri.method = HTTP_GET;
  event_websocket_uri.handler = event_websocket_handler;
  event_websocket_uri.user_ctx = NULL;
  event_websocket_uri.is_websocket = true;

  // 注册WebSocket处理器
  esp_err_t ret = httpd_register_uri_handler(server, &event_websocket_uri);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "注册事件WebSocket处理器失败: %s", esp_err_to_name(ret));
    return ret;
  }

  // 注册通用事件处理器到默认事件循环
  // 注册 ANALYSIS_EVENT 的所有事件
  ret = esp_event_handler_register(ANALYSIS_EVENT, ESP_EVENT_ANY_ID,
                                   &universal_event_handler, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "注册ANALYSIS_EVENT处理器失败: %s", esp_err_to_name(ret));
    return ret;
  }

  // 注册 MOTION_EVENT 的所有事件
  ret = esp_event_handler_register(MOTION_EVENT, ESP_EVENT_ANY_ID,
                                   &universal_event_handler, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "注册MOTION_EVENT处理器失败: %s", esp_err_to_name(ret));
    return ret;
  }

  // 注册 USB_MONITOR_EVENT 的所有事件
  ret = esp_event_handler_register(USB_MONITOR_EVENT, ESP_EVENT_ANY_ID,
                                   &universal_event_handler, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "注册USB_MONITOR_EVENT处理器失败: %s", esp_err_to_name(ret));
    return ret;
  }

  // 注册 VOLTAGE_EVENT 的所有事件
  ret = esp_event_handler_register(VOLTAGE_EVENT, ESP_EVENT_ANY_ID,
                                   &universal_event_handler, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "注册VOLTAGE_EVENT处理器失败: %s", esp_err_to_name(ret));
    return ret;
  }

  event_handler_registered = true;

  ESP_LOGI(TAG, "事件WebSocket系统初始化完成，端点: /event");
  return ESP_OK;
}

/**
 * @brief 停止事件WebSocket系统
 */
esp_err_t event_websocket_stop(void) {
  // 注销事件处理器
  if (event_handler_registered) {
    esp_event_handler_unregister(ANALYSIS_EVENT, ESP_EVENT_ANY_ID,
                                 &universal_event_handler);
    esp_event_handler_unregister(MOTION_EVENT, ESP_EVENT_ANY_ID,
                                 &universal_event_handler);
    esp_event_handler_unregister(USB_MONITOR_EVENT, ESP_EVENT_ANY_ID,
                                 &universal_event_handler);
    esp_event_handler_unregister(VOLTAGE_EVENT, ESP_EVENT_ANY_ID,
                                 &universal_event_handler);
    event_handler_registered = false;
  }

  // 清空客户端列表
  {
    std::lock_guard<std::mutex> lock(clients_mutex);
    event_clients.clear();
  }

  ESP_LOGI(TAG, "事件WebSocket系统已停止");
  return ESP_OK;
}

/**
 * @brief 发送事件到所有WebSocket客户端（供外部调用）
 */
esp_err_t event_websocket_broadcast(const char *message, size_t len) {
  broadcast_to_all_clients(message);
  return ESP_OK;
}
