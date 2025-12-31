#include "ble/ble_router.hpp"
#include "ble/ble_router_c.h"
#include "ble/common.h"
#include "esp_log.h"
#include <cstring>
#include <map>
#include <stdio.h>


namespace {
const char *TAG = "BLE_ROUTER";

// 比较两个UUID是否相等（通过字符串比较）
static bool ble_uuid_equal(const ble_uuid_t *uuid1, const ble_uuid_t *uuid2) {
  if (uuid1 == uuid2) {
    return true;
  }
  if (uuid1 == nullptr || uuid2 == nullptr) {
    return false;
  }
  
  char str1[37];
  char str2[37];
  ble_uuid_to_str(uuid1, str1);
  ble_uuid_to_str(uuid2, str2);
  return strcmp(str1, str2) == 0;
}
}

// 全局存储C++处理函数的映射
static std::map<const void *, ble_handler_func_t> g_handler_map;

// C风格访问函数的包装器
extern "C" {
static int ble_access_wrapper(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
  auto it = g_handler_map.find(arg);
  if (it != g_handler_map.end()) {
    return it->second(conn_handle, attr_handle, ctxt, nullptr);
  }
  return BLE_ATT_ERR_UNLIKELY;
}
}

BleRouter &BleRouter::getInstance() {
  static BleRouter instance;
  return instance;
}

void BleRouter::registerService(const ble_uuid_t *uuid) {
  ble_service_t service;
  service.uuid = uuid;
  services.push_back(service);

  // 记录服务注册日志
  if (uuid->type == BLE_UUID_TYPE_16) {
    char uuid_str[10];
    ble_uuid_to_str(uuid, uuid_str);
    ESP_LOGI(TAG, "注册BLE服务: UUID=%s", uuid_str);
  } else if (uuid->type == BLE_UUID_TYPE_32) {
    char uuid_str[10];
    ble_uuid_to_str(uuid, uuid_str);
    ESP_LOGI(TAG, "注册BLE服务: UUID=%s", uuid_str);
  } else {
    // 128
    char uuid_str[33];
    ble_uuid_to_str(uuid, uuid_str);
    ESP_LOGI(TAG, "注册BLE服务: UUID=%s", uuid_str);
  }
}

void BleRouter::registerCharacteristic(const ble_uuid_t *uuid,
                                       ble_handler_func_t access_cb,
                                       uint8_t flags, uint16_t *val_handle) {
  if (services.empty()) {
    ESP_LOGE(TAG, "No service registered before characteristic");
    return;
  }
  ble_characteristic_t characteristic;
  characteristic.uuid = uuid;
  characteristic.access_cb = access_cb;
  characteristic.flags = flags;
  characteristic.val_handle = val_handle;

  services.back().characteristics.push_back(characteristic);

  // 记录特征注册日志
  if (uuid->type == BLE_UUID_TYPE_16) {
    char uuid_str[10];
    ble_uuid_to_str(uuid, uuid_str);
    ESP_LOGI(TAG, "注册BLE特征: UUID=%s, 标志=0x%02X", uuid_str, flags);
  } else if (uuid->type == BLE_UUID_TYPE_32) {
    char uuid_str[10];
    ble_uuid_to_str(uuid, uuid_str);
    ESP_LOGI(TAG, "注册BLE特征: UUID=%s, 标志=0x%02X", uuid_str, flags);
  } else {
    // 128
    char uuid_str[33];
    ble_uuid_to_str(uuid, uuid_str);
    ESP_LOGI(TAG, "注册BLE特征: UUID=%s, 标志=0x%02X", uuid_str, flags);
  }
}

void BleRouter::finishCurrentService() {
  // 当前服务已经完成，无需特殊处理
  // 下一个registerService调用将开始新服务
}

const struct ble_gatt_svc_def *BleRouter::getGattServices() {
  ESP_LOGI(TAG, "getGattServices() called");
  // 清空之前的结果
  gatt_services.clear();
  gatt_characteristics.clear();
  g_handler_map.clear();

  // 为每个服务创建GATT定义
  for (const auto &service : services) {
    // 为当前服务的特征创建定义数组
    std::vector<struct ble_gatt_chr_def> chr_defs;

    // 添加所有特征
    for (const auto &characteristic : service.characteristics) {
      struct ble_gatt_chr_def chr_def = {0};
      chr_def.uuid = characteristic.uuid;

      // 存储C++处理函数到全局映射
      const void *handler_key = characteristic.uuid;
      g_handler_map[handler_key] = characteristic.access_cb;

      // 使用C风格包装函数
      chr_def.access_cb = ble_access_wrapper;
      chr_def.flags = characteristic.flags;
      chr_def.val_handle = characteristic.val_handle;
      chr_def.arg = const_cast<void *>(handler_key); // 使用UUID作为键

      chr_defs.push_back(chr_def);
    }

    // 添加结束标记
    struct ble_gatt_chr_def end_chr = {0};
    end_chr.uuid = NULL;
    end_chr.access_cb = NULL;
    end_chr.flags = 0;
    end_chr.val_handle = NULL;
    end_chr.arg = NULL;
    chr_defs.push_back(end_chr);

    // 保存特征定义
    gatt_characteristics.push_back(chr_defs);

    // 创建服务定义
    struct ble_gatt_svc_def svc_def = {0};
    svc_def.type = BLE_GATT_SVC_TYPE_PRIMARY;
    svc_def.uuid = service.uuid;
    svc_def.characteristics = &gatt_characteristics.back()[0];

    gatt_services.push_back(svc_def);
  }

  // 添加结束标记
  struct ble_gatt_svc_def end_svc = {0};
  end_svc.type = 0;
  end_svc.uuid = NULL;
  end_svc.characteristics = NULL;
  gatt_services.push_back(end_svc);
  
  // 打印服务和特征值树形结构
  dumpServicesTree();
  
  return gatt_services.data();
}

void BleRouter::dumpServicesTree() const {
  ESP_LOGI(TAG, "GATT服务树:");
  size_t total_characteristics = 0;
  
  // 检测重复的服务UUID
  for (size_t i = 0; i < services.size(); ++i) {
    for (size_t j = i + 1; j < services.size(); ++j) {
      if (ble_uuid_equal(services[i].uuid, services[j].uuid)) {
        char uuid_str[37];
        ble_uuid_to_str(services[i].uuid, uuid_str);
        ESP_LOGW(TAG, "警告: 检测到重复的服务UUID [%zu]和[%zu]: %s", 
                 i, j, uuid_str);
      }
    }
  }
  
  for (size_t i = 0; i < services.size(); ++i) {
    const auto &service = services[i];
    total_characteristics += service.characteristics.size();
    
    // 打印服务UUID
    char svc_uuid_str[37];
    ble_uuid_to_str(service.uuid, svc_uuid_str);
    
    // 判断是否是最后一个服务
    const char *prefix = (i == services.size() - 1) ? "└─" : "├─";
    ESP_LOGI(TAG, "%s 服务[%zu] (UUID: %s)", prefix, i, svc_uuid_str);
    
    // 检测重复的特征UUID（在同一服务内）
    for (size_t j = 0; j < service.characteristics.size(); ++j) {
      for (size_t k = j + 1; k < service.characteristics.size(); ++k) {
        if (ble_uuid_equal(service.characteristics[j].uuid, 
                           service.characteristics[k].uuid)) {
          char chr_uuid_str[37];
          ble_uuid_to_str(service.characteristics[j].uuid, chr_uuid_str);
          ESP_LOGW(TAG, "警告: 服务[%zu]中检测到重复的特征UUID [%zu]和[%zu]: %s", 
                   i, j, k, chr_uuid_str);
        }
      }
    }
    
    // 打印特征值
    for (size_t j = 0; j < service.characteristics.size(); ++j) {
      const auto &chr = service.characteristics[j];
      char chr_uuid_str[37];
      ble_uuid_to_str(chr.uuid, chr_uuid_str);
      
      // 判断是否是最后一个特征值
      bool is_last_chr = (j == service.characteristics.size() - 1);
      bool is_last_svc = (i == services.size() - 1);
      const char *chr_prefix = is_last_chr 
        ? (is_last_svc ? "   └─" : "│  └─")
        : (is_last_svc ? "   ├─" : "│  ├─");
      
      ESP_LOGI(TAG, "%s 特征[%zu] (UUID: %s, flags: 0x%02X)", 
               chr_prefix, j, chr_uuid_str, chr.flags);
    }
  }
  
  ESP_LOGI(TAG, "统计: 服务数量=%zu, 特征值总数=%zu", 
           services.size(), total_characteristics);
}

size_t BleRouter::getServiceCount() const { return services.size(); }

const ble_uuid_t** BleRouter::getServiceUuids(size_t* count) const {
  static const ble_uuid_t* uuid_ptrs[16]; // 最多支持16个服务
  size_t service_count = services.size();
  
  if (service_count > 16) {
    service_count = 16; // 限制最大数量
  }
  
  for (size_t i = 0; i < service_count; ++i) {
    uuid_ptrs[i] = services[i].uuid;
  }
  
  *count = service_count;
  return uuid_ptrs;
}

void BleRouter::clearServices() {
  services.clear();
  gatt_services.clear();
  gatt_characteristics.clear();
  g_handler_map.clear();
  is_valid = false;
}

// C接口实现
extern "C" {
const struct ble_gatt_svc_def *ble_router_get_gatt_services(void) {
  return BleRouter::getInstance().getGattServices();
}

int ble_router_get_service_uuids(const ble_uuid_t*** uuids, size_t* count) {
  if (uuids == nullptr || count == nullptr) {
    return -1;
  }
  
  const ble_uuid_t** result = BleRouter::getInstance().getServiceUuids(count);
  *uuids = result;
  return 0;
}
}