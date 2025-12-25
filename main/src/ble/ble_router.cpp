#include "ble/ble_router.hpp"
#include <cstring>
#include <map>
#include "ble/ble_router_c.h"
#include "ble/common.h"
#include "esp_log.h"

namespace {
const char* TAG = "BLE_ROUTER";
}

// 全局存储C++处理函数的映射
static std::map<const void*, ble_handler_func_t> g_handler_map;

// C风格访问函数的包装器
extern "C" {
static int ble_access_wrapper(uint16_t conn_handle,
                              uint16_t attr_handle,
                              struct ble_gatt_access_ctxt* ctxt,
                              void* arg) {
    auto it = g_handler_map.find(arg);
    if (it != g_handler_map.end()) {
        return it->second(conn_handle, attr_handle, ctxt, nullptr);
    }
    return BLE_ATT_ERR_UNLIKELY;
}
}

BleRouter& BleRouter::getInstance() {
    static BleRouter instance;
    return instance;
}

void BleRouter::registerService(const ble_uuid_t* uuid) {
    ble_service_t service;
    service.uuid = uuid;
    services.push_back(service);

    // 记录服务注册日志
    if (uuid->type == BLE_UUID_TYPE_16) {
        ESP_LOGI(TAG, "注册BLE服务: UUID=0x%04X", BLE_UUID16(uuid)->value);
    } else if (uuid->type == BLE_UUID_TYPE_32) {
        ESP_LOGI(TAG, "注册BLE服务: UUID=0x%08X", BLE_UUID32(uuid)->value);
    } else {
        // 128
        auto uuid_128 = BLE_UUID128(uuid);
        ESP_LOGI(TAG,
                 "注册BLE服务: "
                 "UUID=0x%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X"
                 "%02X%02X%02X",
                 uuid_128->value[0], uuid_128->value[1], uuid_128->value[2],
                 uuid_128->value[3], uuid_128->value[4], uuid_128->value[5],
                 uuid_128->value[6], uuid_128->value[7], uuid_128->value[8],
                 uuid_128->value[9], uuid_128->value[10], uuid_128->value[11],
                 uuid_128->value[12], uuid_128->value[13], uuid_128->value[14],
                 uuid_128->value[15]);
    }
}

void BleRouter::registerCharacteristic(const ble_uuid_t* uuid,
                                       ble_handler_func_t access_cb,
                                       uint8_t flags,
                                       uint16_t* val_handle) {
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
        ESP_LOGI(TAG, "注册BLE特征: UUID=0x%04X, 标志=0x%02X",
                 BLE_UUID16(uuid)->value, flags);
    } else if (uuid->type == BLE_UUID_TYPE_32) {
        ESP_LOGI(TAG, "注册BLE特征: UUID=0x%08X, 标志=0x%02X",
                 BLE_UUID32(uuid)->value, flags);
    } else {
        // 128
        auto uuid_128 = BLE_UUID128(uuid);
        ESP_LOGI(TAG,
                 "注册BLE特征: "
                 "UUID=%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X"
                 "%02X%02X%02X, 标志=0x%02X",
                 uuid_128->value[0], uuid_128->value[1], uuid_128->value[2],
                 uuid_128->value[3], uuid_128->value[4], uuid_128->value[5],
                 uuid_128->value[6], uuid_128->value[7], uuid_128->value[8],
                 uuid_128->value[9], uuid_128->value[10], uuid_128->value[11],
                 uuid_128->value[12], uuid_128->value[13], uuid_128->value[14],
                 uuid_128->value[15], flags);
    }
}

void BleRouter::finishCurrentService() {
    // 当前服务已经完成，无需特殊处理
    // 下一个registerService调用将开始新服务
}

const struct ble_gatt_svc_def* BleRouter::getGattServices() {
    // 清空之前的结果
    gatt_services.clear();
    gatt_characteristics.clear();
    g_handler_map.clear();

    // 为每个服务创建GATT定义
    for (const auto& service : services) {
        // 为当前服务的特征创建定义数组
        std::vector<struct ble_gatt_chr_def> chr_defs;

        // 添加所有特征
        for (const auto& characteristic : service.characteristics) {
            struct ble_gatt_chr_def chr_def = {0};
            chr_def.uuid = characteristic.uuid;

            // 存储C++处理函数到全局映射
            const void* handler_key = characteristic.uuid;
            g_handler_map[handler_key] = characteristic.access_cb;

            // 使用C风格包装函数
            chr_def.access_cb = ble_access_wrapper;
            chr_def.flags = characteristic.flags;
            chr_def.val_handle = characteristic.val_handle;
            chr_def.arg = const_cast<void*>(handler_key);  // 使用UUID作为键

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

    return gatt_services.data();
}

size_t BleRouter::getServiceCount() const {
    return services.size();
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
const struct ble_gatt_svc_def* ble_router_get_gatt_services(void) {
    return BleRouter::getInstance().getGattServices();
}
}