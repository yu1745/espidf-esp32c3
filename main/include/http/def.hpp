#ifdef __cplusplus
#include "esp_system.h"
#include "http_router.hpp"
#include "static_file_handler.hpp"
GET("/hello", [](httpd_req_t* req) -> esp_err_t {
    httpd_resp_send(req, "Hello, World!", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
})

GET("/hello2", [](httpd_req_t* req) -> esp_err_t {
    httpd_resp_send(req, "Hello, World!2 ", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
})

GET("/api/mem", [](httpd_req_t* req) -> esp_err_t {
    char resp[128];
    snprintf(resp, sizeof(resp), "可用堆内存: %d, 总堆内存: %d",
             (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             (int)heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
})

GET("/api/tasks", [](httpd_req_t* req) -> esp_err_t {
    // 分配足够的缓冲区来存储任务列表
    // 每个任务大约需要40字节，假设最多有20个任务
    const size_t taskListBufferSize = 20 * 40;
    char* taskListBuffer = (char*)malloc(taskListBufferSize);
    
    if (taskListBuffer == nullptr) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 获取任务列表
    vTaskList(taskListBuffer);
    
    // 发送响应
    httpd_resp_send(req, taskListBuffer, HTTPD_RESP_USE_STRLEN);
    
    // 释放缓冲区
    free(taskListBuffer);
    
    return ESP_OK;
})

// 静态文件处理器 - 使用通配符匹配所有路径
GET("*", static_file_handler)

#endif
