#include "uart/uart2.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "globals.hpp"
#include "select_thread.hpp"
#include <fcntl.h>
#include <sdkconfig.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 使用UART1作为UART2
#define UART2_NUM UART_NUM_1

namespace {
const char *TAG = "uart2";
const int UART2_BUF_SIZE = 512;
static int uart2_fd = -1; // UART2文件描述符
#ifdef CONFIG_SERVO_MODE_SR6_SHALI
// const int UART2_RX_PIN = 8;
// const int UART2_TX_PIN = 9;
const int UART2_RX_PIN = 20;
const int UART2_TX_PIN = 21;
#else
const int UART2_RX_PIN = 20;
const int UART2_TX_PIN = 21;
#endif

// UART2接收缓冲区，用于按换行符分帧
static uint8_t uart2_rx_buffer[UART2_BUF_SIZE];
static size_t uart2_rx_len = 0;
} // namespace

// UART2初始化函数
esp_err_t uart2_init() {
  // 配置UART2 (UART_NUM_1)
  uart_config_t uart2_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
  };

  ESP_ERROR_CHECK(uart_param_config(UART2_NUM, &uart2_config));

  // 设置UART2引脚
  ESP_ERROR_CHECK(uart_set_pin(UART2_NUM, UART2_TX_PIN, UART2_RX_PIN,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  // 安装UART2驱动
  ESP_ERROR_CHECK(uart_driver_install(UART2_NUM, UART2_BUF_SIZE * 2,
                                      UART2_BUF_SIZE * 2, 0, NULL, 0));
  // 为UART2注册VFS驱动
  uart_vfs_dev_register();
  // 为UART2设置VFS驱动
  uart_vfs_dev_use_driver(UART2_NUM);
  // 使用非阻塞模式, 用于select
  uart_vfs_dev_use_nonblocking(UART2_NUM);

  // 打开UART2设备文件以获取文件描述符
  char uart2_path[32];
  snprintf(uart2_path, sizeof(uart2_path), "/dev/uart/%d", UART2_NUM);
  uart2_fd = open(uart2_path, O_RDWR | O_NONBLOCK);
  if (uart2_fd < 0) {
    ESP_LOGE(TAG, "Failed to open UART2 device: %s", strerror(errno));
    return ESP_ERR_INVALID_STATE;
  } else {
    ESP_LOGI(TAG, "UART2 initialized: fd=%d (RX: %d, TX: %d, 115200 8N1)",
             uart2_fd, UART2_RX_PIN, UART2_TX_PIN);
  }
  // ESP_LOGI(TAG, "UART2 initialized with rx=%d, tx=%d", UART2_RX_PIN,
  //          UART2_TX_PIN);
  return ESP_OK;
}

// 获取UART2文件描述符供select使用
int get_uart2_fd(void) { return uart2_fd; }

// 通过UART2发送数据
esp_err_t uart2_send_response(const char *data, size_t len) {
  if (uart2_fd < 0) {
    ESP_LOGE(TAG, "UART2 file descriptor is invalid");
    return ESP_ERR_INVALID_STATE;
  }

  if (data == nullptr || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  ssize_t bytes_written = write(uart2_fd, data, len);
  if (bytes_written < 0) {
    ESP_LOGE(TAG, "Failed to write to UART2: %s", strerror(errno));
    return ESP_FAIL;
  }

  if (bytes_written != static_cast<ssize_t>(len)) {
    ESP_LOGW(TAG, "Partial UART2 write: %zd/%zu bytes", bytes_written, len);
  }

  return ESP_OK;
}

// 处理UART2数据读取和按换行符分帧
void uart2_handle_data(void) {
  if (uart2_fd < 0) {
    ESP_LOGE(TAG, "UART2 file descriptor is invalid");
    return;
  }

  uint8_t temp_buf[UART2_BUF_SIZE];
  ssize_t bytes_read = read(uart2_fd, temp_buf, sizeof(temp_buf));

  if (bytes_read > 0) {
    // 追加新数据到接收缓冲区
    if (uart2_rx_len + bytes_read > UART2_BUF_SIZE) {
      ESP_LOGW(TAG, "UART2 buffer overflow, discarding %zu bytes",
               uart2_rx_len + bytes_read - UART2_BUF_SIZE);
      bytes_read = UART2_BUF_SIZE - uart2_rx_len;
    }
    memcpy(uart2_rx_buffer + uart2_rx_len, temp_buf, bytes_read);
    uart2_rx_len += bytes_read;

    // ESP_LOGI(TAG, "UART2 received %zd bytes, buffer size: %zu", bytes_read,
    //          uart2_rx_len);

    // 按换行符分割数据并处理完整行
    size_t cmd_start = 0;
    for (size_t i = 0; i < uart2_rx_len; i++) {
      // 检测换行符 (\n)
      if (uart2_rx_buffer[i] == '\n') {
        // 找到完整命令行
        size_t cmd_len = i - cmd_start;

        // 如果行首有 \r，去掉它（处理 \r\n 的情况）
        if (cmd_len > 0 && uart2_rx_buffer[cmd_start] == '\r') {
          cmd_start++;
          cmd_len--;
        }

        // 如果有有效数据，发送到全局队列
        if (cmd_len > 0) {
          data_packet_t *packet =
              (data_packet_t *)malloc(sizeof(data_packet_t));
          if (packet != NULL) {
            packet->source = DATA_SOURCE_UART2;
            packet->client_fd = -1;
            packet->user_data = NULL;
            packet->data = (uint8_t *)malloc(cmd_len);
            if (packet->data != NULL) {
              memcpy(packet->data, uart2_rx_buffer + cmd_start, cmd_len);
              packet->length = cmd_len;

              // 调试日志：打印完整命令
              // ESP_LOGI(TAG, "UART2 complete command: %.*s", (int)cmd_len,
              //          packet->data);

              if (xQueueSend(global_rx_queue, &packet, pdMS_TO_TICKS(10)) !=
                  pdTRUE) {
                ESP_LOGW(TAG, "Failed to send UART2 data to global queue");
                free(packet->data);
                free(packet);
              }
            } else {
              ESP_LOGE(TAG, "Failed to allocate memory for UART2 data");
              free(packet);
            }
          } else {
            ESP_LOGE(TAG, "Failed to allocate memory for UART2 packet");
          }
        }

        // 移动到下一行的起始位置
        cmd_start = i + 1;
      }
    }

    // 移动未处理的数据（不完整行）到缓冲区开头
    if (cmd_start > 0 && cmd_start < uart2_rx_len) {
      size_t remaining = uart2_rx_len - cmd_start;
      memmove(uart2_rx_buffer, uart2_rx_buffer + cmd_start, remaining);
      uart2_rx_len = remaining;
      // ESP_LOGI(TAG, "UART2 remaining data in buffer: %zu bytes", remaining);
    } else if (cmd_start >= uart2_rx_len) {
      // 所有数据都已处理
      uart2_rx_len = 0;
    }

  } else if (bytes_read < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGE(TAG, "UART2 read error: %s", strerror(errno));
    }
  }
}
