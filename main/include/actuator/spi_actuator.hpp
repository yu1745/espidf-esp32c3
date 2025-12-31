#pragma once

#include <mutex>
#include <memory>
#include "actuator/actuator.hpp"
#include "driver/spi_master.h"

namespace actuator {

/**
 * @brief SPI执行器实现类
 *
 * 使用SPI外设实现执行器输出，将-1到1的输入映射到SPI数据输出
 * 适合控制需要SPI通信的执行器设备
 */
class SPIActuator : public Actuator {
   public:
    /**
     * @brief 构造函数
     * @param host_id SPI主机ID (SPI2_HOST 或 SPI3_HOST)
     * @param mosi_io_num MOSI引脚号
     * @param sclk_io_num SCLK引脚号，如果不需要可设为-1
     * @param cs_io_num CS引脚号，如果不需要可设为-1
     * @param clock_speed_hz SPI时钟速度，单位Hz
     * @param offset 偏移量，默认为0.0f
     * @throws std::exception 初始化失败时抛出异常
     */
    SPIActuator(spi_host_device_t host_id, int mosi_io_num, int sclk_io_num = -1,
                int cs_io_num = -1, uint32_t clock_speed_hz = 100000,
                float offset = 0.0f);

    /**
     * @brief 析构函数
     * 自动释放SPI资源
     */
    virtual ~SPIActuator();

    /**
     * @brief 设置执行器目标值
     * @param target 目标值，范围[-1, 1]
     */
    void setTarget(float target) override;

   protected:
    /**
     * @brief 执行器输出实现
     * @param wait 等待时间，单位ms，0表示不等待，-1表示等待直到完成
     * @return true 输出成功，false 输出失败
     */
    bool actuate(int wait = 0) override;

   private:
    spi_host_device_t m_host_id;        // SPI主机ID
    int m_mosi_io_num;                  // MOSI引脚号
    int m_sclk_io_num;                  // SCLK引脚号
    int m_cs_io_num;                     // CS引脚号
    uint32_t m_clock_speed_hz;          // SPI时钟速度
    spi_device_handle_t m_spi_device;   // SPI设备句柄
    std::mutex m_mutex;                  // 互斥锁
    bool m_initialized;                  // 初始化标志
    bool m_bus_owner;                    // 是否拥有总线（需要释放总线）
    std::unique_ptr<uint8_t[], void (*)(uint8_t *)> m_tx_buffer;  // DMA传输缓冲区（使用自定义deleter）
    size_t m_buffer_size;                // 缓冲区大小

    /**
     * @brief 初始化SPI
     * @return true 初始化成功，false 初始化失败
     */
    bool initSPI();

    /**
     * @brief 将目标值(-1到1)转换为脉冲宽度（微秒）
     * @param target 目标值，范围[-1, 1]
     * @return 高电平持续时间（微秒），范围0-2000
     */
    uint32_t targetToPulseWidth(float target);

    /**
     * @brief 生成SPI时序数据
     * @param pulse_width_us 高电平持续时间（微秒），范围0-2000
     * @param buffer 输出缓冲区
     * @param buffer_size 缓冲区大小（字节）
     * @return 实际使用的字节数
     */
    size_t generateSPIPulseData(uint32_t pulse_width_us, uint8_t *buffer, size_t buffer_size);

    static constexpr uint32_t PERIOD_US = 3000;  // 周期3ms（3000微秒）
    static constexpr uint32_t MIN_PULSE_WIDTH_US = 500;    // 最小高电平宽度500us
    static constexpr uint32_t MAX_PULSE_WIDTH_US = 2500; // 最大高电平宽度2000us
};

}  // namespace actuator

