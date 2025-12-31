#include "esp_netif_types.h"
#include "globals.hpp"
#ifdef __cplusplus
#include "decoy.hpp"
#include "esp_netif.h"
#include "esp_system.h"
#include "http_router.hpp"
#include "setting.hpp"
#include "static_file_handler.hpp"
#include "utils.hpp"
#include "executor/executor_factory.hpp"
#include "wifi.hpp"

GET("/hello", [](httpd_req_t *req) -> esp_err_t {
  httpd_resp_send(req, "Hello, World!", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
})

GET("/hello2", [](httpd_req_t *req) -> esp_err_t {
  httpd_resp_send(req, "Hello, World!2 ", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
})

GET("/api/mem", [](httpd_req_t *req) -> esp_err_t {
  char resp[128];
  snprintf(resp, sizeof(resp), "可用堆内存: %d, 总堆内存: %d",
           (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
           (int)heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
})

GET("/api/restart", [](httpd_req_t *req) -> esp_err_t {
  esp_restart();
  httpd_resp_send(req, "重启中...", HTTPD_RESP_USE_STRLEN);
  vTaskDelay(pdMS_TO_TICKS(1000));
  return ESP_OK;
})

GET("/api/tasks", [](httpd_req_t *req) -> esp_err_t {
  // 分配足够的缓冲区来存储任务列表
  // 每个任务大约需要40字节，假设最多有20个任务
  const size_t taskListBufferSize = 20 * 40;
  char *taskListBuffer = (char *)malloc(taskListBufferSize);

  if (taskListBuffer == nullptr) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // 获取任务列表
  vTaskList(taskListBuffer);

  // 发送响应
  httpd_resp_send(req, taskListBuffer, HTTPD_RESP_USE_STRLEN);

  // 释放缓冲区
  free(taskListBuffer);

  return ESP_OK;
})

GET("/api/ipinfo", [](httpd_req_t *req) -> esp_err_t {
  static const char *TAG = "api_ipinfo";

  // 获取STA和AP的netif句柄
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

  // 使用智能指针分配内存
  const size_t response_size = 512;
  std::unique_ptr<char[]> response(new (std::nothrow) char[response_size]);

  if (!response) {
    ESP_LOGE(TAG, "内存分配失败，需要 %zu 字节", response_size);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // 获取AP的IP信息（无论STA是否连接都需要）
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
          snprintf(response.get(), response_size,
                   "{\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"" IPSTR "\"}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&dns_info.ip.u_addr.ip4),
                   IP2STR(&ap_ip_info.ip));
        } else {
          snprintf(response.get(), response_size,
                   "{\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"未初始化\"}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&dns_info.ip.u_addr.ip4));
        }
      } else {
        // 如果获取DNS失败，返回默认值
        if (ap_ip_valid) {
          snprintf(response.get(), response_size,
                   "{\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"" IPSTR "\"}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&ip_info.ip), IP2STR(&ap_ip_info.ip));
        } else {
          snprintf(response.get(), response_size,
                   "{\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                   "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                   "\"softap_ip\":\"未初始化\"}",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw), IP2STR(&ip_info.ip));
        }
      }
    } else {
      // STA未连接
      if (ap_ip_valid) {
        snprintf(response.get(), response_size,
                 "{\"sta_ip\":\"未连接\",\"softap_ip\":\"" IPSTR "\"}",
                 IP2STR(&ap_ip_info.ip));
      } else {
        snprintf(response.get(), response_size,
                 "{\"sta_ip\":\"未连接\",\"softap_ip\":\"未初始化\"}");
      }
    }
  } else {
    // STA netif不存在，只返回AP IP
    if (ap_ip_valid) {
      snprintf(response.get(), response_size,
               "{\"sta_ip\":\"未连接\",\"softap_ip\":\"" IPSTR "\"}",
               IP2STR(&ap_ip_info.ip));
    } else {
      snprintf(response.get(), response_size,
               "{\"sta_ip\":\"未连接\",\"softap_ip\":\"未初始化\"}");
    }
  }

  // 设置响应头
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response.get(), HTTPD_RESP_USE_STRLEN);

  ESP_LOGI(TAG, "IP信息: %s", response.get());

  // 智能指针会自动释放内存，无需手动free

  return ESP_OK;
})

GET("/api/decoy", [](httpd_req_t *req) -> esp_err_t {
  static const char *TAG = "api_decoy";

  try {
    // 分配查询参数缓冲区
    const size_t query_buf_size = 64;
    std::unique_ptr<char[]> query_buf(new (std::nothrow) char[query_buf_size]);

    if (!query_buf) {
      ESP_LOGE(TAG, "内存分配失败");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    // 获取查询参数
    size_t query_len = httpd_req_get_url_query_len(req);

    if (query_len > 0 && query_len < query_buf_size) {
      httpd_req_get_url_query_str(req, query_buf.get(), query_buf_size);

      // 分配电压参数缓冲区
      const size_t voltage_str_size = 16;
      std::unique_ptr<char[]> voltage_str(
          new (std::nothrow) char[voltage_str_size]);

      if (!voltage_str) {
        ESP_LOGE(TAG, "内存分配失败");
        httpd_resp_send_500(req);
        return ESP_FAIL;
      }

      // 解析电压参数
      if (httpd_query_key_value(query_buf.get(), "vol", voltage_str.get(),
                                voltage_str_size) == ESP_OK) {
        int voltage_value = atoi(voltage_str.get());

        // 根据电压值确定电压等级
        VoltageLevel level;
        switch (voltage_value) {
        case 9:
          level = VoltageLevel::V9;
          break;
        case 12:
          level = VoltageLevel::V12;
          break;
        case 15:
          level = VoltageLevel::V15;
          break;
        default:
          ESP_LOGE(TAG, "无效的电压值: %d (必须是9、12或15)", voltage_value);
          httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                              "Invalid voltage value (must be 9, 12 or 15)");
          return ESP_FAIL;
        }

        // 设置电压
        bool success = Decoy::getInstance()->setVoltage(level);
        if (success) {
          httpd_resp_set_status(req, "200 OK");
          httpd_resp_set_type(req, "application/json");
          httpd_resp_send(
              req,
              R"({"status":"success","message":"Voltage set successfully"})",
              HTTPD_RESP_USE_STRLEN);
          ESP_LOGI(TAG, "电压设置成功: %dV", voltage_value);
          return ESP_OK;
        } else {
          ESP_LOGE(TAG, "电压设置失败: %dV", voltage_value);
          httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                              "Failed to set voltage");
          return ESP_FAIL;
        }
      } else {
        ESP_LOGE(TAG, "缺少voltage参数");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "Missing 'voltage' parameter");
        return ESP_FAIL;
      }
    } else {
      ESP_LOGE(TAG, "无效的查询参数或无查询参数");
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                          "Missing query parameters");
      return ESP_FAIL;
    }

  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "设置电压异常: %s", e.what());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Exception occurred");
    return ESP_FAIL;
  }
})

GET("/api/vol", [](httpd_req_t *req) -> esp_err_t {
  static const char *TAG = "api_vol";

  try {
    // 获取Voltage单例实例并读取电压
    float voltage = Voltage::getInstance()->getVoltage();

    // 分配响应缓冲区
    const size_t response_size = 32;
    std::unique_ptr<char[]> response(new (std::nothrow) char[response_size]);

    if (!response) {
      ESP_LOGE(TAG, "内存分配失败");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    // 格式化纯文本响应（仅包含数字）
    snprintf(response.get(), response_size, "%.2f", voltage);

    // 设置响应头并发送
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, response.get(), HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "电压读取成功: %.2fV", voltage);
    return ESP_OK;

  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "读取电压失败: %s", e.what());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to read voltage");
    return ESP_FAIL;
  }
})

GET("/api/setting", [](httpd_req_t *req) -> esp_err_t {
  static const char *TAG = "api_setting_get";

  try {
    // 创建SettingWrapper对象并加载配置文件
    SettingWrapper setting;
    setting.loadFromFile(SETTING_FILE_PATH);

    // 获取编码后数据所需的最大缓冲区大小
    size_t max_size = SettingWrapper::getMaxEncodeSize();
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[max_size]);

    if (!buffer) {
      ESP_LOGE(TAG, "内存分配失败，需要 %zu 字节", max_size);
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    // 将Setting结构体编码为字节数组
    size_t encoded_size = setting.encode(buffer.get(), max_size);
    if (encoded_size == 0) {
      ESP_LOGE(TAG, "编码Setting失败");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "Failed to encode Setting");
      return ESP_FAIL;
    }

    // 设置响应头
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/octet-stream");
    // httpd_resp_set_hdr(req, "Content-Disposition", "attachment;
    // filename=\"setting.bin\"");

    // 发送protobuf编码的数据
    httpd_resp_send(req, reinterpret_cast<const char *>(buffer.get()),
                    encoded_size);

    ESP_LOGI(TAG, "Setting数据读取并编码成功，大小: %zu 字节", encoded_size);
    return ESP_OK;

  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "读取Setting失败: %s", e.what());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to load Setting from file");
    return ESP_FAIL;
  }
})

POST("/api/setting", [](httpd_req_t *req) -> esp_err_t {
  static const char *TAG = "api_setting";

  try {
    // 获取POST数据内容长度
    size_t recv_size = req->content_len;
    if (recv_size == 0) {
      ESP_LOGE(TAG, "无效的Content-Length");
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Content-Length");
      return ESP_FAIL;
    }

    // 分配缓冲区接收数据
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[recv_size + 1]);
    if (!buffer) {
      ESP_LOGE(TAG, "内存分配失败，需要 %zu 字节", recv_size);
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    // 接收POST数据
    int total_received = httpd_req_recv(req, buffer.get(), recv_size);
    if (total_received <= 0) {
      ESP_LOGE(TAG, "接收POST数据失败，接收字节数: %d", total_received);
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                          "Failed to receive POST data");
      return ESP_FAIL;
    }

    // 保存旧的配置用于比较 WiFi 设置
    SettingWrapper old_setting;
    old_setting.loadFromFile();

    // 使用SettingWrapper解码protobuf数据
    SettingWrapper setting(reinterpret_cast<const uint8_t *>(buffer.get()),
                           total_received);
    setting.saveToFile();
    g_executor = ExecutorFactory::createExecutor(setting);

    // 检查 WiFi 配置是否变化
    if (old_setting.isWifiConfigChanged(setting)) {
      ESP_LOGI(TAG, "检测到 WiFi 配置变化，重新配置 WiFi...");
      esp_err_t ret = wifi_reconfigure();
      if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi 重新配置失败: %s", esp_err_to_name(ret));
      }
    }

    // 返回成功响应
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, R"({"status":"success","message":"Setting received and decoded"})",
        HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Setting数据接收并解码成功，大小: %d 字节", total_received);
    return ESP_OK;

  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "解码Setting失败: %s", e.what());
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                        "Failed to decode Setting protobuf");
    return ESP_FAIL;
  }
})

GET("/api/version", [](httpd_req_t *req) -> esp_err_t {
  static const char *TAG = "api_version";
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  std::string version;
  bool success = false;
  try {
    version = getBuildParameters();
    success = true;
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "获取版本信息失败: %s", e.what());
    version =
        "{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
    success = false;
  }
  if (success) {
    httpd_resp_send(req, version.c_str(), HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, version.c_str());
  }
  return ESP_OK;
})

GET("/api/modes", [](httpd_req_t *req) -> esp_err_t {
  static const char *TAG = "api_executor_modes";

  try {
    // 获取所有支持的mode
    std::vector<int32_t> modes = ExecutorFactory::getSupportedModes();

    // 计算响应缓冲区大小
    // 每个mode数字最多11位（包括负号），加上逗号和空格，每个约15字节
    // 加上数组格式，总共约需要 modes.size() * 15 + 32 字节
    const size_t response_size = modes.size() * 15 + 32;
    std::unique_ptr<char[]> response(new (std::nothrow) char[response_size]);

    if (!response) {
      ESP_LOGE(TAG, "内存分配失败，需要 %zu 字节", response_size);
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    // 构建JSON数组
    char *ptr = response.get();
    int written = snprintf(ptr, response_size, "[");
    if (written < 0 || written >= static_cast<int>(response_size)) {
      ESP_LOGE(TAG, "格式化JSON失败");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    ptr += written;
    size_t remaining = response_size - written;

    // 添加每个mode数字
    for (size_t i = 0; i < modes.size(); ++i) {
      // 添加逗号（除了第一个元素）
      if (i > 0) {
        written = snprintf(ptr, remaining, ",");
        if (written < 0 || written >= static_cast<int>(remaining)) {
          ESP_LOGE(TAG, "格式化JSON失败");
          httpd_resp_send_500(req);
          return ESP_FAIL;
        }
        ptr += written;
        remaining -= written;
      }

      // 添加mode数字
      written = snprintf(ptr, remaining, "%ld", static_cast<long>(modes[i]));
      if (written < 0 || written >= static_cast<int>(remaining)) {
        ESP_LOGE(TAG, "格式化JSON失败");
        httpd_resp_send_500(req);
        return ESP_FAIL;
      }
      ptr += written;
      remaining -= written;
    }

    // 关闭数组
    written = snprintf(ptr, remaining, "]");
    if (written < 0 || written >= static_cast<int>(remaining)) {
      ESP_LOGE(TAG, "格式化JSON失败");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    // 设置响应头并发送
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response.get(), HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "返回executor modes列表: %s", response.get());
    return ESP_OK;

  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "获取executor modes失败: %s", e.what());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to get executor modes");
    return ESP_FAIL;
  }
})

// 静态文件处理器 - 使用通配符匹配所有路径
GET("*", static_file_handler)

#endif
