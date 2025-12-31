#pragma once

#include "esp_err.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <proto/setting.pb.h>

#define SETTING_FILE_PATH "/spiffs/setting.bin"

/**
 * @brief 初始化 Setting 模块
 * @details 当配置文件不存在时，会创建一个默认配置文件
 * @throws std::runtime_error 如果初始化失败
 */
esp_err_t setting_init();

class SettingWrapper {
public:
  /**
   * @brief 默认构造函数，创建一个初始化为零的 Setting 结构体
   */
  SettingWrapper();

  /**
   * @brief 从字节数组构造 Setting 对象
   * @param data 输入的字节数组指针
   * @param size 字节数组的大小
   * @throws std::runtime_error 如果解码失败
   */
  explicit SettingWrapper(const uint8_t *data, size_t size);

  /**
   * @brief 从 Setting 实例构造 SettingWrapper 对象
   * @param setting 输入的 Setting 实例的常量引用
   * @throws std::runtime_error 如果拷贝失败
   */
  explicit SettingWrapper(const Setting &setting);

  /**
   * @brief 拷贝构造函数
   * @param other 另一个 SettingWrapper 对象
   */
  SettingWrapper(const SettingWrapper &other);

  /**
   * @brief 移动构造函数
   * @param other 另一个 SettingWrapper 对象
   */
  SettingWrapper(SettingWrapper &&other) noexcept;

  /**
   * @brief 拷贝赋值运算符
   * @param other 另一个 SettingWrapper 对象
   * @return *this
   */
  SettingWrapper &operator=(const SettingWrapper &other);

  /**
   * @brief 移动赋值运算符
   * @param other 另一个 SettingWrapper 对象
   * @return *this
   */
  SettingWrapper &operator=(SettingWrapper &&other) noexcept;

  /**
   * @brief 析构函数
   */
  ~SettingWrapper() = default;

  /**
   * @brief 将 Setting 结构体编码为字节数组
   * @param buffer 输出缓冲区指针
   * @param size 缓冲区大小
   * @return 实际编码的字节数，如果失败返回 0
   * @throws std::runtime_error 如果编码失败
   */
  size_t encode(uint8_t *buffer, size_t size) const;

  /**
   * @brief 获取编码后数据所需的最大缓冲区大小
   * @return 最大编码大小
   */
  static constexpr size_t getMaxEncodeSize() { return Setting_size; }

  /**
   * @brief 从字节数组解码并更新 Setting 结构体
   * @param data 输入的字节数组指针
   * @param size 字节数组的大小
   * @throws std::runtime_error 如果解码失败
   */
  void decode(const uint8_t *data, size_t size);

  /**
   * @brief 获取 Setting 结构体的原始指针
   * @return Setting 结构体的指针
   */
  Setting *get();

  /**
   * @brief 获取 Setting 结构体的常量指针
   * @return Setting 结构体的常量指针
   */
  const Setting *get() const;

  /**
   * @brief 重载箭头运算符
   * @return Setting 结构体的指针
   */
  Setting *operator->();

  /**
   * @brief 重载箭头运算符（常量版本）
   * @return Setting 结构体的常量指针
   */
  const Setting *operator->() const;

  /**
   * @brief 重载解引用运算符
   * @return Setting 结构体的引用
   */
  Setting &operator*();

  /**
   * @brief 重载解引用运算符（常量版本）
   * @return Setting 结构体的常量引用
   */
  const Setting &operator*() const;

  /**
   * @brief 重置 Setting 结构体为零值
   */
  void reset();

  /**
   * @brief 验证 Setting 结构体是否有效
   * @return true 如果有效，false 如果无效
   */
  bool isValid() const;

  /**
   * @brief 从 SPIFFS 文件系统加载配置
   * @param filepath 配置文件路径
   * @throws std::runtime_error 如果文件打开或读取失败，或解码失败
   */
  void loadFromFile(const char *filepath);

  /**
   * @brief 从默认文件路径加载配置
   * @throws std::runtime_error 如果文件打开或读取失败，或解码失败
   */
  void loadFromFile() { loadFromFile(SETTING_FILE_PATH); }

  /**
   * @brief 保存配置到 SPIFFS 文件系统
   * @param filepath 配置文件路径
   * @throws std::runtime_error 如果文件打开或写入失败
   */
  void saveToFile(const char *filepath) const;

  /**
   * @brief 从默认文件路径保存配置
   * @throws std::runtime_error 如果文件打开或写入失败
   */
  void saveToFile() const { saveToFile(SETTING_FILE_PATH); };

  /**
   * @brief 打印 Servo 配置信息
   * @details 使用 ESP_LOGI 输出所有 servo 相关的配置参数
   */
  void printServoSetting() const;

private:
  std::unique_ptr<Setting> m_setting;
  static const char *TAG;
};
