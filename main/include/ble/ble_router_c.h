#ifndef BLE_ROUTER_C_H
#define BLE_ROUTER_C_H

#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

// C接口声明
extern const struct ble_gatt_svc_def* ble_router_get_gatt_services(void);

#ifdef __cplusplus
}
#endif

#endif  // BLE_ROUTER_C_H