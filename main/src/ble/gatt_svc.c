/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "ble/gatt_svc.h"
#include "ble/ble_router_c.h"
#include "ble/common.h"


const static char* TAG = "GATT_SVC";

/* GATT services table - will be populated by BLE router */
static const struct ble_gatt_svc_def* gatt_svr_svcs = NULL;


/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {
        /* Service register event */
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(TAG, "registered service %s with handle=%d",
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                     ctxt->svc.handle);
            break;

        /* Characteristic register event */
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(TAG,
                     "registering characteristic %s with "
                     "def_handle=%d val_handle=%d",
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                     ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;

        /* Descriptor register event */
        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                     ctxt->dsc.handle);
            break;

        /* Unknown event */
        default:
            assert(0);
            break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update heart rate subscription status
 */

void gatt_svr_subscribe_cb(struct ble_gap_event* event) {
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    // /* Check attribute handle */
    // if (event->subscribe.attr_handle == heart_rate_chr_val_handle) {
    //     /* Update heart rate subscription status */
    //     heart_rate_chr_conn_handle = event->subscribe.conn_handle;
    //     heart_rate_chr_conn_handle_inited = true;
    //     heart_rate_ind_status = event->subscribe.cur_indicate;
    // }
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Register services using BLE router
 *      3. Update NimBLE host GATT services counter
 *      4. Add GATT services to server
 */
int gatt_svc_init(void) {
    /* Local variables */
    int rc;
    gatt_svr_svcs = ble_router_get_gatt_services();
    /* 使用BLE路由器注册服务 */
    /* 2. GATT service initialization */
    ble_svc_gatt_init();

    /* 3. Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    /* 4. Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
