#pragma once

#ifdef __cplusplus
#include "ble_router.hpp"
#include "esp_system.h"
#include "host/ble_uuid.h"

// 心率服务定义
// static const ble_uuid16_t heart_rate_svc_uuid = BLE_UUID16_INIT(0x180D);
// static uint16_t heart_rate_chr_val_handle;
// static const ble_uuid16_t heart_rate_chr_uuid = BLE_UUID16_INIT(0x2A37);

// // 自动化IO服务定义
// static const ble_uuid16_t auto_io_svc_uuid = BLE_UUID16_INIT(0x1815);
// static uint16_t led_chr_val_handle;
// static const ble_uuid128_t led_chr_uuid = BLE_UUID128_INIT(0x23,
//                                                            0xd1,
//                                                            0xbc,
//                                                            0xea,
//                                                            0x5f,
//                                                            0x78,
//                                                            0x23,
//                                                            0x15,
//                                                            0xde,
//                                                            0xef,
//                                                            0x12,
//                                                            0x12,
//                                                            0x25,
//                                                            0x15,
//                                                            0x00,
//                                                            0x00);

// 心率服务注册
// BLE_SERVICE(&heart_rate_svc_uuid.u)
// INDICATE_CHR(
//     &heart_rate_chr_uuid.u,
//     [](uint16_t conn_handle,
//        uint16_t attr_handle,
//        struct ble_gatt_access_ctxt* ctxt,
//        void* arg) -> int {
//         // 心率特征访问处理
//         static uint8_t heart_rate_chr_val[2] = {0};

//         switch (ctxt->op) {
//             case BLE_GATT_ACCESS_OP_READ_CHR:
//                 // 更新心率值
//                 heart_rate_chr_val[1] = 75;  // 示例心率值
//                 return os_mbuf_append(ctxt->om, &heart_rate_chr_val,
//                                       sizeof(heart_rate_chr_val)) == 0
//                            ? 0
//                            : BLE_ATT_ERR_INSUFFICIENT_RES;
//             default:
//                 return BLE_ATT_ERR_UNLIKELY;
//         }
//     },
//     &heart_rate_chr_val_handle)
// BLE_SERVICE_END()

// 自动化IO服务注册
// BLE_SERVICE(&auto_io_svc_uuid.u)
// WRITE_CHR(
//     &led_chr_uuid.u,
//     [](uint16_t conn_handle,
//        uint16_t attr_handle,
//        struct ble_gatt_access_ctxt* ctxt,
//        void* arg) -> int {
//         // LED特征访问处理
//         switch (ctxt->op) {
//             case BLE_GATT_ACCESS_OP_WRITE_CHR:
//                 if (ctxt->om->om_len == 1) {
//                     if (ctxt->om->om_data[0]) {
//                         ESP_LOGI("NimBLE_GATT_Server", "LED turned on!");
//                     } else {
//                         ESP_LOGI("NimBLE_GATT_Server", "LED turned off!");
//                     }
//                     return 0;
//                 } else {
//                     return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
//                 }
//             default:
//                 return BLE_ATT_ERR_UNLIKELY;
//         }
//     },
//     &led_chr_val_handle)
// BLE_SERVICE_END()

#endif