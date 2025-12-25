#include "http/static_file_handler.hpp"
#include <esp_heap_caps.h>
#include <esp_vfs.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* TAG = "static_file_handler";

// StaticFileHandler构造函数
StaticFileHandler::StaticFileHandler() : buffer(nullptr), buffer_size(0) {
    buffer_size = 8192;
    buffer = (char*)malloc(buffer_size);
    if (!buffer) {
        ESP_LOGE(TAG, "无法分配内存缓冲区");
    }
}

// StaticFileHandler析构函数
StaticFileHandler::~StaticFileHandler() {
    if (buffer) {
        heap_caps_free(buffer);
        buffer = nullptr;
        ESP_LOGI(TAG, "释放了文件传输缓冲区");
    }
}

// StaticFileHandler单例实现
StaticFileHandler& StaticFileHandler::getInstance() {
    static StaticFileHandler instance;
    return instance;
}

// 设置静态文件的基础路径
void StaticFileHandler::setBasePath(const std::string& base_path) {
    this->base_path = base_path;
    ESP_LOGI(TAG, "设置静态文件基础路径: %s", base_path.c_str());
}

// 获取当前设置的基础路径
const std::string& StaticFileHandler::getBasePath() const {
    return base_path;
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

    // 构建完整的文件路径
    std::string file_path;
    if (base_path.empty()) {
        file_path = DEFAULT_BASE_PATH + decoded_uri;
    } else {
        file_path = base_path + decoded_uri;
    }

    // 如果请求的是目录，尝试添加index.html
    struct stat st;
    if (stat(file_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        if (file_path.back() != '/') {
            file_path += '/';
        }
        file_path += "index.html";
        ESP_LOGI(TAG, "目录请求，尝试访问: %s", file_path.c_str());
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
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");

    // 添加跨域头
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // 检查缓冲区是否可用
    if (!buffer || buffer_size == 0) {
        ESP_LOGE(TAG, "缓冲区不可用");
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    // 分块读取并发送文件内容
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
        esp_err_t ret = httpd_resp_send_chunk(req, buffer, bytes_read);
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