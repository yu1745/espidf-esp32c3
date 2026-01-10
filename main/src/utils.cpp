#include "utils.hpp"
#include "def.h"
#include <cmath>
#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <sdkconfig.h>
#include <string>


void delay1() { vTaskDelay(1); }

std::string getBuildParameters() {
  std::unique_ptr<char[]> json_buffer(new (std::nothrow) char[1024]);
  if (!json_buffer) {
    throw std::runtime_error("内存分配失败");
  }
  memset(json_buffer.get(), 0, 1024);

  int n = snprintf(
      json_buffer.get(), 1024,
      R"({"firmware_version":"%s","build_time":"%s %s UTC+8","hardware":"%s")",
      FIRMWARE_VERSION, __DATE__, __TIME__,
#if CONFIG_IDF_TARGET_ESP32C3
      "esp32-c3"
#elif CONFIG_IDF_TARGET_ESP32
      "esp32"
#else
      "unknown"
#endif
  );

#ifdef CONFIG_ENABLE_WIFI
  n += snprintf(json_buffer.get() + n, 1024 - n, R"(, "ENABLE_WIFI":%d)",
                CONFIG_ENABLE_WIFI);
#endif
#ifdef CONFIG_ENABLE_BLE
  n += snprintf(json_buffer.get() + n, 1024 - n, R"(, "ENABLE_BLE":%d)",
                CONFIG_ENABLE_BLE);
#endif
#ifdef CONFIG_ENABLE_LED
  n += snprintf(json_buffer.get() + n, 1024 - n, R"(, "ENABLE_LED":%d)",
                CONFIG_ENABLE_LED);
#endif
#ifdef CONFIG_ENABLE_TEMP
  n += snprintf(json_buffer.get() + n, 1024 - n, R"(, "ENABLE_TEMP":%d)",
                CONFIG_ENABLE_TEMP);
#endif
#ifdef CONFIG_ENABLE_BUTTON
  n += snprintf(json_buffer.get() + n, 1024 - n, R"(, "ENABLE_BUTTON":%d)",
                CONFIG_ENABLE_BUTTON);
#endif
#ifdef CONFIG_ENABLE_VOLTAGE
  n += snprintf(json_buffer.get() + n, 1024 - n, R"(, "ENABLE_VOLTAGE":%d)",
                CONFIG_ENABLE_VOLTAGE);
#endif
#ifdef CONFIG_ENABLE_DECOY
  n += snprintf(json_buffer.get() + n, 1024 - n, R"(, "ENABLE_DECOY":%d)",
                CONFIG_ENABLE_DECOY);
#endif
  snprintf(json_buffer.get() + n, 1024 - n, R"(})");

  return std::string(json_buffer.get());
}

void axis7_to_axis6(float x7, float y7, float z7, float roll7, float pitch7,
                    float twist7, float &x6, float &y6, float &z6, float &roll6,
                    float &pitch6, float &twist6) {
  // 将欧拉角转换为弧度
  float roll = roll7 * M_PI / 180.0f;
  float pitch = pitch7 * M_PI / 180.0f;
  float twist = twist7 * M_PI / 180.0f;

  // 旋转顺序：roll(x) -> twist(y) -> pitch(z)
  // 计算旋转矩阵元素
  float cos_roll = cosf(roll);
  float sin_roll = sinf(roll);
  float cos_pitch = cosf(pitch);
  float sin_pitch = sinf(pitch);
  float cos_twist = cosf(twist);
  float sin_twist = sinf(twist);

  // 第七轴末端的旋转矩阵
  float seventh_rotation[3][3] = {
      {cos_pitch * cos_twist + sin_pitch * sin_roll * sin_twist,
       -sin_pitch * cos_roll,
       cos_pitch * sin_twist - sin_pitch * sin_roll * cos_twist},
      {sin_pitch * cos_twist - cos_pitch * sin_roll * sin_twist,
       cos_pitch * cos_roll,
       sin_pitch * sin_twist + cos_pitch * sin_roll * cos_twist},
      {-cos_roll * sin_twist, sin_roll, cos_roll * cos_twist}};

  // 第七轴末端的齐次变换矩阵
  float T_7[4][4] = {{seventh_rotation[0][0], seventh_rotation[0][1],
                      seventh_rotation[0][2], x7},
                     {seventh_rotation[1][0], seventh_rotation[1][1],
                      seventh_rotation[1][2], y7},
                     {seventh_rotation[2][0], seventh_rotation[2][1],
                      seventh_rotation[2][2], z7},
                     {0.0f, 0.0f, 0.0f, 1.0f}};

  // 计算第六轴末端的齐次变换矩阵
  // T_7 = T_6 @ T_6_to_7 => T_6 = T_7 @ inv(T_6_to_7)
  // T_6_to_7的逆矩阵（因为是平移矩阵，逆矩阵为负平移）
  float T_6_to_7_inv[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                              {0.0f, 1.0f, 0.0f, -EXTENSION_LENGTH},
                              {0.0f, 0.0f, 1.0f, 0.0f},
                              {0.0f, 0.0f, 0.0f, 1.0f}};

  // 矩阵乘法 T_6 = T_7 @ T_6_to_7_inv
  float T_6[4][4];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      T_6[i][j] = 0.0f;
      for (int k = 0; k < 4; k++) {
        T_6[i][j] += T_7[i][k] * T_6_to_7_inv[k][j];
      }
    }
  }

  // 提取位置和旋转
  x6 = T_6[0][3];
  y6 = T_6[1][3];
  z6 = T_6[2][3];

  // 第六轴末端的旋转矩阵
  float sixth_rotation[3][3] = {{T_6[0][0], T_6[0][1], T_6[0][2]},
                                {T_6[1][0], T_6[1][1], T_6[1][2]},
                                {T_6[2][0], T_6[2][1], T_6[2][2]}};

  // 将旋转矩阵转换为欧拉角
  float R[3][3] = {
      {sixth_rotation[0][0], sixth_rotation[0][1], sixth_rotation[0][2]},
      {sixth_rotation[1][0], sixth_rotation[1][1], sixth_rotation[1][2]},
      {sixth_rotation[2][0], sixth_rotation[2][1], sixth_rotation[2][2]}};

  float sy = sqrtf(R[0][0] * R[0][0] + R[1][0] * R[1][0]);

  float roll_rad, pitch_rad, twist_rad;

  if (sy > 1e-6f) { // 非奇异情况
    roll_rad = atan2f(R[2][1], R[2][2]);
    twist_rad = atan2f(-R[2][0], sy);
    pitch_rad = atan2f(R[1][0], R[0][0]);
  } else { // 奇异情况（万向节锁）
    roll_rad = atan2f(-R[1][2], R[1][1]);
    twist_rad = atan2f(-R[2][0], sy);
    pitch_rad = 0.0f;
  }

  // 将弧度转换为角度
  roll6 = roll_rad * 180.0f / M_PI;
  pitch6 = pitch_rad * 180.0f / M_PI;
  twist6 = twist_rad * 180.0f / M_PI;
}

void list_root_directory() {
  const char *TAG = "vfs";
  ESP_LOGI(TAG, "Listing root directory (/):");

  DIR *dir = opendir("/spiffs");
  if (dir == NULL) {
    ESP_LOGE(TAG, "Failed to open root directory");
    return;
  }

  struct dirent *entry;
  int count = 0;

  while ((entry = readdir(dir)) != NULL) {
    ESP_LOGI(TAG, "  %s", entry->d_name);
    count++;
  }

  closedir(dir);
  ESP_LOGI(TAG, "Total entries: %d", count);
}

void printBuildConfigOptions() {
  const char *TAG = "BuildConfig";
  ESP_LOGI(TAG, "========== 编译选项状态 ==========");
  ESP_LOGI(TAG, "CONFIG_ENABLE_WIFI:    %s",
           CONFIG_ENABLE_WIFI ? "开启" : "关闭");
  ESP_LOGI(TAG, "CONFIG_ENABLE_BLE:     %s",
           CONFIG_ENABLE_BLE ? "开启" : "关闭");
  ESP_LOGI(TAG, "CONFIG_ENABLE_LED:     %s",
           CONFIG_ENABLE_LED ? "开启" : "关闭");
  ESP_LOGI(TAG, "CONFIG_ENABLE_TEMP:    %s",
           CONFIG_ENABLE_TEMP ? "开启" : "关闭");
  ESP_LOGI(TAG, "CONFIG_ENABLE_BUTTON:  %s",
           CONFIG_ENABLE_BUTTON ? "开启" : "关闭");
  ESP_LOGI(TAG, "CONFIG_ENABLE_VOLTAGE: %s",
           CONFIG_ENABLE_VOLTAGE ? "开启" : "关闭");
  ESP_LOGI(TAG, "CONFIG_ENABLE_DECOY:   %s",
           CONFIG_ENABLE_DECOY ? "开启" : "关闭");
  ESP_LOGI(TAG, "CONFIG_ENABLE_MDNS:    %s",
           CONFIG_ENABLE_MDNS ? "开启" : "关闭");
  ESP_LOGI(TAG, "====================================");
}