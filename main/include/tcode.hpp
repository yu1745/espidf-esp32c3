#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include "esp_timer.h"
#include "esp_log.h"

#ifdef __cplusplus

/**
 * @brief TCode命令
 */
struct TCodeComand {
    char axisType;         // 第一步匹配的轴
    char axisNum;          // 第二步匹配的单个数字
    float axisvalue;       // 第三步匹配的数字序列（表示为0.xxx）
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
   // 当前TCode状态数据（供子类访问）
   TCodeComand L0_current;
   TCodeComand L1_current;
   TCodeComand L2_current;
   TCodeComand R0_current;
   TCodeComand R1_current;
   TCodeComand R2_current;
   TCodeComand L0_last;
   TCodeComand L1_last;
   TCodeComand L2_last;
   TCodeComand R0_last;
   TCodeComand R1_last;
   TCodeComand R2_last;

   /**
    * @brief 插值函数类型定义
    * 函数接受TCode对象指针，返回指向6个float的指针（顺序为：L0, L1, L2, R0, R1, R2）
    */
   using InterpolateFunc = std::function<float*(TCode*)>;

   /**
    * @brief 设置插值函数
    * @param func 插值函数
    */
   void setInterpolateFunc(InterpolateFunc func) {
       m_interpolateFunc = func;
   }

   /**
    * @brief 插值方法
    * @return 指向6个float的指针，顺序为：L0, L1, L2, R0, R1, R2
    * 调用设置的插值函数进行插值计算
    */
   float* interpolate() {
       if (m_interpolateFunc) {
           return m_interpolateFunc(this);
       }
       // 如果没有设置插值函数，返回当前值（不应该发生，因为构造函数会设置默认实现）
       m_interpolatedValues[0] = L0_current.axisvalue;
       m_interpolatedValues[1] = L1_current.axisvalue;
       m_interpolatedValues[2] = L2_current.axisvalue;
       m_interpolatedValues[3] = R0_current.axisvalue;
       m_interpolatedValues[4] = R1_current.axisvalue;
       m_interpolatedValues[5] = R2_current.axisvalue;
       return m_interpolatedValues;
   }

    TCode() : m_interpolatedValues{0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f} {
        // 设置默认插值实现
        m_interpolateFunc = defaultInterpolate;
        
        // 初始化 L0_last
        L0_last.axisType = 'L';
        L0_last.axisNum = '0';
        L0_last.axisvalue = 0.5f;
        L0_last.extendType = '\0';
        L0_last.extendValue = 0;
        L0_last.receiveTime = 0;

        // 初始化 L0_current
        L0_current.axisType = 'L';
        L0_current.axisNum = '0';
        L0_current.axisvalue = 0.5f;
        L0_current.extendType = '\0';
        L0_current.extendValue = 0;
        L0_current.receiveTime = 0;

        // 初始化 L1_last
        L1_last.axisType = 'L';
        L1_last.axisNum = '1';
        L1_last.axisvalue = 0.5f;
        L1_last.extendType = '\0';
        L1_last.extendValue = 0;
        L1_last.receiveTime = 0;

        // 初始化 L1_current
        L1_current.axisType = 'L';
        L1_current.axisNum = '1';
        L1_current.axisvalue = 0.5f;
        L1_current.extendType = '\0';
        L1_current.extendValue = 0;
        L1_current.receiveTime = 0;

        // 初始化 L2_last
        L2_last.axisType = 'L';
        L2_last.axisNum = '2';
        L2_last.axisvalue = 0.5f;
        L2_last.extendType = '\0';
        L2_last.extendValue = 0;
        L2_last.receiveTime = 0;

        // 初始化 L2_current
        L2_current.axisType = 'L';
        L2_current.axisNum = '2';
        L2_current.axisvalue = 0.5f;
        L2_current.extendType = '\0';
        L2_current.extendValue = 0;
        L2_current.receiveTime = 0;

        // 初始化 R0_last
        R0_last.axisType = 'R';
        R0_last.axisNum = '0';
        R0_last.axisvalue = 0.5f;
        R0_last.extendType = '\0';
        R0_last.extendValue = 0;
        R0_last.receiveTime = 0;

        // 初始化 R0_current
        R0_current.axisType = 'R';
        R0_current.axisNum = '0';
        R0_current.axisvalue = 0.5f;
        R0_current.extendType = '\0';
        R0_current.extendValue = 0;
        R0_current.receiveTime = 0;

        // 初始化 R1_last
        R1_last.axisType = 'R';
        R1_last.axisNum = '1';
        R1_last.axisvalue = 0.5f;
        R1_last.extendType = '\0';
        R1_last.extendValue = 0;
        R1_last.receiveTime = 0;

        // 初始化 R1_current
        R1_current.axisType = 'R';
        R1_current.axisNum = '1';
        R1_current.axisvalue = 0.5f;
        R1_current.extendType = '\0';
        R1_current.extendValue = 0;
        R1_current.receiveTime = 0;

        // 初始化 R2_last
        R2_last.axisType = 'R';
        R2_last.axisNum = '2';
        R2_last.axisvalue = 0.5f;
        R2_last.extendType = '\0';
        R2_last.extendValue = 0;
        R2_last.receiveTime = 0;

        // 初始化 R2_current
        R2_current.axisType = 'R';
        R2_current.axisNum = '2';
        R2_current.axisvalue = 0.5f;
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
        TCodeComand result = {'\0', '\0', 0.0f, '\0', 0, 0};
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
        uint32_t axisValue = 0;
        uint32_t digitCount = 0;  // 记录数字位数
        while (i < input.length() && is_digit(input[i])) {
            axisValue = axisValue * 10 + char_to_digit(input[i++]);
            digitCount++;
        }
        // 将解析到的整数转换为0.xxx的浮点数，根据数字位数决定除数
        // 例如：5（1位）-> 0.5, 50（2位）-> 0.50, 500（3位）-> 0.500
        float divisor = 1.0f;
        for (uint16_t j = 0; j < digitCount; j++) {
            divisor *= 10.0f;
        }
        result.axisvalue = static_cast<float>(axisValue) / divisor;

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
        ESP_LOGD("TCode", "postprocess: %s", tostring().c_str());
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
    // 插值结果数组，顺序为：L0, L1, L2, R0, R1, R2
    float m_interpolatedValues[6];
    
    // 插值函数
    InterpolateFunc m_interpolateFunc;

    /**
     * @brief 默认插值实现（静态方法）
     * @param tcode TCode对象指针
     * @return 指向6个float的指针，顺序为：L0, L1, L2, R0, R1, R2
     * 如果current的extendType为'I'，则在last和current之间进行插值
     * 否则直接返回current的值
     */
    static float* defaultInterpolate(TCode* tcode) {
        if (tcode == nullptr) {
            return nullptr;
        }
        
        uint64_t currentTime = static_cast<uint64_t>(esp_timer_get_time());
        
        // 处理L0
        if (tcode->L0_current.extendType == 'I') {
            tcode->m_interpolatedValues[0] = interpolateValue(
                tcode->L0_last.axisvalue, tcode->L0_current.axisvalue,
                tcode->L0_current.receiveTime, currentTime, tcode->L0_current.extendValue);
        } else {
            tcode->m_interpolatedValues[0] = tcode->L0_current.axisvalue;
        }
        
        // 处理L1
        if (tcode->L1_current.extendType == 'I') {
            tcode->m_interpolatedValues[1] = interpolateValue(
                tcode->L1_last.axisvalue, tcode->L1_current.axisvalue,
                tcode->L1_current.receiveTime, currentTime, tcode->L1_current.extendValue);
        } else {
            tcode->m_interpolatedValues[1] = tcode->L1_current.axisvalue;
        }
        
        // 处理L2
        if (tcode->L2_current.extendType == 'I') {
            tcode->m_interpolatedValues[2] = interpolateValue(
                tcode->L2_last.axisvalue, tcode->L2_current.axisvalue,
                tcode->L2_current.receiveTime, currentTime, tcode->L2_current.extendValue);
        } else {
            tcode->m_interpolatedValues[2] = tcode->L2_current.axisvalue;
        }
        
        // 处理R0
        if (tcode->R0_current.extendType == 'I') {
            tcode->m_interpolatedValues[3] = interpolateValue(
                tcode->R0_last.axisvalue, tcode->R0_current.axisvalue,
                tcode->R0_current.receiveTime, currentTime, tcode->R0_current.extendValue);
        } else {
            tcode->m_interpolatedValues[3] = tcode->R0_current.axisvalue;
        }
        
        // 处理R1
        if (tcode->R1_current.extendType == 'I') {
            tcode->m_interpolatedValues[4] = interpolateValue(
                tcode->R1_last.axisvalue, tcode->R1_current.axisvalue,
                tcode->R1_current.receiveTime, currentTime, tcode->R1_current.extendValue);
        } else {
            tcode->m_interpolatedValues[4] = tcode->R1_current.axisvalue;
        }
        
        // 处理R2
        if (tcode->R2_current.extendType == 'I') {
            tcode->m_interpolatedValues[5] = interpolateValue(
                tcode->R2_last.axisvalue, tcode->R2_current.axisvalue,
                tcode->R2_current.receiveTime, currentTime, tcode->R2_current.extendValue);
        } else {
            tcode->m_interpolatedValues[5] = tcode->R2_current.axisvalue;
        }
        
        return tcode->m_interpolatedValues;
    }

    /**
     * @brief 插值单个值（静态辅助方法）
     * @param lastValue 上一个值
     * @param currentValue 当前值
     * @param receiveTime 接收时间（微秒）
     * @param currentTime 当前时间（微秒）
     * @param duration 插值持续时间（毫秒，来自extendValue）
     * @return 插值后的值
     */
    static inline float interpolateValue(float lastValue, float currentValue,
                                  uint64_t receiveTime, uint64_t currentTime,
                                  uint16_t duration) {
        if (duration == 0) {
            // 如果持续时间为0，直接返回当前值
            return currentValue;
        }
        
        // 计算从接收时间到现在的经过时间（微秒转毫秒）
        int64_t elapsed = static_cast<int64_t>(currentTime - receiveTime);
        float elapsedMs = static_cast<float>(elapsed) / 1000.0f;
        
        // 如果已经超过插值时间，返回当前值
        if (elapsedMs >= static_cast<float>(duration)) {
            return currentValue;
        }
        
        // 计算插值比例（0.0 到 1.0）
        float t = elapsedMs / static_cast<float>(duration);
        
        // 线性插值
        return lastValue + (currentValue - lastValue) * t;
    }

    // 内联函数用于字符类型检查
    inline bool is_letter(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

    // 内联函数用于数字转换
    inline uint32_t char_to_digit(char c) {
        return static_cast<uint32_t>(c - '0');
    }
    // 添加tostring方法，用于调试输出当前TCode状态
    std::string tostring() const {
        char buffer[256];
        // 这里只展示当前current状态，last可视需要补充
        snprintf(
            buffer, sizeof(buffer),
            "L0: %.3f L1: %.3f L2: %.3f | R0: %.3f R1: %.3f R2: %.3f",
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