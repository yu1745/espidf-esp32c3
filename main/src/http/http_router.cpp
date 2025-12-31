#include "http/http_router.hpp"
#include <cstdlib>
#include <cstring>
#include "esp_timer.h"

static const char* TAG = "http_router";

// HttpRouter单例实现
HttpRouter& HttpRouter::getInstance() {
    static HttpRouter instance;
    return instance;
}

// 注册HTTP端点
void HttpRouter::registerEndpoint(const std::string& uri,
                                  httpd_method_t method,
                                  http_handler_func_t handler) {
    http_endpoint_t endpoint;
    endpoint.uri = uri;
    endpoint.method = method;
    endpoint.handler = handler;

    endpoints.push_back(endpoint);
    ESP_LOGI(TAG, "注册端点: %s [%d]", uri.c_str(), method);
}

// 静态处理器存储
static std::vector<http_handler_func_t> g_handler_storage;
static std::vector<http_endpoint_t> g_endpoint_storage;

// OPTIONS请求处理器 - 处理跨域预检请求
static esp_err_t http_options_handler(httpd_req_t* req) {
    // 设置跨域响应头
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods",
                       "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers",
                       "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");

    // 返回200状态码
    httpd_resp_send(req, NULL, 0);
    ESP_LOGI(TAG, "处理OPTIONS预检请求: %s", req->uri);
    return ESP_OK;
}

// 静态C风格处理器函数
static esp_err_t http_request_handler(httpd_req_t* req) {
    size_t index = (size_t)req->user_ctx;
    if (index < g_handler_storage.size()) {
        // 设置跨域响应头
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Methods",
                           "GET, POST, PUT, DELETE, OPTIONS");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers",
                           "Content-Type, Authorization");

        auto now = static_cast<int>(esp_timer_get_time());
        auto ret = g_handler_storage[index](req);
        ESP_LOGI(TAG, "处理端点 %s [%d] 耗时: %.1f ms",
                 g_endpoint_storage[index].uri.c_str(),
                 g_endpoint_storage[index].method,
                 (esp_timer_get_time() - now) / 1000.0f);
        return ret;
    }
    ESP_LOGE(TAG, "无效的处理器索引: %zu", index);
    return ESP_FAIL;
}

// 将所有注册的端点注册到HTTP服务器
esp_err_t HttpRouter::registerAllEndpoints(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "服务器句柄为空");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "开始注册 %zu 个HTTP端点", endpoints.size());

    // 清空并重新填充静态存储
    g_handler_storage.clear();
    g_endpoint_storage.clear();
    g_handler_storage.reserve(endpoints.size());
    g_endpoint_storage.reserve(endpoints.size());

    for (size_t i = 0; i < endpoints.size(); i++) {
        const auto& endpoint = endpoints[i];

        // 存储处理器函数和端点信息
        g_handler_storage.push_back(endpoint.handler);
        g_endpoint_storage.push_back(endpoint);

        // 创建HTTP URI处理器
        httpd_uri_t uri_handler = {0};
        uri_handler.uri = endpoint.uri.c_str();
        uri_handler.method = endpoint.method;
        uri_handler.user_ctx = (void*)i;             // 存储索引
        uri_handler.handler = http_request_handler;  // 使用静态C风格函数

        // 注册端点
        esp_err_t ret = httpd_register_uri_handler(server, &uri_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "注册端点失败: %s [%d] - %s", endpoint.uri.c_str(),
                     endpoint.method, esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "成功注册端点: %s [%d]", endpoint.uri.c_str(),
                 endpoint.method);
    }

    // 注册OPTIONS预检请求处理器，匹配所有路径
    httpd_uri_t options_handler = {0};
    options_handler.uri = "*";
    options_handler.method = HTTP_OPTIONS;
    options_handler.handler = http_options_handler;
    options_handler.user_ctx = NULL;

    esp_err_t ret = httpd_register_uri_handler(server, &options_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册OPTIONS处理器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "成功注册OPTIONS预检处理器");

    ESP_LOGI(TAG, "所有HTTP端点注册完成");
    return ESP_OK;
}

// 获取已注册端点的数量
size_t HttpRouter::getEndpointCount() const {
    return endpoints.size();
}

// 辅助函数：发送HTTP响应
esp_err_t http_send_response(httpd_req_t* req,
                             const char* content,
                             const char* content_type) {
    if (!req || !content) {
        return ESP_FAIL;
    }

    // 设置响应头
    httpd_resp_set_type(req, content_type);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods",
                       "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers",
                       "Content-Type, Authorization");

    // 发送响应体
    return httpd_resp_send(req, content, strlen(content));
}

// 辅助函数：发送JSON响应
esp_err_t http_send_json_response(httpd_req_t* req, const char* json_content) {
    return http_send_response(req, json_content, "application/json");
}

// 辅助函数：获取请求参数
std::string get_query_param(httpd_req_t* req, const char* param_name) {
    if (!req || !param_name) {
        return "";
    }

    // 获取查询字符串长度
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        return "";
    }

    // 分配缓冲区并获取查询字符串
    char* query = (char*)malloc(query_len + 1);
    if (!query) {
        ESP_LOGE(TAG, "内存分配失败");
        return "";
    }

    if (httpd_req_get_url_query_str(req, query, query_len + 1) != ESP_OK) {
        free(query);
        return "";
    }

    // 查找参数
    char param_value[128];
    esp_err_t ret = httpd_query_key_value(query, param_name, param_value,
                                          sizeof(param_value));
    free(query);

    if (ret == ESP_OK) {
        return std::string(param_value);
    }

    return "";
}

// 辅助函数：获取POST数据
std::string get_post_data(httpd_req_t* req) {
    if (!req) {
        return "";
    }

    // 获取内容长度
    size_t content_length = req->content_len;
    if (content_length == 0) {
        return "";
    }

    // 分配缓冲区
    char* post_data = (char*)malloc(content_length + 1);
    if (!post_data) {
        ESP_LOGE(TAG, "内存分配失败");
        return "";
    }

    // 读取POST数据
    size_t received = 0;
    while (received < content_length) {
        int ret = httpd_req_recv(req, post_data + received,
                                 content_length - received);
        if (ret <= 0) {
            break;
        }
        received += ret;
    }
    post_data[received] = '\0';

    std::string result(post_data);
    free(post_data);

    return result;
}