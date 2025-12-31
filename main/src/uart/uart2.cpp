#include "uart/uart2.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "globals.hpp"
#include "select_thread.hpp"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 使用UART1作为UART2
#define UART2_NUM UART_NUM_1 

namespace {
const char *TAG = "uart2";
const int UART2_BUF_SIZE = 1024;
static int uart2_fd = -1; // UART2文件描述符
const int UART2_RX_PIN = 20;
const int UART2_TX_PIN = 21;
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

// 处理UART2数据读取和echo
void uart2_handle_data(void) {
  if (uart2_fd < 0) {
    ESP_LOGE(TAG, "UART2 file descriptor is invalid");
    return;
  }

  uint8_t buffer[UART2_BUF_SIZE];
  ssize_t bytes_read = read(uart2_fd, buffer, sizeof(buffer) - 1);

  if (bytes_read > 0) {
    buffer[bytes_read] = '\0'; // 确保字符串结束
    ESP_LOGI(TAG, "UART2 received %zd bytes: %s", bytes_read, buffer);

    // 将数据写回UART2 (echo)
    ssize_t bytes_written = write(uart2_fd, buffer, bytes_read);
    if (bytes_written < 0) {
      ESP_LOGE(TAG, "Failed to write to UART2: %s", strerror(errno));
    } else {
      ESP_LOGI(TAG, "UART2 echoed %zd bytes", bytes_written);
    }

    // 将数据添加到全局接收队列供其他模块使用
    data_packet_t *packet = (data_packet_t *)malloc(sizeof(data_packet_t));
    if (packet != NULL) {
      packet->source = DATA_SOURCE_UART2; // 标识数据来自UART2
      packet->client_fd = -1;              // UART没有客户端文件描述符
      packet->user_data = NULL;            // UART不需要user_data
      packet->data = (uint8_t *)malloc(bytes_read);
      if (packet->data != NULL) {
        memcpy(packet->data, buffer, bytes_read);
        packet->length = bytes_read;

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
  } else if (bytes_read < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGE(TAG, "UART2 read error: %s", strerror(errno));
    }
  }
}
