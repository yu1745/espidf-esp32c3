#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化mDNS服务
 *
 * 从Setting配置中读取mDNS名称、TCP和UDP端口号，并添加HTTP、TCP和UDP服务
 */
esp_err_t init_mdns();

#ifdef __cplusplus
}
#endif
