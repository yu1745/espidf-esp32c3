#pragma once

#include <cstdint>
#include <string>
#include "esp_timer.h"
#include "esp_log.h"

#ifdef __cplusplus

/**
 * @brief TCode命令
 */
struct TCodeComand {
    char axisType;         // 第一步匹配的轴
    char axisNum;          // 第二步匹配的单个数字
    uint16_t axisvalue;    // 第三步匹配的数字序列
    char extendType;       // 第四步匹配的字母
    uint16_t extendValue;  // 第五步匹配的数字序列
    uint64_t receiveTime;  // 接收时间戳（微秒）
};

/**
 * @brief TCode匹配器类
 * 用于匹配特定模式的字符串：字母+数字+数字序列+字母+数字序列
 */
class TCode {
   public:
    TCode() {
        // 初始化 L0_last
        L0_last.axisType = 'L';
        L0_last.axisNum = '0';
        L0_last.axisvalue = 5;
        L0_last.extendType = '\0';
        L0_last.extendValue = 0;
        L0_last.receiveTime = 0;
        
        // 初始化 L0_current
        L0_current.axisType = 'L';
        L0_current.axisNum = '0';
        L0_current.axisvalue = 5;
        L0_current.extendType = '\0';
        L0_current.extendValue = 0;
        L0_current.receiveTime = 0;
        
        // 初始化 L1_last
        L1_last.axisType = 'L';
        L1_last.axisNum = '1';
        L1_last.axisvalue = 5;
        L1_last.extendType = '\0';
        L1_last.extendValue = 0;
        L1_last.receiveTime = 0;
        
        // 初始化 L1_current
        L1_current.axisType = 'L';
        L1_current.axisNum = '1';
        L1_current.axisvalue = 5;
        L1_current.extendType = '\0';
        L1_current.extendValue = 0;
        L1_current.receiveTime = 0;
        
        // 初始化 L2_last
        L2_last.axisType = 'L';
        L2_last.axisNum = '2';
        L2_last.axisvalue = 5;
        L2_last.extendType = '\0';
        L2_last.extendValue = 0;
        L2_last.receiveTime = 0;
        
        // 初始化 L2_current
        L2_current.axisType = 'L';
        L2_current.axisNum = '2';
        L2_current.axisvalue = 5;
        L2_current.extendType = '\0';
        L2_current.extendValue = 0;
        L2_current.receiveTime = 0;
        
        // 初始化 R0_last
        R0_last.axisType = 'R';
        R0_last.axisNum = '0';
        R0_last.axisvalue = 5;
        R0_last.extendType = '\0';
        R0_last.extendValue = 0;
        R0_last.receiveTime = 0;
        
        // 初始化 R0_current
        R0_current.axisType = 'R';
        R0_current.axisNum = '0';
        R0_current.axisvalue = 5;
        R0_current.extendType = '\0';
        R0_current.extendValue = 0;
        R0_current.receiveTime = 0;
        
        // 初始化 R1_last
        R1_last.axisType = 'R';
        R1_last.axisNum = '1';
        R1_last.axisvalue = 5;
        R1_last.extendType = '\0';
        R1_last.extendValue = 0;
        R1_last.receiveTime = 0;
        
        // 初始化 R1_current
        R1_current.axisType = 'R';
        R1_current.axisNum = '1';
        R1_current.axisvalue = 5;
        R1_current.extendType = '\0';
        R1_current.extendValue = 0;
        R1_current.receiveTime = 0;
        
        // 初始化 R2_last
        R2_last.axisType = 'R';
        R2_last.axisNum = '2';
        R2_last.axisvalue = 5;
        R2_last.extendType = '\0';
        R2_last.extendValue = 0;
        R2_last.receiveTime = 0;
        
        // 初始化 R2_current
        R2_current.axisType = 'R';
        R2_current.axisNum = '2';
        R2_current.axisvalue = 5;
        R2_current.extendType = '\0';
        R2_current.extendValue = 0;
        R2_current.receiveTime = 0;
    }
    ~TCode() = default;

    /**
     * @brief 匹配字符串
     * @param input 要匹配的字符串引用
     * @return 匹配结果结构体
     * 支持格式：字母+数字+数字序列（必需）+ 字母+数字序列（可选）
     */
    TCodeComand match(const std::string& input) {
        TCodeComand result = {0};
        size_t i = 0;

        // 第一步：匹配轴类型（字母）
        if (i < input.length() && is_letter(input[i])) {
            result.axisType = input[i++];
        }

        // 第二步：匹配轴编号（单个数字）
        if (i < input.length() && is_digit(input[i])) {
            result.axisNum = input[i++];
        }

        // 第三步：匹配轴值（数字序列）
        uint16_t axisValue = 0;
        while (i < input.length() && is_digit(input[i])) {
            axisValue = axisValue * 10 + char_to_digit(input[i++]);
        }
        result.axisvalue = axisValue;

        // 第四步：匹配扩展类型（字母，可选）
        if (i < input.length() && is_letter(input[i])) {
            result.extendType = input[i++];
            
            // 第五步：匹配扩展值（数字序列，可选）
            uint16_t extendValue = 0;
            while (i < input.length() && is_digit(input[i])) {
                extendValue = extendValue * 10 + char_to_digit(input[i++]);
            }
            result.extendValue = extendValue;
        } else {
            // 如果没有扩展类型，则扩展类型和扩展值保持默认值
            result.extendType = '\0';
            result.extendValue = 0;
        }

        return result;
    }

    /**
     * @brief 预处理函数
     * @param input 要处理的字符串引用
     * 将字符串按空格分开，逐个调用match，将结果赋值到对应属性
     */
    void preprocess(const std::string& input) {
        ESP_LOGI("TCode", "preprocess: %s", input.c_str());
        std::string token;
        size_t start = 0;
        size_t end = input.find(' ');

        while (end != std::string::npos) {
            token = input.substr(start, end - start);
            processToken(token);
            start = end + 1;
            end = input.find(' ', start);
        }

        // 处理最后一个token
        token = input.substr(start);
        if (!token.empty()) {
            processToken(token);
        }
        ESP_LOGI("TCode", "postprocess: %s", tostring().c_str());
    }

    /**
     * @brief 处理单个token
     * @param token 要处理的token
     */
    void processToken(const std::string& token) {
        TCodeComand result = match(token);
        char type = result.axisType;
        char num = result.axisNum;
        
        // 获取当前时间戳（微秒）
        result.receiveTime = static_cast<uint64_t>(esp_timer_get_time());

        if (type == 'L' || type == 'l') {
            switch (num) {
                case '0':
                    L0_last = L0_current;
                    L0_current = result;
                    break;
                case '1':
                    L1_last = L1_current;
                    L1_current = result;
                    break;
                case '2':
                    L2_last = L2_current;
                    L2_current = result;
                    break;
            }
        } else if (type == 'R' || type == 'r') {
            switch (num) {
                case '0':
                    R0_last = R0_current;
                    R0_current = result;
                    break;
                case '1':
                    R1_last = R1_current;
                    R1_current = result;
                    break;
                case '2':
                    R2_last = R2_current;
                    R2_current = result;
                    break;
            }
        }
    }

   private:
    // 内联函数用于字符类型检查
    inline bool is_letter(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

    // 内联函数用于数字转换
    inline uint32_t char_to_digit(char c) {
        return static_cast<uint32_t>(c - '0');
    }
    TCodeComand L0_last;
    TCodeComand L0_current;
    TCodeComand L1_last;
    TCodeComand L1_current;
    TCodeComand L2_last;
    TCodeComand L2_current;
    TCodeComand R0_last;
    TCodeComand R0_current;
    TCodeComand R1_last;
    TCodeComand R1_current;
    TCodeComand R2_last;
    TCodeComand R2_current;
    // 添加tostring方法，用于调试输出当前TCode状态
    std::string tostring() const {
        char buffer[256];
        // 这里只展示当前current状态，last可视需要补充
        snprintf(
            buffer, sizeof(buffer),
            "L0: %u L1: %u L2: %u | R0: %u R1: %u R2: %u",
            L0_current.axisvalue, L1_current.axisvalue, L2_current.axisvalue,
            R0_current.axisvalue, R1_current.axisvalue, R2_current.axisvalue
        );
        return std::string(buffer);
    }
    void print() const {
        ESP_LOGI("TCode", "%s", tostring().c_str());
    }
};

#endif