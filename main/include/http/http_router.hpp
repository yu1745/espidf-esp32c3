#ifndef HTTP_ROUTER_H
#define HTTP_ROUTER_H

#include <esp_http_server.h>
#include <esp_log.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

#ifdef __cplusplus

// HTTP处理器函数类型定义
using http_handler_func_t = std::function<esp_err_t(httpd_req_t* req)>;

// HTTP端点信息结构体
typedef struct {
    std::string uri;
    httpd_method_t method;
    http_handler_func_t handler;
} http_endpoint_t;

#else

#endif  // __cplusplus

#ifdef __cplusplus

// HTTP路由器类
class HttpRouter {
   public:
    // 获取单例实例
    static HttpRouter& getInstance();

    // 注册HTTP端点
    void registerEndpoint(const std::string& uri,
                          httpd_method_t method,
                          http_handler_func_t handler);

    // 将所有注册的端点注册到HTTP服务器
    esp_err_t registerAllEndpoints(httpd_handle_t server);

    // 获取已注册端点的数量
    size_t getEndpointCount() const;

   private:
    HttpRouter() = default;
    ~HttpRouter() = default;
    HttpRouter(const HttpRouter&) = delete;
    HttpRouter& operator=(const HttpRouter&) = delete;

    std::vector<http_endpoint_t> endpoints;
};

// 辅助宏：用于二次展开
#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)

// 辅助宏：用于生成唯一的标识符
#define _GET_UNIQUE_ID() CONCAT(unique_id_, __LINE__)

// 辅助宏：用于创建注册器结构体
#define _DEFINE_REGISTRAR_STRUCT(method, uri, handler, id)                 \
    namespace {                                                            \
    struct CONCAT(method##_registrar_, id) {                               \
        CONCAT(method##_registrar_, id)() {                                \
            HttpRouter::getInstance().registerEndpoint(uri, HTTP_##method, \
                                                       handler);           \
        }                                                                  \
    };                                                                     \
    }

// 辅助宏：用于创建注册器实例
#define _DEFINE_REGISTRAR_INSTANCE(method, id) \
    static CONCAT(method##_registrar_, id) CONCAT(method##_instance_, id);

// 宏定义：GET端点注册
#define GET(uri, handler)                                         \
    _DEFINE_REGISTRAR_STRUCT(GET, uri, handler, _GET_UNIQUE_ID()) \
    _DEFINE_REGISTRAR_INSTANCE(GET, _GET_UNIQUE_ID())

// 宏定义：POST端点注册
#define POST(uri, handler)                                         \
    _DEFINE_REGISTRAR_STRUCT(POST, uri, handler, _GET_UNIQUE_ID()) \
    _DEFINE_REGISTRAR_INSTANCE(POST, _GET_UNIQUE_ID())

// 宏定义：PUT端点注册
#define PUT(uri, handler)                                         \
    _DEFINE_REGISTRAR_STRUCT(PUT, uri, handler, _GET_UNIQUE_ID()) \
    _DEFINE_REGISTRAR_INSTANCE(PUT, _GET_UNIQUE_ID())

// 宏定义：DELETE端点注册
#define DELETE(uri, handler)                                         \
    _DEFINE_REGISTRAR_STRUCT(DELETE, uri, handler, _GET_UNIQUE_ID()) \
    _DEFINE_REGISTRAR_INSTANCE(DELETE, _GET_UNIQUE_ID())

// 辅助函数：发送HTTP响应
esp_err_t http_send_response(httpd_req_t* req,
                             const char* content,
                             const char* content_type = "text/html");

// 辅助函数：发送JSON响应
esp_err_t http_send_json_response(httpd_req_t* req, const char* json_content);

// 辅助函数：获取请求参数
std::string get_query_param(httpd_req_t* req, const char* param_name);

// 辅助函数：获取POST数据
std::string get_post_data(httpd_req_t* req);

#endif  // __cplusplus

#endif  // HTTP_ROUTER_H