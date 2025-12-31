#pragma once

#include "driver/gpio.h"
#include "mutex"
#include "soc/gpio_num.h"
#include "voltage.hpp"

/**
 * @brief 电压等级枚举
 */
enum class VoltageLevel {
  V9 = 1,  // 9V
  V12 = 2, // 12V
  V15 = 3  // 15V
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化Decoy（电压控制模块）
 *
 * @return esp_err_t
 *         - ESP_OK: 初始化成功
 *         - ESP_FAIL: 初始化失败
 */
esp_err_t decoy_init(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief 电压控制类（单例模式）
 *
 * 通过控制 MOD1、MOD2 引脚来实现电压控制
 * IO6: MOD1 控制
 * IO7: MOD2 控制
 * MOD3: 电路上拉固定为高电平
 *
 * 电压读取功能由 Voltage 类提供
 */
class Decoy {
public:
  /**
   * @brief 获取单例实例
   * @return Decoy单例实例的指针
   */
  static Decoy *getInstance();

  /**
   * @brief 析构函数
   */
  ~Decoy();

  /**
   * @brief 设置目标电压
   * @param level 目标电压等级
   * @return true 设置成功，false 设置失败
   */
  bool setVoltage(VoltageLevel level);

  // 删除拷贝构造和拷贝赋值
  Decoy(const Decoy &) = delete;
  Decoy &operator=(const Decoy &) = delete;

private:
  /**
   * @brief 初始化 GPIO
   * @return true 初始化成功，false 初始化失败
   */
  bool initGPIO();

  /**
   * @brief 从配置文件加载引脚配置
   * @return true 加载成功，false 加载失败
   */
  bool loadPinConfig();

  // GPIO 引脚定义（从配置文件动态加载）
  gpio_num_t m_pin_mod1; // MOD1 控制引脚
  gpio_num_t m_pin_mod2; // MOD2 控制引脚

  // 电压组合配置表 [MOD1, MOD2, MOD3]
  // 使用 VoltageLevel 枚举值作为数组下标访问
  static constexpr bool VOLTAGE_CONFIGS[4][3] = {
      {0, 1, 1}, // V9:  9V   (MOD1=0, MOD2=1, MOD3=1)
      {1, 0, 1}, // V12: 12V   (MOD1=1, MOD2=0, MOD3=1)
      {0, 0, 1}  // V15: 15V   (MOD1=0, MOD2=0, MOD3=1)
  };

  std::mutex m_mutex;         // 互斥锁
  bool m_initialized = false; // 初始化状态

  // 私有构造函数（单例模式）
  Decoy();
};
