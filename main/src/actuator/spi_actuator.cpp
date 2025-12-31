#include "actuator/spi_actuator.hpp"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <mutex>
#include <stdexcept>
#include <memory>
#include <cstring>
#include "freertos/task.h"

static const char *TAG = "SPIActuator";

namespace actuator {

SPIActuator::SPIActuator(spi_host_device_t host_id, int mosi_io_num,
                         int sclk_io_num, int cs_io_num,
                         uint32_t clock_speed_hz, float offset)
    : Actuator(offset),
      m_host_id(host_id),
      m_mosi_io_num(mosi_io_num),
      m_sclk_io_num(sclk_io_num),
      m_cs_io_num(cs_io_num),
      m_clock_speed_hz(clock_speed_hz),
      m_spi_device(nullptr),
      m_initialized(false),
      m_bus_owner(false),
      m_tx_buffer(nullptr, [](uint8_t *) {}),
      m_buffer_size(0) {
  try {
    // 初始化SPI
    if (!initSPI()) {
      ESP_LOGE(TAG, "Failed to initialize SPI actuator");
      throw std::runtime_error("Failed to initialize SPI actuator");
    }

    ESP_LOGI(TAG, "SPI actuator initialized successfully");
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Exception in SPI actuator constructor: %s", e.what());
    // 清理已分配的资源
    if (m_spi_device) {
      spi_bus_remove_device(m_spi_device);
      m_spi_device = nullptr;
    }
    if (m_bus_owner) {
      spi_bus_free(m_host_id);
      m_bus_owner = false;
    }
    // 重新抛出异常
    throw;
  }
}

SPIActuator::~SPIActuator() {
  // 移除SPI设备
  if (m_initialized && m_spi_device) {
    esp_err_t ret = spi_bus_remove_device(m_spi_device);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to remove SPI device: %s", esp_err_to_name(ret));
    } else {
      ESP_LOGD(TAG, "SPI device removed");
    }
    m_spi_device = nullptr;
  }

  // 如果拥有总线，释放总线
  if (m_bus_owner) {
    esp_err_t ret = spi_bus_free(m_host_id);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to free SPI bus: %s", esp_err_to_name(ret));
    } else {
      ESP_LOGD(TAG, "SPI bus freed");
    }
    m_bus_owner = false;
  }

  ESP_LOGI(TAG, "SPI actuator deinitialized");
}

bool SPIActuator::initSPI() {
  esp_err_t ret;

  // 配置SPI总线
  spi_bus_config_t bus_config = {};
  bus_config.mosi_io_num = m_mosi_io_num;
  bus_config.miso_io_num = -1;  // 不使用MISO
  bus_config.sclk_io_num = m_sclk_io_num;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  // 计算需要的缓冲区大小：周期3ms
  // 最大情况：3000us * clock_speed_hz / 1000000 位
  // 对于100kHz时钟：3000us * 100000 / 1000000 = 300位 = 37.5字节
  // 为了安全，增加50%的余量，并向上取整到字节
  // 使用浮点数计算以确保精度
  float max_bits_float = static_cast<float>(PERIOD_US) * m_clock_speed_hz / 1000000.0f;
  uint32_t max_bits = static_cast<uint32_t>(max_bits_float * 1.5f);  // 增加50%余量
  m_buffer_size = (max_bits + 7) / 8;  // 向上取整到字节
  if (m_buffer_size < 256) {
    m_buffer_size = 256;  // 最小256字节（频率降低后数据量减少，可以降低最小值）
  }
  // max_transfer_sz 需要足够大以容纳最大传输
  bus_config.max_transfer_sz = m_buffer_size;
  ESP_LOGI(TAG, "SPI buffer size: %zu bytes, max_transfer_sz: %zu bytes", m_buffer_size, bus_config.max_transfer_sz);

  // 分配DMA缓冲区（需要在DMA-capable内存中）
  uint8_t *buffer = static_cast<uint8_t *>(
      heap_caps_malloc(m_buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  if (!buffer) {
    ESP_LOGE(TAG, "Failed to allocate DMA buffer");
    return false;
  }
  // 使用自定义deleter的智能指针
  m_tx_buffer = std::unique_ptr<uint8_t[], void (*)(uint8_t *)>(
      buffer, [](uint8_t *p) { heap_caps_free(p); });

  // 尝试初始化SPI总线，使用DMA
  // 如果总线已经被初始化，这个调用会失败，但我们继续尝试添加设备
  ret = spi_bus_initialize(m_host_id, &bus_config, SPI_DMA_CH_AUTO);
  if (ret == ESP_OK) {
    m_bus_owner = true;
    ESP_LOGD(TAG, "SPI bus initialized");
  } else if (ret == ESP_ERR_INVALID_STATE) {
    // 总线已经被初始化，我们不是所有者
    m_bus_owner = false;
    ESP_LOGD(TAG, "SPI bus already initialized, not owner");
  } else {
    ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
    return false;
  }

  // 配置SPI设备
  spi_device_interface_config_t dev_config = {};
  dev_config.clock_source = SPI_CLK_SRC_DEFAULT;
  dev_config.command_bits = 0;
  dev_config.address_bits = 0;
  dev_config.dummy_bits = 0;
  dev_config.mode = 0;  // SPI模式0
  dev_config.duty_cycle_pos = 128;
  dev_config.cs_ena_pretrans = 0;
  dev_config.cs_ena_posttrans = 0;
  dev_config.clock_speed_hz = static_cast<int>(m_clock_speed_hz);
  dev_config.spics_io_num = m_cs_io_num;
  dev_config.flags = 0;
  dev_config.queue_size = 1;
  dev_config.pre_cb = nullptr;
  dev_config.post_cb = nullptr;

  // 添加SPI设备
  ret = spi_bus_add_device(m_host_id, &dev_config, &m_spi_device);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
    // 如果我们是总线所有者，释放总线
    if (m_bus_owner) {
      spi_bus_free(m_host_id);
      m_bus_owner = false;
    }
    return false;
  }

  m_initialized = true;
  return true;
}

void SPIActuator::setTarget(float target) {
  // 自动加上offset
  float target_with_offset = target + m_offset;

  // 限制目标值在[-1, 1]范围内
  if (target_with_offset < -1.0f) {
    m_target = -1.0f;
  } else if (target_with_offset > 1.0f) {
    m_target = 1.0f;
  } else {
    m_target = target_with_offset;
  }

  // 调用具体的执行实现
  actuate(0);
}

bool SPIActuator::actuate(int wait) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_initialized || !m_spi_device || !m_tx_buffer) {
    ESP_LOGE(TAG, "SPI not initialized");
    return false;
  }

  // 获取当前目标值
  float target = getTarget();

  // 将目标值转换为脉冲宽度
  uint32_t pulse_width_us = targetToPulseWidth(target);

  // 生成SPI时序数据
  size_t data_size = generateSPIPulseData(pulse_width_us, m_tx_buffer.get(), m_buffer_size);
  if (data_size == 0) {
    ESP_LOGE(TAG, "Failed to generate SPI pulse data");
    return false;
  }

  // 准备SPI事务
  spi_transaction_t trans = {};
  trans.length = data_size * 8;  // 转换为位数
  trans.tx_buffer = m_tx_buffer.get();
  trans.rx_buffer = nullptr;

  // 传输数据
  esp_err_t ret = spi_device_transmit(m_spi_device, &trans);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to transmit SPI data: %s", esp_err_to_name(ret));
    return false;
  }

  // 等待传输完成
  if (wait > 0) {
    // SPI传输是同步的，但我们可以等待一段时间
    vTaskDelay(pdMS_TO_TICKS(wait));
  } else if (wait == -1) {
    // 等待传输完成（SPI传输是同步的，所以这里不需要额外等待）
    // 但为了兼容性，我们可以等待一小段时间确保完成
    vTaskDelay(pdMS_TO_TICKS(25));  // 等待一个周期多一点
  }

  ESP_LOGD(TAG, "Set target %.2f to pulse width %u us", target, pulse_width_us);
  return true;
}

uint32_t SPIActuator::targetToPulseWidth(float target) {
  // 将-1到1的输入映射到500-2500us的高电平持续时间
  // 线性映射：-1 -> 500us, 0 -> 1500us, 1 -> 2500us
  // 公式：pulse_width = 1000 + target * 1000
  float pulse_width_us = 1500.0f + target * 1000.0f;

  // 限制范围在500-2500us之间
  if (pulse_width_us < MIN_PULSE_WIDTH_US) {
    pulse_width_us = MIN_PULSE_WIDTH_US;
  } else if (pulse_width_us > MAX_PULSE_WIDTH_US) {
    pulse_width_us = MAX_PULSE_WIDTH_US;
  }

  return static_cast<uint32_t>(pulse_width_us);
}

size_t SPIActuator::generateSPIPulseData(uint32_t pulse_width_us, uint8_t *buffer, size_t buffer_size) {
  if (!buffer || buffer_size == 0) {
    return 0;
  }

  // 计算每个bit的时间（微秒）
  float bit_time_us = 1000000.0f / m_clock_speed_hz;

  // 计算需要的位数
  // 格式：pulse_width高 + (剩余时间)低，总周期固定3000us
  uint32_t high_bits = static_cast<uint32_t>(pulse_width_us / bit_time_us);
  // 剩余低电平时间 = 3000 - pulse_width
  uint32_t low_us = PERIOD_US - pulse_width_us;
  uint32_t low_bits = static_cast<uint32_t>(low_us / bit_time_us);
  uint32_t total_bits = high_bits + low_bits;

  // 计算需要的字节数
  size_t total_bytes = (total_bits + 7) / 8;
  if (total_bytes > buffer_size) {
    ESP_LOGE(TAG, "Buffer too small: need %zu bytes, have %zu bytes", total_bytes, buffer_size);
    return 0;
  }
  
  // 检查是否超过SPI最大传输限制（应该在initSPI中设置，但这里再次检查）
  if (total_bytes > m_buffer_size) {
    ESP_LOGE(TAG, "Data size %zu bytes exceeds max_transfer_sz %zu bytes", total_bytes, m_buffer_size);
    return 0;
  }

  // 清零缓冲区（所有位都是低电平）
  memset(buffer, 0, total_bytes);

  // 计算高电平部分的起始位置（总位数）
  uint32_t high_end_bit = high_bits;

  // 按位设置高电平
  for (uint32_t bit = 0; bit < high_end_bit && bit < total_bits; bit++) {
    size_t byte_idx = bit / 8;
    uint32_t bit_idx = bit % 8;
    if (byte_idx < total_bytes) {
      buffer[byte_idx] |= (1 << bit_idx);
    }
  }

  ESP_LOGI(TAG, "Generated SPI pulse: %u us high + %u us low, %u total bits, %zu bytes (period: %u us)",
           pulse_width_us, low_us, total_bits, total_bytes, PERIOD_US);

  return total_bytes;
}

}  // namespace actuator

