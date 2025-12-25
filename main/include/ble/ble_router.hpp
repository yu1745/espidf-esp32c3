#ifndef BLE_ROUTER_H
#define BLE_ROUTER_H

#include <functional>
#include <vector>
#include "esp_log.h"
#include "host/ble_gatt.h"

#ifdef __cplusplus

// BLE处理器函数类型定义
using ble_handler_func_t = std::function<int(uint16_t conn_handle,
                                             uint16_t attr_handle,
                                             struct ble_gatt_access_ctxt* ctxt,
                                             void* arg)>;

// BLE特征信息结构体
typedef struct {
    const ble_uuid_t* uuid;
    ble_handler_func_t access_cb;
    uint8_t flags;
    uint16_t* val_handle;
} ble_characteristic_t;

// BLE服务信息结构体
typedef struct {
    const ble_uuid_t* uuid;
    std::vector<ble_characteristic_t> characteristics;
} ble_service_t;

#else

#endif  // __cplusplus

#ifdef __cplusplus

// BLE路由器类
class BleRouter {
   public:
    // 获取单例实例
    static BleRouter& getInstance();

    // 注册BLE服务
    void registerService(const ble_uuid_t* uuid);

    // 注册BLE特征到当前服务
    void registerCharacteristic(const ble_uuid_t* uuid,
                                ble_handler_func_t access_cb,
                                uint8_t flags,
                                uint16_t* val_handle = nullptr);

    // 完成当前服务的注册
    void finishCurrentService();

    // 将所有注册的服务转换为GATT服务定义数组
    const struct ble_gatt_svc_def* getGattServices();

    // 获取已注册服务的数量
    size_t getServiceCount() const;

    // 清空所有注册的服务
    void clearServices();

   private:
    BleRouter() = default;
    ~BleRouter() = default;
    BleRouter(const BleRouter&) = delete;
    BleRouter& operator=(const BleRouter&) = delete;

    std::vector<ble_service_t> services;
    std::vector<struct ble_gatt_svc_def> gatt_services;
    std::vector<std::vector<struct ble_gatt_chr_def>> gatt_characteristics;
    bool is_valid;
};

// 辅助宏：用于生成唯一的标识符
#define _GET_UNIQUE_ID() CONCAT(unique_id_, __LINE__)

// 辅助宏：用于二次展开
#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)

// 辅助宏：用于创建服务注册器结构体
#define _DEFINE_BLE_SERVICE_REGISTRAR_STRUCT(uuid, id)             \
    namespace {                                                    \
    struct CONCAT(ble_service_registrar_, id) {                    \
        CONCAT(ble_service_registrar_, id)() {                     \
            BleRouter::getInstance().registerService(uuid);        \
        }                                                          \
    };                                                             \
    }

// 辅助宏：用于创建服务注册器实例
#define _DEFINE_BLE_SERVICE_REGISTRAR_INSTANCE(id) \
    static CONCAT(ble_service_registrar_, id) CONCAT(ble_service_instance_, id);

// 辅助宏：用于创建特征注册器结构体
#define _DEFINE_BLE_CHR_REGISTRAR_STRUCT(uuid, access_cb, flags, val_handle, \
                                         id)                                 \
    namespace {                                                              \
    struct CONCAT(ble_chr_registrar_, id) {                                  \
        CONCAT(ble_chr_registrar_, id)() {                                   \
            BleRouter::getInstance().registerCharacteristic(                 \
                uuid, access_cb, flags, val_handle);                         \
        }                                                                    \
    };                                                                       \
    }

// 辅助宏：用于创建特征注册器实例
#define _DEFINE_BLE_CHR_REGISTRAR_INSTANCE(id) \
    static CONCAT(ble_chr_registrar_, id) CONCAT(ble_chr_instance_, id);

// 辅助宏：用于创建服务结束注册器结构体
#define _DEFINE_BLE_SERVICE_END_REGISTRAR_STRUCT(id)         \
    namespace {                                              \
    struct CONCAT(ble_service_end_registrar_, id) {          \
        CONCAT(ble_service_end_registrar_, id)() {           \
            BleRouter::getInstance().finishCurrentService(); \
        }                                                    \
    };                                                       \
    }

// 辅助宏：用于创建服务结束注册器实例
#define _DEFINE_BLE_SERVICE_END_REGISTRAR_INSTANCE(id) \
    static CONCAT(ble_service_end_registrar_, id)      \
        CONCAT(ble_service_end_instance_, id);

// 宏定义：BLE服务注册
#define BLE_SERVICE(uuid)                                        \
    _DEFINE_BLE_SERVICE_REGISTRAR_STRUCT(uuid, _GET_UNIQUE_ID()) \
    _DEFINE_BLE_SERVICE_REGISTRAR_INSTANCE(_GET_UNIQUE_ID())

// 宏定义：只读特征注册
#define READ_CHR(uuid, access_cb, val_handle)                              \
    _DEFINE_BLE_CHR_REGISTRAR_STRUCT(uuid, access_cb, BLE_GATT_CHR_F_READ, \
                                     val_handle, _GET_UNIQUE_ID())         \
    _DEFINE_BLE_CHR_REGISTRAR_INSTANCE(_GET_UNIQUE_ID())

// 宏定义：只写特征注册
#define WRITE_CHR(uuid, access_cb, val_handle)                              \
    _DEFINE_BLE_CHR_REGISTRAR_STRUCT(uuid, access_cb, BLE_GATT_CHR_F_WRITE, \
                                     val_handle, _GET_UNIQUE_ID())          \
    _DEFINE_BLE_CHR_REGISTRAR_INSTANCE(_GET_UNIQUE_ID())

// 宏定义：读写特征注册
#define READ_WRITE_CHR(uuid, access_cb, val_handle)                  \
    _DEFINE_BLE_CHR_REGISTRAR_STRUCT(                                \
        uuid, access_cb, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE, \
        val_handle, _GET_UNIQUE_ID())                                \
    _DEFINE_BLE_CHR_REGISTRAR_INSTANCE(_GET_UNIQUE_ID())

// 宏定义：通知特征注册
#define NOTIFY_CHR(uuid, access_cb, val_handle)                       \
    _DEFINE_BLE_CHR_REGISTRAR_STRUCT(                                 \
        uuid, access_cb, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY, \
        val_handle, _GET_UNIQUE_ID())                                 \
    _DEFINE_BLE_CHR_REGISTRAR_INSTANCE(_GET_UNIQUE_ID())

// 宏定义：指示特征注册
#define INDICATE_CHR(uuid, access_cb, val_handle)                       \
    _DEFINE_BLE_CHR_REGISTRAR_STRUCT(                                   \
        uuid, access_cb, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE, \
        val_handle, _GET_UNIQUE_ID())                                   \
    _DEFINE_BLE_CHR_REGISTRAR_INSTANCE(_GET_UNIQUE_ID())

// 宏定义：服务结束注册
#define BLE_SERVICE_END()                                      \
    _DEFINE_BLE_SERVICE_END_REGISTRAR_STRUCT(_GET_UNIQUE_ID()) \
    _DEFINE_BLE_SERVICE_END_REGISTRAR_INSTANCE(_GET_UNIQUE_ID())

#endif  // __cplusplus

#endif  // BLE_ROUTER_H