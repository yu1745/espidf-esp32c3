#include "setting.hpp"
#include "pb_encode.h"
#include "pb_decode.h"
#include "esp_log.h"
#include <stdexcept>
#include <cstring>
#include "proto/setting.pb.h"
#include "stdio.h"
#include "setting_config.hpp"

const char* SettingWrapper::TAG = "SettingWrapper";

Setting default_setting = {
    {
        "ZTE-Y55AcX",       // ssid (Wifi.ssid)
        "asdk7788",       // password (Wifi.password)
        0,        // static_ip (Wifi.static_ip)
        0,        // gateway (Wifi.gateway)
        0,        // dns (Wifi.dns)
        true,     // enable_soft_ap (Wifi.enable_soft_ap)
        "ESP32",  // soft_ap_ssid (Wifi.soft_ap_ssid)
        "",       // soft_ap_password (Wifi.soft_ap_password)
        0,        // soft_ap_ip (Wifi.soft_ap_ip) //deprecated
        8000,     // tcp_port
        8000      // udp_port
    },
    {}  // 伺服配置将在下面通过applyServoConfig函数设置
    ,
    {0, 0, 0},      // Temperature (deprecated)
    {"tcode"},      // mDNS.name
    {true},         // led
    {4, 6, 7, -1},  // decoy
    {
        // zdt
        0.02,   // Kp
        0.0,    // Ki
        0.0,    // Kd
        0,      // IMax
        false,  // useAutoHome
        {
            100,   // home_speed
            5000,  // home_timeout
            50,    // home_collide_speed
            300,   // home_collide_current
            10,    // home_collide_time
            0,     // home_wait_time
        },
        false  // enable_debug
    }};

esp_err_t setting_init() {
    const char* TAG = "setting_init";
    ESP_LOGI(TAG, "初始化 Setting 模块");

    // 尝试打开文件进行读取，以检查文件是否存在
    FILE* file = fopen(SETTING_FILE_PATH, "rb");
    if (file != nullptr) {
        // //打印文件大小
        // fseek(file, 0, SEEK_END);
        // long file_size = ftell(file);
        // fseek(file, 0, SEEK_SET);
        // ESP_LOGI(TAG, "文件大小: %ld 字节", file_size);
        // 文件存在，关闭文件
        fclose(file);
        ESP_LOGI(TAG, "配置文件已存在: %s", SETTING_FILE_PATH);
        return ESP_OK;
    }

    // 文件不存在，创建默认配置
    ESP_LOGI(TAG, "配置文件不存在，创建默认配置: %s", SETTING_FILE_PATH);

    try {
        // 创建默认的 SettingWrapper 对象
        SettingWrapper default_setting_wrapper(default_setting);
        default_setting_wrapper.get()->servo = getDefaultServoConfig();

        // 保存到文件
        default_setting_wrapper.saveToFile(SETTING_FILE_PATH);

        ESP_LOGI(TAG, "默认配置已保存到文件");
        return ESP_OK;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "创建默认配置失败: %s", e.what());
        return ESP_FAIL;
    }
}

SettingWrapper::SettingWrapper() {
    try {
        m_setting = std::make_unique<Setting>();
        memset(m_setting.get(), 0, sizeof(Setting));
        // 使用 nanopb 提供的零初始化宏
        *m_setting = Setting_init_zero;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "构造函数异常: %s", e.what());
        m_setting.reset();
        throw;
    }
}

SettingWrapper::SettingWrapper(const uint8_t* data, size_t size) {
    try {
        m_setting = std::make_unique<Setting>();
        decode(data, size);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "从字节数组构造异常: %s", e.what());
        m_setting.reset();
        throw;
    }
}

SettingWrapper::SettingWrapper(const Setting& setting) {
    try {
        m_setting = std::make_unique<Setting>();
        memcpy(m_setting.get(), &setting, sizeof(Setting));
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "从 Setting 实例构造异常: %s", e.what());
        m_setting.reset();
        throw;
    }
}

SettingWrapper::SettingWrapper(const SettingWrapper& other) {
    try {
        m_setting = std::make_unique<Setting>();
        memcpy(m_setting.get(), other.m_setting.get(), sizeof(Setting));
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "拷贝构造函数异常: %s", e.what());
        m_setting.reset();
        throw;
    }
}

SettingWrapper::SettingWrapper(SettingWrapper&& other) noexcept
    : m_setting(std::move(other.m_setting)) {
}

SettingWrapper& SettingWrapper::operator=(const SettingWrapper& other) {
    if (this != &other) {
        try {
            auto new_setting = std::make_unique<Setting>();
            memcpy(new_setting.get(), other.m_setting.get(), sizeof(Setting));
            m_setting = std::move(new_setting);
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "拷贝赋值异常: %s", e.what());
            throw;
        }
    }
    return *this;
}

SettingWrapper& SettingWrapper::operator=(SettingWrapper&& other) noexcept {
    if (this != &other) {
        m_setting = std::move(other.m_setting);
    }
    return *this;
}

size_t SettingWrapper::encode(uint8_t* buffer, size_t size) const {
    if (!m_setting) {
        ESP_LOGE(TAG, "Setting 结构体为空");
        throw std::runtime_error("Setting 结构体为空");
    }

    if (buffer == nullptr) {
        ESP_LOGE(TAG, "输出缓冲区为空");
        throw std::runtime_error("输出缓冲区为空");
    }

    if (size < Setting_size) {
        ESP_LOGE(TAG, "缓冲区大小不足，需要至少 %zu 字节，实际 %zu 字节", 
                 static_cast<size_t>(Setting_size), size);
        throw std::runtime_error("缓冲区大小不足");
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, size);

    if (!pb_encode(&stream, &Setting_msg, m_setting.get())) {
        ESP_LOGE(TAG, "编码失败: %s", PB_GET_ERROR(&stream));
        throw std::runtime_error(PB_GET_ERROR(&stream));
    }

    ESP_LOGI(TAG, "编码成功，大小: %zu 字节", static_cast<size_t>(stream.bytes_written));
    
    return static_cast<size_t>(stream.bytes_written);
}

void SettingWrapper::decode(const uint8_t* data, size_t size) {
    if (!m_setting) {
        ESP_LOGE(TAG, "Setting 结构体为空");
        throw std::runtime_error("Setting 结构体为空");
    }

    if (data == nullptr) {
        ESP_LOGE(TAG, "输入数据为空");
        throw std::runtime_error("输入数据为空");
    }

    if (size == 0) {
        ESP_LOGE(TAG, "输入数据大小为 0");
        throw std::runtime_error("输入数据大小为 0");
    }

    // 先清零
    memset(m_setting.get(), 0, sizeof(Setting));

    pb_istream_t stream = pb_istream_from_buffer(data, size);

    if (!pb_decode(&stream, &Setting_msg, m_setting.get())) {
        ESP_LOGE(TAG, "解码失败: %s", PB_GET_ERROR(&stream));
        throw std::runtime_error(PB_GET_ERROR(&stream));
    }

    ESP_LOGI(TAG, "解码成功，大小: %zu 字节", size);
}

Setting* SettingWrapper::get() {
    return m_setting.get();
}

const Setting* SettingWrapper::get() const {
    return m_setting.get();
}

Setting* SettingWrapper::operator->() {
    return m_setting.get();
}

const Setting* SettingWrapper::operator->() const {
    return m_setting.get();
}

Setting& SettingWrapper::operator*() {
    return *m_setting;
}

const Setting& SettingWrapper::operator*() const {
    return *m_setting;
}

void SettingWrapper::reset() {
    if (m_setting) {
        memset(m_setting.get(), 0, sizeof(Setting));
        *m_setting = Setting_init_zero;
        ESP_LOGI(TAG, "Setting 结构体已重置");
    }
}

bool SettingWrapper::isValid() const {
    return m_setting != nullptr;
}

void SettingWrapper::loadFromFile(const char* filepath) {
    if (filepath == nullptr) {
        ESP_LOGE(TAG, "文件路径为空");
        throw std::runtime_error("文件路径为空");
    }

    FILE* file = fopen(filepath, "rb");
    if (file == nullptr) {
        ESP_LOGE(TAG, "无法打开文件 %s 进行读取", filepath);
        throw std::runtime_error("无法打开文件");
    }

    try {
        // 获取文件大小
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (file_size <= 0) {
            ESP_LOGE(TAG, "文件 %s 大小无效: %ld", filepath, file_size);
            fclose(file);
            throw std::runtime_error("文件大小无效");
        }

        ESP_LOGI(TAG, "文件 %s 大小: %ld 字节", filepath, file_size);

        // 分配缓冲区读取文件内容
        std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(file_size);

        size_t bytes_read = fread(buffer.get(), 1, file_size, file);
        if (bytes_read != static_cast<size_t>(file_size)) {
            ESP_LOGE(TAG, "读取文件不完整: 期望 %ld 字节, 实际 %zu 字节", file_size, bytes_read);
            fclose(file);
            throw std::runtime_error("读取文件不完整");
        }

        fclose(file);

        // 使用已有的 decode 方法解码
        decode(buffer.get(), bytes_read);

        ESP_LOGI(TAG, "成功从文件 %s 加载配置", filepath);
    } catch (const std::exception& e) {
        if (file != nullptr) {
            fclose(file);
        }
        ESP_LOGE(TAG, "从文件加载配置失败: %s", e.what());
        throw;
    }
}

void SettingWrapper::saveToFile(const char* filepath) const {
    if (!m_setting) {
        ESP_LOGE(TAG, "Setting 结构体为空");
        throw std::runtime_error("Setting 结构体为空");
    }

    if (filepath == nullptr) {
        ESP_LOGE(TAG, "文件路径为空");
        throw std::runtime_error("文件路径为空");
    }

    // 分配缓冲区进行编码
    size_t buffer_size = getMaxEncodeSize();
    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(buffer_size);

    try {
        // 编码数据
        size_t encoded_size = encode(buffer.get(), buffer_size);

        // 打开文件进行写入
        FILE* file = fopen(filepath, "wb");
        if (file == nullptr) {
            ESP_LOGE(TAG, "无法打开文件 %s 进行写入", filepath);
            throw std::runtime_error("无法打开文件");
        }

        // 写入编码后的数据
        size_t bytes_written = fwrite(buffer.get(), 1, encoded_size, file);
        if (bytes_written != encoded_size) {
            ESP_LOGE(TAG, "写入文件不完整: 期望 %zu 字节, 实际 %zu 字节", encoded_size, bytes_written);
            fclose(file);
            throw std::runtime_error("写入文件不完整");
        }

        // 确保数据刷新到磁盘
        fflush(file);
        fclose(file);

        ESP_LOGI(TAG, "成功保存配置到文件 %s, 大小: %zu 字节", filepath, encoded_size);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "保存配置到文件失败: %s", e.what());
        throw;
    }
}

void SettingWrapper::printServoSetting() const {
    if (!m_setting) {
        ESP_LOGE(TAG, "Setting 结构体为空，无法打印 Servo 配置");
        return;
    }

    const Setting_Servo &servo = m_setting->servo;

    ESP_LOGI(TAG, "========== Servo 配置 ==========");
    
    // 打印引脚配置
    ESP_LOGI(TAG, "引脚配置:");
    ESP_LOGI(TAG, "  A_SERVO_PIN: %d", servo.A_SERVO_PIN);
    ESP_LOGI(TAG, "  B_SERVO_PIN: %d", servo.B_SERVO_PIN);
    ESP_LOGI(TAG, "  C_SERVO_PIN: %d", servo.C_SERVO_PIN);
    ESP_LOGI(TAG, "  D_SERVO_PIN: %d", servo.D_SERVO_PIN);
    ESP_LOGI(TAG, "  E_SERVO_PIN: %d", servo.E_SERVO_PIN);
    ESP_LOGI(TAG, "  F_SERVO_PIN: %d", servo.F_SERVO_PIN);
    ESP_LOGI(TAG, "  G_SERVO_PIN: %d", servo.G_SERVO_PIN);

    // 打印 PWM 频率配置
    ESP_LOGI(TAG, "PWM 频率配置:");
    ESP_LOGI(TAG, "  A_SERVO_PWM_FREQ: %d Hz", servo.A_SERVO_PWM_FREQ);
    ESP_LOGI(TAG, "  B_SERVO_PWM_FREQ: %d Hz", servo.B_SERVO_PWM_FREQ);
    ESP_LOGI(TAG, "  C_SERVO_PWM_FREQ: %d Hz", servo.C_SERVO_PWM_FREQ);
    ESP_LOGI(TAG, "  D_SERVO_PWM_FREQ: %d Hz", servo.D_SERVO_PWM_FREQ);
    ESP_LOGI(TAG, "  E_SERVO_PWM_FREQ: %d Hz", servo.E_SERVO_PWM_FREQ);
    ESP_LOGI(TAG, "  F_SERVO_PWM_FREQ: %d Hz", servo.F_SERVO_PWM_FREQ);
    ESP_LOGI(TAG, "  G_SERVO_PWM_FREQ: %d Hz", servo.G_SERVO_PWM_FREQ);

    // 打印零点配置
    ESP_LOGI(TAG, "零点配置:");
    ESP_LOGI(TAG, "  A_SERVO_ZERO: %d", servo.A_SERVO_ZERO);
    ESP_LOGI(TAG, "  B_SERVO_ZERO: %d", servo.B_SERVO_ZERO);
    ESP_LOGI(TAG, "  C_SERVO_ZERO: %d", servo.C_SERVO_ZERO);
    ESP_LOGI(TAG, "  D_SERVO_ZERO: %d", servo.D_SERVO_ZERO);
    ESP_LOGI(TAG, "  E_SERVO_ZERO: %d", servo.E_SERVO_ZERO);
    ESP_LOGI(TAG, "  F_SERVO_ZERO: %d", servo.F_SERVO_ZERO);
    ESP_LOGI(TAG, "  G_SERVO_ZERO: %d", servo.G_SERVO_ZERO);

    // 打印缩放配置
    ESP_LOGI(TAG, "缩放配置:");
    ESP_LOGI(TAG, "  L0_SCALE: %.3f", servo.L0_SCALE);
    ESP_LOGI(TAG, "  L1_SCALE: %.3f", servo.L1_SCALE);
    ESP_LOGI(TAG, "  L2_SCALE: %.3f", servo.L2_SCALE);
    ESP_LOGI(TAG, "  R0_SCALE: %.3f", servo.R0_SCALE);
    ESP_LOGI(TAG, "  R1_SCALE: %.3f", servo.R1_SCALE);
    ESP_LOGI(TAG, "  R2_SCALE: %.3f", servo.R2_SCALE);

    // 打印左右范围配置
    ESP_LOGI(TAG, "左右范围配置:");
    ESP_LOGI(TAG, "  L0_LEFT: %.3f, L0_RIGHT: %.3f", servo.L0_LEFT, servo.L0_RIGHT);
    ESP_LOGI(TAG, "  L1_LEFT: %.3f, L1_RIGHT: %.3f", servo.L1_LEFT, servo.L1_RIGHT);
    ESP_LOGI(TAG, "  L2_LEFT: %.3f, L2_RIGHT: %.3f", servo.L2_LEFT, servo.L2_RIGHT);
    ESP_LOGI(TAG, "  R0_LEFT: %.3f, R0_RIGHT: %.3f", servo.R0_LEFT, servo.R0_RIGHT);
    ESP_LOGI(TAG, "  R1_LEFT: %.3f, R1_RIGHT: %.3f", servo.R1_LEFT, servo.R1_RIGHT);
    ESP_LOGI(TAG, "  R2_LEFT: %.3f, R2_RIGHT: %.3f", servo.R2_LEFT, servo.R2_RIGHT);

    // 打印反向配置
    ESP_LOGI(TAG, "反向配置:");
    ESP_LOGI(TAG, "  L0_REVERSE: %s", servo.L0_REVERSE ? "true" : "false");
    ESP_LOGI(TAG, "  L1_REVERSE: %s", servo.L1_REVERSE ? "true" : "false");
    ESP_LOGI(TAG, "  L2_REVERSE: %s", servo.L2_REVERSE ? "true" : "false");
    ESP_LOGI(TAG, "  R0_REVERSE: %s", servo.R0_REVERSE ? "true" : "false");
    ESP_LOGI(TAG, "  R1_REVERSE: %s", servo.R1_REVERSE ? "true" : "false");
    ESP_LOGI(TAG, "  R2_REVERSE: %s", servo.R2_REVERSE ? "true" : "false");

    // 打印模式配置
    ESP_LOGI(TAG, "模式配置:");
    ESP_LOGI(TAG, "  MODE: %.3f", servo.MODE);

    ESP_LOGI(TAG, "================================");
}