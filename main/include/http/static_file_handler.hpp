#pragma once

#include <string>
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "spiffs.h"

#ifdef __cplusplus

/**
 * @brief 静态文件HTTP处理器类
 *
 * 该类提供从SPIFFS文件系统提供静态文件的功能
 * 支持8K缓冲区，自动MIME类型检测
 */
class StaticFileHandler {
   public:
    // 默认的静态文件路径前缀
    static constexpr const char* DEFAULT_BASE_PATH = "/spiffs";

    /**
     * @brief 获取单例实例
     *
     * @return StaticFileHandler& 单例引用
     */
    static StaticFileHandler& getInstance();

    /**
     * @brief 设置静态文件的基础路径
     *
     * @param base_path 基础路径，默认为"/spiffs"
     */
    void setBasePath(const std::string& base_path);

    /**
     * @brief 获取当前设置的基础路径
     *
     * @return const std::string& 基础路径
     */
    const std::string& getBasePath() const;

    /**
     * @brief 静态文件HTTP处理器函数
     *
     * @param req HTTP请求结构体
     * @return esp_err_t 处理结果
     */
    esp_err_t handleStaticFile(httpd_req_t* req);

    /**
     * @brief 根据文件扩展名获取MIME类型
     *
     * @param filename 文件名
     * @return const char* MIME类型字符串
     */
    const char* getMimeType(const std::string& filename);

    /**
     * @brief URL解码函数
     *
     * @param src 源字符串
     * @return std::string 解码后的字符串
     */
    std::string urlDecode(const std::string& src);

   private:
    StaticFileHandler();
    ~StaticFileHandler();
    StaticFileHandler(const StaticFileHandler&) = delete;
    StaticFileHandler& operator=(const StaticFileHandler&) = delete;

    std::string base_path;
    char* buffer;          // 动态分配的缓冲区
    size_t buffer_size;    // 缓冲区大小
};

/**
 * @brief 便捷的静态文件处理器函数
 *
 * 可以直接用于HTTP路由注册
 *
 * @param req HTTP请求结构体
 * @return esp_err_t 处理结果
 */
esp_err_t static_file_handler(httpd_req_t* req);

#endif  // __cplusplus