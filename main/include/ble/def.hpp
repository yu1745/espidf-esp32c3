#pragma once

#ifdef __cplusplus
#include "ble_router.hpp"
#include "decoy.hpp"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_netif_types.h"
#include "executor/executor_factory.hpp"
#include "globals.hpp"
#include "handyplug/handy_handler.hpp"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"
#include "select_thread.hpp"
#include "setting.hpp"
#include "utils.hpp"
#include "wifi.hpp"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

// 主服务UUID定义 (SERVICE_UUID: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
static ble_uuid128_t tcode_svc_uuid;
static uint16_t tcode_chr_val_handle;

// 主特征UUID定义 (CHARACTERISTIC_UUID: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
static ble_uuid128_t tcode_chr_uuid;

// 设置特征UUID定义 (SETTING_UUID: "271b49ed-672f-48ea-a1c2-40990681a0da")
static ble_uuid128_t tcode_setting_uuid;

// IP特征UUID定义 (IP_UUID: "27920f48-71db-4909-aab7-a3b2f83e4224")
static ble_uuid128_t tcode_ip_uuid;

// 版本特征UUID定义 (VERSION_UUID: "27920f48-71db-4909-aab7-a3b2f83e4225")
static ble_uuid128_t tcode_version_uuid;

// 电压特征UUID定义 (VOL_UUID: "27920f48-71db-4909-aab7-a3b2f83e4226")
static ble_uuid128_t tcode_vol_uuid;

// 重启特征UUID定义 (RESTART_UUID: "27920f48-71db-4909-aab7-a3b2f83e4227")
static ble_uuid128_t tcode_restart_uuid;

// Handy服务UUID定义 (HANDY_SERVICE_UUID:
// "1775244d-6b43-439b-877c-060f2d9bed07")
static ble_uuid128_t handy_svc_uuid;

// Handy特征UUID定义1 (HANDY_CHARACTERISTIC_UUID:
// "1775ff51-6b43-439b-877c-060f2d9bed07")
static ble_uuid128_t handy_chr_uuid1;

// Handy特征UUID定义2 (HANDY_CHARACTERISTIC_UUID2:
// "1775ff55-6b43-439b-877c-060f2d9bed07")
static ble_uuid128_t handy_chr_uuid2;

EXEC_LAMBDA([]() {
  ble_uuid_any_t temp_uuid;
  ble_uuid_from_str(&temp_uuid, "4fafc201-1fb5-459e-8fcc-c5c9c331914b");
  memcpy(&tcode_svc_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "beb5483e-36e1-4688-b7f5-ea07361b26a8");
  memcpy(&tcode_chr_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "271b49ed-672f-48ea-a1c2-40990681a0da");
  memcpy(&tcode_setting_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "27920f48-71db-4909-aab7-a3b2f83e4224");
  memcpy(&tcode_ip_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "27920f48-71db-4909-aab7-a3b2f83e4225");
  memcpy(&tcode_version_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "27920f48-71db-4909-aab7-a3b2f83e4226");
  memcpy(&tcode_vol_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "27920f48-71db-4909-aab7-a3b2f83e4227");
  memcpy(&tcode_restart_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "1775244d-6b43-439b-877c-060f2d9bed07");
  memcpy(&handy_svc_uuid, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "1775ff51-6b43-439b-877c-060f2d9bed07");
  memcpy(&handy_chr_uuid1, &temp_uuid, sizeof(ble_uuid128_t));
  ble_uuid_from_str(&temp_uuid, "1775ff55-6b43-439b-877c-060f2d9bed07");
  memcpy(&handy_chr_uuid2, &temp_uuid, sizeof(ble_uuid128_t));
});

// 主服务注册
BLE_SERVICE(&tcode_svc_uuid.u)
// tcode
WRITE_CHR(
    &tcode_chr_uuid.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      // 主特征访问处理
      static uint8_t tcode_chr_val[256] = {};
      memset(tcode_chr_val, 0, sizeof(tcode_chr_val));

      switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_WRITE_CHR:
        // 保存接收到的数据
        if (ctxt->om->om_len <= sizeof(tcode_chr_val)) {
          memcpy(tcode_chr_val, ctxt->om->om_data, ctxt->om->om_len);

          // 将数据写入全局队列
          if (global_rx_queue != nullptr) {
            // 分配数据包内存
            data_packet_t *packet =
                static_cast<data_packet_t *>(malloc(sizeof(data_packet_t)));
            if (packet == nullptr) {
              ESP_LOGE("BLE", "Failed to allocate memory for BLE packet");
              return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            packet->source = DATA_SOURCE_BLE;
            packet->client_fd = -1;   // BLE没有client_fd
            packet->user_data = NULL; // BLE不需要user_data

            packet->data = static_cast<uint8_t *>(malloc(ctxt->om->om_len));
            if (packet->data == nullptr) {
              ESP_LOGE("BLE", "Failed to allocate memory for BLE data");
              free(packet);
              return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            memcpy(packet->data, ctxt->om->om_data, ctxt->om->om_len);
            packet->length = ctxt->om->om_len;

            // 发送到队列
            if (xQueueSend(global_rx_queue, &packet, pdMS_TO_TICKS(100)) !=
                pdTRUE) {
              // 发送失败，释放内存
              free(packet->data);
              free(packet);
              ESP_LOGW("BLE", "Failed to send BLE data to global queue");
            }
          }

          return 0;
        } else {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
      default:
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    &tcode_chr_val_handle)
// /api/setting
READ_WRITE_CHR(
    &tcode_setting_uuid.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      static const char *TAG = "BLE_SETTING";

      switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_READ_CHR: {
        // 对应HTTP GET /api/setting
        try {
          // 创建SettingWrapper对象并加载配置文件
          SettingWrapper setting;
          setting.loadFromFile(SETTING_FILE_PATH);

          // 获取编码后数据所需的最大缓冲区大小
          size_t max_size = SettingWrapper::getMaxEncodeSize();
          std::unique_ptr<uint8_t[]> buffer(new (std::nothrow)
                                                uint8_t[max_size]);

          if (!buffer) {
            ESP_LOGE(TAG, "内存分配失败，需要 %zu 字节", max_size);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
          }

          // 将Setting结构体编码为字节数组
          size_t encoded_size = setting.encode(buffer.get(), max_size);
          if (encoded_size == 0) {
            ESP_LOGE(TAG, "编码Setting失败");
            return BLE_ATT_ERR_UNLIKELY;
          }

          // 检查缓冲区是否足够大
          if (ctxt->om->om_len < encoded_size) {
            // 尝试使用os_mbuf_append扩展缓冲区
            int rc = os_mbuf_append(ctxt->om, buffer.get(), encoded_size);
            if (rc != 0) {
              ESP_LOGE(TAG, "无法扩展BLE缓冲区，需要 %zu 字节，实际 %u 字节",
                       encoded_size, ctxt->om->om_len);
              return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
          } else {
            // 缓冲区足够大，直接复制数据
            memcpy(ctxt->om->om_data, buffer.get(), encoded_size);
            // 注意：BLE会自动使用om_len来确定返回的数据长度
            // 但我们需要确保数据被正确复制
          }

          ESP_LOGI(TAG, "Setting数据读取并编码成功，大小: %zu 字节",
                   encoded_size);
          return 0;
        } catch (const std::exception &e) {
          ESP_LOGE(TAG, "读取Setting失败: %s", e.what());
          return BLE_ATT_ERR_UNLIKELY;
        }
      }
      case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        // 对应HTTP POST /api/setting
        try {
          // 计算整个mbuf链的总长度（数据可能被分片到多个mbuf中）
          struct os_mbuf *om = ctxt->om;
          size_t recv_size = 0;
          while (om != nullptr) {
            recv_size += om->om_len;
            om = SLIST_NEXT(om, om_next);
          }

          if (recv_size == 0) {
            ESP_LOGE(TAG, "无效的数据长度");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
          }

          ESP_LOGI(TAG, "Setting数据总长度: %zu 字节", recv_size);

          // 分配缓冲区接收数据
          std::unique_ptr<uint8_t[]> buffer(new (std::nothrow)
                                                uint8_t[recv_size]);
          if (!buffer) {
            ESP_LOGE(TAG, "内存分配失败，需要 %zu 字节", recv_size);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
          }

          // 从整个mbuf链复制所有数据
          om = ctxt->om;
          uint8_t *dst = buffer.get();
          while (om != nullptr) {
            memcpy(dst, om->om_data, om->om_len);
            dst += om->om_len;
            om = SLIST_NEXT(om, om_next);
          }

          ESP_LOGI(TAG, "Setting数据接收大小: %zu 字节", recv_size);

          // 保存旧的配置用于比较 WiFi 设置
          SettingWrapper old_setting;
          old_setting.loadFromFile();

          // 使用SettingWrapper解码protobuf数据
          SettingWrapper setting(buffer.get(), recv_size);
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

          ESP_LOGI(TAG, "Setting数据接收并解码成功，大小: %zu 字节", recv_size);
          return 0;
        } catch (const std::exception &e) {
          ESP_LOGE(TAG, "解码Setting失败: %s", e.what());
          return BLE_ATT_ERR_UNLIKELY;
        }
      }
      default:
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    nullptr)
// /api/ip
READ_CHR(
    &tcode_ip_uuid.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      static const char *TAG = "BLE_IP";

      try {
        // 获取STA和AP的netif句柄
        esp_netif_t *sta_netif =
            esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

        // 使用智能指针分配内存
        const size_t response_size = 512;
        std::unique_ptr<char[]> response(
            new (std::nothrow) char[response_size]);

        if (!response) {
          ESP_LOGE(TAG, "内存分配失败，需要 %zu 字节", response_size);
          return BLE_ATT_ERR_INSUFFICIENT_RES;
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
            ret = esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN,
                                         &dns_info);

            if (ret == ESP_OK) {
              if (ap_ip_valid) {
                snprintf(
                    response.get(), response_size,
                    "{\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                    "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                    "\"softap_ip\":\"" IPSTR "\"}",
                    IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                    IP2STR(&ip_info.gw), IP2STR(&dns_info.ip.u_addr.ip4),
                    IP2STR(&ap_ip_info.ip));
              } else {
                snprintf(
                    response.get(), response_size,
                    "{\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                    "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                    "\"softap_ip\":\"未初始化\"}",
                    IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                    IP2STR(&ip_info.gw), IP2STR(&dns_info.ip.u_addr.ip4));
              }
            } else {
              // 如果获取DNS失败，返回默认值
              if (ap_ip_valid) {
                snprintf(
                    response.get(), response_size,
                    "{\"sta_ip\":\"" IPSTR "\",\"sta_subnet\":\"" IPSTR "\","
                    "\"sta_gateway\":\"" IPSTR "\",\"sta_dns\":\"" IPSTR "\","
                    "\"softap_ip\":\"" IPSTR "\"}",
                    IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                    IP2STR(&ip_info.gw), IP2STR(&ip_info.ip),
                    IP2STR(&ap_ip_info.ip));
              } else {
                snprintf(
                    response.get(), response_size,
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

        size_t response_len = strlen(response.get());

        // 检查缓冲区是否足够大
        if (ctxt->om->om_len < response_len) {
          // 尝试使用os_mbuf_append扩展缓冲区
          int rc = os_mbuf_append(ctxt->om, response.get(), response_len);
          if (rc != 0) {
            ESP_LOGE(TAG, "无法扩展BLE缓冲区，需要 %zu 字节，实际 %u 字节",
                     response_len, ctxt->om->om_len);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
          }
        } else {
          // 缓冲区足够大，直接复制数据
          memcpy(ctxt->om->om_data, response.get(), response_len);
        }

        ESP_LOGI(TAG, "IP信息: %s", response.get());
        return 0;

      } catch (const std::exception &e) {
        ESP_LOGE(TAG, "获取IP信息失败: %s", e.what());
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    nullptr)
// /api/version
READ_CHR(
    &tcode_version_uuid.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      static const char *TAG = "BLE_VERSION";

      try {
        std::string version = getBuildParameters();
        size_t version_len = version.length();

        // 检查缓冲区是否足够大
        if (ctxt->om->om_len < version_len) {
          // 尝试使用os_mbuf_append扩展缓冲区
          int rc = os_mbuf_append(ctxt->om, version.c_str(), version_len);
          if (rc != 0) {
            ESP_LOGE(TAG, "无法扩展BLE缓冲区，需要 %zu 字节，实际 %u 字节",
                     version_len, ctxt->om->om_len);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
          }
        } else {
          // 缓冲区足够大，直接复制数据
          memcpy(ctxt->om->om_data, version.c_str(), version_len);
        }

        ESP_LOGI(TAG, "版本信息读取成功");
        return 0;

      } catch (const std::exception &e) {
        ESP_LOGE(TAG, "获取版本信息失败: %s", e.what());
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    nullptr)
// /api/vol
READ_WRITE_CHR(
    &tcode_vol_uuid.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      static const char *TAG = "BLE_VOL";

      switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_READ_CHR: {
        // 对应HTTP GET /api/vol
        try {
          // 获取Voltage单例实例并读取电压
          float voltage = Voltage::getInstance()->getVoltage();

          // 分配响应缓冲区
          const size_t response_size = 32;
          std::unique_ptr<char[]> response(
              new (std::nothrow) char[response_size]);

          if (!response) {
            ESP_LOGE(TAG, "内存分配失败");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
          }

          // 格式化纯文本响应（仅包含数字）
          size_t response_len =
              snprintf(response.get(), response_size, "%.2f", voltage);

          // 检查缓冲区是否足够大
          if (ctxt->om->om_len < response_len) {
            // 尝试使用os_mbuf_append扩展缓冲区
            int rc = os_mbuf_append(ctxt->om, response.get(), response_len);
            if (rc != 0) {
              ESP_LOGE(TAG, "无法扩展BLE缓冲区，需要 %zu 字节，实际 %u 字节",
                       response_len, ctxt->om->om_len);
              return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
          } else {
            // 缓冲区足够大，直接复制数据
            memcpy(ctxt->om->om_data, response.get(), response_len);
          }

          ESP_LOGI(TAG, "电压读取成功: %.2fV", voltage);
          return 0;

        } catch (const std::exception &e) {
          ESP_LOGE(TAG, "读取电压失败: %s", e.what());
          return BLE_ATT_ERR_UNLIKELY;
        }
      }
      case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        // 对应HTTP GET /api/decoy?vol=9/12/15
        try {
          // 获取写入的数据长度
          size_t recv_size = ctxt->om->om_len;
          if (recv_size == 0) {
            ESP_LOGE(TAG, "无效的数据长度");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
          }

          // 分配缓冲区接收数据（需要额外1字节用于null终止符）
          const size_t voltage_str_size = 16;
          std::unique_ptr<char[]> voltage_str(
              new (std::nothrow) char[voltage_str_size]);

          if (!voltage_str) {
            ESP_LOGE(TAG, "内存分配失败");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
          }

          // 从BLE接收缓冲区复制数据，确保null终止
          size_t copy_size = (recv_size < voltage_str_size - 1)
                                 ? recv_size
                                 : voltage_str_size - 1;
          memcpy(voltage_str.get(), ctxt->om->om_data, copy_size);
          voltage_str.get()[copy_size] = '\0';

          // 解析电压值
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
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
          }

          // 设置电压
          bool success = Decoy::getInstance()->setVoltage(level);
          if (success) {
            ESP_LOGI(TAG, "电压设置成功: %dV", voltage_value);
            return 0;
          } else {
            ESP_LOGE(TAG, "电压设置失败: %dV", voltage_value);
            return BLE_ATT_ERR_UNLIKELY;
          }

        } catch (const std::exception &e) {
          ESP_LOGE(TAG, "设置电压异常: %s", e.what());
          return BLE_ATT_ERR_UNLIKELY;
        }
      }
      default:
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    nullptr)
// /api/restart
WRITE_CHR(
    &tcode_restart_uuid.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      static const char *TAG = "BLE_RESTART";

      switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        // 对应HTTP GET /api/restart
        // 任意写入值都会触发重启
        ESP_LOGI(TAG, "收到重启请求，正在重启设备...");
        esp_restart();
        // 注意：esp_restart() 不会立即返回，但为了编译正确，我们保留返回语句
        vTaskDelay(pdMS_TO_TICKS(1000));
        return 0;
      }
      default:
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    nullptr)
BLE_SERVICE_END()

// Handy服务注册
BLE_SERVICE(&handy_svc_uuid.u)
WRITE_CHR(
    &handy_chr_uuid1.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      static const char *TAG = "BLE_HANDY1";
      static uint8_t handy_chr_val[256] = {};
      memset(handy_chr_val, 0, sizeof(handy_chr_val));

      switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (ctxt->om->om_len <= sizeof(handy_chr_val)) {
          memcpy(handy_chr_val, ctxt->om->om_data, ctxt->om->om_len);

          // 将数据写入handy队列
          if (handy_queue != nullptr) {
            // 分配std::string对象
            std::string* data = new std::string((char*)handy_chr_val, ctxt->om->om_len);

            // 发送到队列
            if (xQueueSend(handy_queue, &data, pdMS_TO_TICKS(100)) != pdTRUE) {
              // 发送失败，释放内存
              delete data;
              ESP_LOGW(TAG, "Failed to send handy data to handy queue");
            } else {
              ESP_LOGD(TAG, "Sent handy data to queue, size: %d", ctxt->om->om_len);
            }
          } else {
            ESP_LOGW(TAG, "handy_queue is null, cannot send handy data");
          }

          return 0;
        } else {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
      default:
        ESP_LOGE(TAG, "Unsupported operation: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    nullptr)
WRITE_CHR(
    &handy_chr_uuid2.u,
    [](uint16_t conn_handle, uint16_t attr_handle,
       struct ble_gatt_access_ctxt *ctxt, void *arg) -> int {
      static const char *TAG = "BLE_HANDY2";
      static uint8_t handy_chr_val2[256] = {};
      memset(handy_chr_val2, 0, sizeof(handy_chr_val2));

      switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (ctxt->om->om_len <= sizeof(handy_chr_val2)) {
          memcpy(handy_chr_val2, ctxt->om->om_data, ctxt->om->om_len);

          // 将数据写入handy队列
          if (handy_queue != nullptr) {
            // 分配std::string对象
            std::string* data = new std::string((char*)handy_chr_val2, ctxt->om->om_len);

            // 发送到队列
            if (xQueueSend(handy_queue, &data, pdMS_TO_TICKS(100)) != pdTRUE) {
              // 发送失败，释放内存
              delete data;
              ESP_LOGW(TAG, "Failed to send handy data to handy queue");
            } else {
              ESP_LOGD(TAG, "Sent handy data to queue, size: %d", ctxt->om->om_len);
            }
          } else {
            ESP_LOGW(TAG, "handy_queue is null, cannot send handy data");
          }

          return 0;
        } else {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
      default:
        ESP_LOGE(TAG, "Unsupported operation: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
      }
    },
    nullptr)
BLE_SERVICE_END()

#endif