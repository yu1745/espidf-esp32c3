#include "spiffs.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

namespace {
const char* TAG = "spiffs";
}  // namespace

esp_err_t spiffs_init(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs",
                                  .partition_label = NULL,  // 使用默认分区标签
                                  .max_files = 5,
                                  .format_if_mount_failed = true};

    // 注册并挂载 SPIFFS 到 VFS
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",
                     esp_err_to_name(ret));
        }
        return ret;
    }

    // 检查 SPIFFS 是否已挂载
    if (!esp_spiffs_mounted(conf.partition_label)) {
        ESP_LOGE(TAG, "SPIFFS not mounted");
        return ESP_FAIL;
    }

    // 获取 SPIFFS 信息
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
                 esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    ESP_LOGI(TAG, "SPIFFS initialized successfully");
    return ESP_OK;
}