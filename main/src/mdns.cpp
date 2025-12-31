#include "mdns.hpp"
#include "esp_log.h"
#include "mdns.h"
#include "setting.hpp"
#include <cstring>

static const char *TAG = "mdns";

// 初始化mDNS服务
esp_err_t init_mdns() {
  ESP_LOGI(TAG, "Initializing mDNS...");

  // 加载setting配置
  SettingWrapper setting;
  try {
    setting.loadFromFile();
    ESP_LOGI(TAG, "成功加载mDNS配置");
  } catch (const std::exception& e) {
    ESP_LOGE(TAG, "加载mDNS配置失败: %s", e.what());
    ESP_LOGE(TAG, "使用默认配置");
    // 使用默认值
    setting->mdns.name[0] = 't';
    setting->mdns.name[1] = 'c';
    setting->mdns.name[2] = 'o';
    setting->mdns.name[3] = 'd';
    setting->mdns.name[4] = 'e';
    setting->mdns.name[5] = '\0';
    setting->wifi.tcp_port = 8000;
    setting->wifi.udp_port = 8000;
  }

  // 初始化mDNS
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mDNS初始化失败: %s", esp_err_to_name(err));
    return err;
  }

  // 检查mDNS名称是否为空
  const char* mdns_name = setting->mdns.name;
  if (strlen(mdns_name) == 0) {
    ESP_LOGW(TAG, "mDNS名称为空，使用默认值: tcode");
    mdns_name = "tcode";
  }

  // 设置mDNS主机名（会自动添加.local后缀）
  err = mdns_hostname_set(mdns_name);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "设置mDNS主机名失败: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "mDNS主机名设置为: %s.local", mdns_name);

  // 获取TCP和UDP端口配置
  int32_t tcp_port = setting->wifi.tcp_port;
  int32_t udp_port = setting->wifi.udp_port;

  // 验证端口号有效
  if (tcp_port <= 0 || tcp_port > 65535) {
    ESP_LOGW(TAG, "无效的TCP端口号: %d，使用默认值: 8000", tcp_port);
    tcp_port = 8000;
  }
  if (udp_port <= 0 || udp_port > 65535) {
    ESP_LOGW(TAG, "无效的UDP端口号: %d，使用默认值: 8000", udp_port);
    udp_port = 8000;
  }

  // 添加HTTP服务 (_http._tcp:80)
  err = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "添加HTTP服务失败: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "mDNS HTTP服务已添加: _http._tcp:80");

  // 添加TCP服务 (_tcode._tcp)
  err = mdns_service_add(NULL, "_tcode", "_tcp", tcp_port, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "添加TCP服务失败: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "mDNS TCP服务已添加: _tcode._tcp:%d", tcp_port);

  // 添加UDP服务 (_tcode._udp)
  err = mdns_service_add(NULL, "_tcode", "_udp", udp_port, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "添加UDP服务失败: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "mDNS UDP服务已添加: _tcode._udp:%d", udp_port);

  ESP_LOGI(TAG, "mDNS初始化完成");
  return ESP_OK;
}
