#include "http/static_file_handler.hpp"
#include "esp_log.h"
#include <esp_vfs.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <algorithm>

static const char* TAG = "static_file_handler";

// 初始化静态成员变量
std::atomic<size_t> StaticFileHandler::max_allocated_memory{0};

// StaticFileHandler单例实现
StaticFileHandler& StaticFileHandler::getInstance() {
    static StaticFileHandler instance;
    return instance;
}

// 获取同时分配的最大内存
size_t StaticFileHandler::getMaxAllocatedMemory() {
    return max_allocated_memory.load(std::memory_order_relaxed);
}

// 根据文件扩展名获取MIME类型
const char* StaticFileHandler::getMimeType(const std::string& filename) {
    // 查找最后一个点的位置
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";  // 默认MIME类型
    }

    std::string extension = filename.substr(dot_pos + 1);

    // 转换为小写
    for (char& c : extension) {
        c = tolower(c);
    }

    // 常见文件扩展名对应的MIME类型
    if (extension == "html" || extension == "htm") {
        return "text/html";
    } else if (extension == "css") {
        return "text/css";
    } else if (extension == "js") {
        return "application/javascript";
    } else if (extension == "json") {
        return "application/json";
    } else if (extension == "xml") {
        return "application/xml";
    } else if (extension == "txt") {
        return "text/plain";
    } else if (extension == "jpg" || extension == "jpeg") {
        return "image/jpeg";
    } else if (extension == "png") {
        return "image/png";
    } else if (extension == "gif") {
        return "image/gif";
    } else if (extension == "svg") {
        return "image/svg+xml";
    } else if (extension == "ico") {
        return "image/x-icon";
    } else if (extension == "pdf") {
        return "application/pdf";
    } else if (extension == "zip") {
        return "application/zip";
    } else if (extension == "mp3") {
        return "audio/mpeg";
    } else if (extension == "mp4") {
        return "video/mp4";
    } else if (extension == "woff") {
        return "font/woff";
    } else if (extension == "woff2") {
        return "font/woff2";
    } else if (extension == "ttf") {
        return "font/ttf";
    } else if (extension == "eot") {
        return "application/vnd.ms-fontobject";
    } else {
        return "application/octet-stream";  // 默认MIME类型
    }
}

// URL解码函数
std::string StaticFileHandler::urlDecode(const std::string& src) {
    std::string decoded;
    decoded.reserve(src.length());
    for (size_t i = 0; i < src.length(); ++i) {
        if (src[i] == '%' && i + 2 < src.length()) {
            // 解码%XX格式的字符
            int hex;
            if (sscanf(&src[i + 1], "%2x", &hex) == 1) {
                decoded += static_cast<char>(hex);
                i += 2;
            } else {
                decoded += src[i];
            }
        } else if (src[i] == '+') {
            // '+'解码为空格
            decoded += ' ';
        } else {
            decoded += src[i];
        }
    }
    return decoded;
}

// 静态文件HTTP处理器函数
esp_err_t StaticFileHandler::handleStaticFile(httpd_req_t* req) {
    if (!req) {
        ESP_LOGE(TAG, "请求结构体为空");
        return ESP_FAIL;
    }

    // 获取请求的URI
    std::string uri(req->uri);
    ESP_LOGI(TAG, "请求静态文件: %s", uri.c_str());

    // URL解码
    std::string decoded_uri = urlDecode(uri);

    // 构建完整的文件路径，固定使用 DEFAULT_BASE_PATH
    std::string file_path = DEFAULT_BASE_PATH;

    struct stat st;

    // 特殊处理根路径 /
    if (decoded_uri == "/" || decoded_uri.empty()) {
        if (file_path.back() != '/') {
            file_path += '/';
        }
        file_path += "index.html";
        ESP_LOGI(TAG, "根路径请求，尝试访问: %s", file_path.c_str());
    } else {
        // 处理其他路径
        file_path += decoded_uri;

        // 如果请求的是目录，尝试添加index.html
        if (stat(file_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            if (file_path.back() != '/') {
                file_path += '/';
            }
            file_path += "index.html";
            ESP_LOGI(TAG, "目录请求，尝试访问: %s", file_path.c_str());
        }
    }

    // 检查文件是否存在
    if (stat(file_path.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "文件不存在: %s", file_path.c_str());

        // 发送404响应
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "text/html");
        const char* not_found_content =
            "<html><body>"
            "<h1>404 - 文件未找到</h1>"
            "<p>请求的文件不存在</p>"
            "</body></html>";
        httpd_resp_send(req, not_found_content, strlen(not_found_content));
        return ESP_OK;
    }

    // 检查是否为常规文件
    if (!S_ISREG(st.st_mode)) {
        ESP_LOGE(TAG, "不是常规文件: %s", file_path.c_str());

        // 发送403响应
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_set_type(req, "text/html");
        const char* forbidden_content =
            "<html><body>"
            "<h1>403 - 禁止访问</h1>"
            "<p>无法访问请求的资源</p>"
            "</body></html>";
        httpd_resp_send(req, forbidden_content, strlen(forbidden_content));
        return ESP_OK;
    }

    // 根据文件大小线性调整缓冲区，最大8KB
    // 小文件使用较小缓冲区，大文件使用8KB缓冲区
    const size_t MAX_BUFFER_SIZE = 8192;  // 8KB
    const size_t MIN_BUFFER_SIZE = 512;    // 512B
    const size_t BUFFER_SCALE_THRESHOLD = 102400;  // 100KB，超过此大小使用最大缓冲区
    size_t buffer_size;

    if (st.st_size <= BUFFER_SCALE_THRESHOLD) {
        // 线性调整：512B 到 8KB 之间按比例
        buffer_size = MIN_BUFFER_SIZE +
                     (MAX_BUFFER_SIZE - MIN_BUFFER_SIZE) * st.st_size / BUFFER_SCALE_THRESHOLD;
    } else {
        // 大文件使用最大缓冲区
        buffer_size = MAX_BUFFER_SIZE;
    }

    // 确保至少是MIN_BUFFER_SIZE，最多是MAX_BUFFER_SIZE
    buffer_size = std::max(MIN_BUFFER_SIZE, std::min(MAX_BUFFER_SIZE, buffer_size));

    ESP_LOGI(TAG, "文件大小: %ld bytes, 使用缓冲区: %zu bytes", st.st_size, buffer_size);

    // 为每个请求分配独立的缓冲区
    std::unique_ptr<char[]> buffer(new char[buffer_size]);

    // 更新同时分配的最大内存记录（只能上涨不能下跌）
    size_t expected = max_allocated_memory.load(std::memory_order_relaxed);
    while (expected < buffer_size) {
        if (max_allocated_memory.compare_exchange_weak(expected, buffer_size,
                                                       std::memory_order_relaxed,
                                                       std::memory_order_relaxed)) {
            ESP_LOGI(TAG, "更新最大分配内存: %zu bytes", buffer_size);
            break;
        }
    }

    // 打开文件
    FILE* file = fopen(file_path.c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "无法打开文件: %s", file_path.c_str());

        // 发送500响应
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/html");
        const char* error_content =
            "<html><body>"
            "<h1>500 - 内部服务器错误</h1>"
            "<p>无法打开请求的文件</p>"
            "</body></html>";
        httpd_resp_send(req, error_content, strlen(error_content));
        return ESP_OK;
    }

    // 设置响应头
    httpd_resp_set_type(req, getMimeType(file_path));

    // 添加缓存控制头
    // httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");

    // 添加跨域头
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // 为.js和.css文件添加gzip编码头
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string extension = file_path.substr(dot_pos + 1);
        // 转换为小写
        for (char& c : extension) {
            c = tolower(c);
        }
        if (extension == "js" || extension == "css") {
            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            ESP_LOGI(TAG, "为%s文件添加gzip编码头", extension.c_str());
        }
    }

    // 分块读取并发送文件内容（使用本地独立的缓冲区）
    size_t bytes_read;
    while ((bytes_read = fread(buffer.get(), 1, buffer_size, file)) > 0) {
        esp_err_t ret = httpd_resp_send_chunk(req, buffer.get(), bytes_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "发送文件块失败: %s", esp_err_to_name(ret));
            fclose(file);
            return ret;
        }
    }

    // 关闭文件
    fclose(file);

    // 发送结束块
    httpd_resp_send_chunk(req, nullptr, 0);

    ESP_LOGI(TAG, "成功发送文件: %s (%zu bytes)", file_path.c_str(),
             st.st_size);
    return ESP_OK;
}

// 便捷的静态文件处理器函数
esp_err_t static_file_handler(httpd_req_t* req) {
    return StaticFileHandler::getInstance().handleStaticFile(req);
}