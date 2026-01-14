#include "handyplug/handy_handler.hpp"
#include "esp_log.h"
#include "globals.hpp"
#include "handyplug/handyplug.pb.h"
#include "pb.h"
#include "pb_decode.h"
#include "select_thread.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

static const char *TAG = "HandyHandler";

// Handy队列句柄
QueueHandle_t handy_queue = nullptr;

/**
 * @brief 生成TCode字符串
 * @param position 位置值(0.0-1.0)
 * @param duration 持续时间(毫秒)
 * @return TCode字符串，格式如 "L0500I1000"
 */
static std::string generateTCode(double position, uint32_t duration) {
  // 将0.0-1.0映射到0-999
  int value = static_cast<int>(std::round(position * 999.0));
  char cmdStr[32];
  snprintf(cmdStr, sizeof(cmdStr), "L0%dI%d", value, static_cast<int>(duration));
  return std::string(cmdStr);
}

/**
 * @brief Handy任务处理函数
 * @param arg 任务参数
 */
void handy_task(void *arg) {
  std::string *data;
  while (true) {
    if (xQueueReceive(handy_queue, &data, portMAX_DELAY) == pdTRUE) {
      auto len = data->length();
      auto pb_is =
          pb_istream_from_buffer((const pb_byte_t *)data->c_str(), len);
      handyplug_Payload payload = handyplug_Payload_init_zero;

      payload.Messages.funcs.decode = [](pb_istream_t *stream,
                                         const pb_field_t *field,
                                         void **arg) -> bool {
        while (stream->bytes_left) {
          handyplug_Message msg = handyplug_Message_init_zero;
          pb_wire_type_t wire_type;
          uint32_t tag = handyplug_Message_LinearCmd_tag;
          bool eof;
          pb_decode_tag(stream, &wire_type, &tag, &eof);

          if (!(wire_type == PB_WT_STRING &&
                tag == handyplug_Message_LinearCmd_tag)) {
            return true;
          }

          pb_istream_t *subStream = new pb_istream_t;
          pb_make_string_substream(stream, subStream);
          handyplug_LinearCmd cmd = handyplug_LinearCmd_init_zero;

          cmd.Vectors.funcs.decode = [](pb_istream_t *stream,
                                        const pb_field_t *field,
                                        void **arg) -> bool {
            while (stream->bytes_left) {
              handyplug_LinearCmd_Vector vector =
                  handyplug_LinearCmd_Vector_init_zero;
              if (!pb_decode(stream, handyplug_LinearCmd_Vector_fields,
                             &vector)) {
                ESP_LOGE(TAG, "decode handyplug_LinearCmd_Vector failed");
                return false;
              } else {
                // 生成TCode字符串
                std::string tcode =
                    generateTCode(vector.Position, vector.Duration);
                ESP_LOGD(TAG,
                         "Generated TCode: %s (Position: %.3f, Duration: %u)",
                         tcode.c_str(), vector.Position, vector.Duration);

                // 发送到全局队列
                if (global_rx_queue != nullptr) {
                  // 分配数据包内存
                  data_packet_t *packet = static_cast<data_packet_t *>(
                      malloc(sizeof(data_packet_t)));
                  if (packet == nullptr) {
                    ESP_LOGE(TAG, "Failed to allocate memory for handy packet");
                    return false;
                  }

                  packet->source = DATA_SOURCE_HANDY;
                  packet->client_fd = -1;   // HANDY没有client_fd
                  packet->user_data = NULL; // HANDY不需要user_data

                  packet->data =
                      static_cast<uint8_t *>(malloc(tcode.length() + 1));
                  if (packet->data == nullptr) {
                    ESP_LOGE(TAG, "Failed to allocate memory for handy data");
                    free(packet);
                    return false;
                  }

                  // 复制TCode字符串（包含null终止符）
                  memcpy(packet->data, tcode.c_str(), tcode.length() + 1);
                  packet->length = tcode.length() + 1;

                  // 发送到队列
                  if (xQueueSend(global_rx_queue, &packet,
                                 pdMS_TO_TICKS(100)) != pdTRUE) {
                    // 发送失败，释放内存
                    free(packet->data);
                    free(packet);
                    ESP_LOGW(TAG, "Failed to send handy data to global queue");
                    return false;
                  }

                  ESP_LOGD(TAG, "Sent TCode to global queue: %s",
                           tcode.c_str());
                } else {
                  ESP_LOGW(TAG,
                           "global_rx_queue is null, cannot send handy data");
                }
              }
            }
            return true;
          };

          if (!pb_decode(subStream, handyplug_LinearCmd_fields, &cmd)) {
            ESP_LOGE(TAG, "decode handyplug_LinearCmd failed");
          } else {
            ESP_LOGD(
                TAG,
                "decode handyplug_LinearCmd success, Id: %d, DeviceIndex: %d",
                cmd.Id, cmd.DeviceIndex);
          }
          pb_close_string_substream(stream, subStream);
          delete subStream;
        }
        return true;
      };

      if (!pb_decode(&pb_is, handyplug_Payload_fields, &payload)) {
        ESP_LOGE(TAG, "decode handyplug_Payload failed");
      } else {
        ESP_LOGD(TAG, "decode handyplug_Payload success");
      }

      delete data;
    }
  }
}

/**
 * @brief 初始化Handy处理模块
 * @return ESP_OK成功，其他值失败
 */
esp_err_t handy_handler_init(void) {
  if (handy_queue != nullptr) {
    ESP_LOGW(TAG, "Handy queue already initialized");
    return ESP_OK;
  }

  // 创建队列，最多存储10个消息指针
  handy_queue = xQueueCreate(10, sizeof(std::string *));
  if (handy_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create handy queue");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Handy handler initialized");
  return ESP_OK;
}

/**
 * @brief 启动Handy处理任务
 * @return ESP_OK成功，其他值失败
 */
esp_err_t handy_handler_start(void) {
  if (handy_queue == nullptr) {
    ESP_LOGE(TAG, "Handy queue not initialized");
    return ESP_FAIL;
  }

  BaseType_t ret =
      xTaskCreate(handy_task, "handy_task", 4096, nullptr, 5, nullptr);

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create handy task");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Handy task started");
  return ESP_OK;
}
