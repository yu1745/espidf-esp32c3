#pragma once

#include <string>
#include <atomic>
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus

/**
 * @brief 静态文件HTTP处理器类
 *
 * 该类提供从SPIFFS文件系统提供静态文件的功能
 * 使用每个请求独立的缓冲区，自动MIME类型检测，线程安全
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
    static const char* getMimeType(const std::string& filename);

    /**
     * @brief URL解码函数
     *
     * @param src 源字符串
     * @return std::string 解码后的字符串
     */
    static std::string urlDecode(const std::string& src);

    /**
     * @brief 获取同时分配的最大内存（字节数）
     *
     * @return size_t 最大分配内存
     */
    static size_t getMaxAllocatedMemory();

   private:
    StaticFileHandler() = default;
    ~StaticFileHandler() = default;
    StaticFileHandler(const StaticFileHandler&) = delete;
    StaticFileHandler& operator=(const StaticFileHandler&) = delete;

    // 记录同时分配的最大内存（只能上涨不能下跌）
    static std::atomic<size_t> max_allocated_memory;
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