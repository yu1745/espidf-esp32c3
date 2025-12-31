#ifndef BLE_ROUTER_C_H
#define BLE_ROUTER_C_H

#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

// C接口声明
extern const struct ble_gatt_svc_def* ble_router_get_gatt_services(void);

// 获取服务UUID列表（用于广告数据）
// uuids: 输出参数，指向UUID数组的指针
// count: 输出参数，UUID数量
// 返回: 成功返回0，失败返回非0
extern int ble_router_get_service_uuids(const ble_uuid_t*** uuids, size_t* count);

#ifdef __cplusplus
}
#endif

#endif  // BLE_ROUTER_C_H