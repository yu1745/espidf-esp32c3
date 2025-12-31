# SettingWrapper 使用文档

## 简介

`SettingWrapper` 是一个用于管理 Protocol Buffers (nanopb) `Setting` 结构体的 C++ 包装类，提供了字节数组与 Setting 结构体之间的相互转换功能，使用智能指针自动管理内存。

## 主要特性

- ✅ 使用 `std::unique_ptr` 自动管理内存，无需手动释放
- ✅ 支持字节数组与 Setting 结构体的相互转换
- ✅ 提供完整的 RAII 语义和异常安全保证
- ✅ 支持拷贝和移动语义
- ✅ 友好的智能指针式访问接口
- ✅ 使用原始指针 `uint8_t*` 进行编码/解码，避免不必要的内存分配

## 基本用法

### 1. 包含头文件

```cpp
#include "setting.hpp"
```

### 2. 创建 SettingWrapper 对象

#### 2.1 默认构造（创建空的 Setting）

```cpp
SettingWrapper setting;
// 创建一个初始化为零的 Setting 结构体
```

#### 2.2 从字节数组构造

```cpp
// 假设有一个包含 protobuf 数据的字节数组
uint8_t data[1086];  // 缓冲区大小至少为 Setting_size
size_t data_size = /* 实际数据大小 */;

SettingWrapper setting(data, data_size);
// 自动解码字节数组为 Setting 结构体
```

#### 2.3 拷贝构造

```cpp
SettingWrapper setting1;
SettingWrapper setting2(setting1);  // 深拷贝
```

#### 2.4 移动构造

```cpp
SettingWrapper setting1;
SettingWrapper setting2(std::move(setting1));  // 移动构造
```

### 3. 访问和修改 Setting 结构体

```cpp
SettingWrapper setting;

// 方式 1: 使用箭头运算符
setting->wifi.tcp_port = 8080;
setting->wifi.udp_port = 9090;
setting->servo.A_SERVO_PIN = 5;

// 方式 2: 使用解引用运算符
auto& s = *setting;
s.wifi.tcp_port = 8080;

// 方式 3: 使用 get() 获取原始指针
Setting* ptr = setting.get();
ptr->wifi.tcp_port = 8080;
```

### 4. 编码为字节数组

```cpp
SettingWrapper setting;
setting->wifi.tcp_port = 8080;

// 分配足够大的缓冲区
uint8_t buffer[SettingWrapper::getMaxEncodeSize()];

// 编码为字节数组
size_t encoded_size = setting.encode(buffer, sizeof(buffer));

// 使用编码后的数据（例如保存到文件、发送到网络等）
ESP_LOGI("Example", "编码数据大小: %zu 字节", encoded_size);
```

### 5. 从字节数组解码

```cpp
// 方式 1: 使用构造函数
uint8_t data[1086];
size_t data_size = /* 从某处获取的数据大小 */;
SettingWrapper setting(data, data_size);

// 方式 2: 使用 decode() 方法
SettingWrapper setting;
setting.decode(data, data_size);
```

### 6. 重置 Setting

```cpp
SettingWrapper setting;
setting->wifi.tcp_port = 8080;

// 重置为零值
setting.reset();
```

### 7. 验证 Setting 是否有效

```cpp
SettingWrapper setting;

if (setting.isValid()) {
    ESP_LOGI("Example", "Setting 有效");
}
```

## 完整示例

### 示例 1: 基本使用

```cpp
#include "setting.hpp"

void example_basic() {
    // 创建 SettingWrapper
    SettingWrapper setting;
    
    // 设置一些值
    strncpy(setting->wifi.ssid, "MyWiFi", sizeof(setting->wifi.ssid) - 1);
    setting->wifi.tcp_port = 8080;
    setting->wifi.udp_port = 9090;
    setting->servo.A_SERVO_PIN = 5;
    setting->led.enable = true;
    
    // 编码为字节数组
    uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
    size_t encoded_size = setting.encode(buffer, sizeof(buffer));
    ESP_LOGI("Example", "编码成功，大小: %zu 字节", encoded_size);
    
    // 从字节数组解码
    SettingWrapper setting2(buffer, encoded_size);
    ESP_LOGI("Example", "解码后 TCP 端口: %d", setting2->wifi.tcp_port);
}
```

### 示例 2: 保存到 SPIFFS

```cpp
#include "setting.hpp"
#include "spiffs.h"

void save_setting_to_spiffs(const std::string& filename) {
    SettingWrapper setting;
    setting->wifi.tcp_port = 8080;
    
    // 分配缓冲区并编码
    uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
    size_t encoded_size = setting.encode(buffer, sizeof(buffer));
    
    // 保存到 SPIFFS
    FILE* f = fopen(filename.c_str(), "wb");
    if (f) {
        fwrite(buffer, 1, encoded_size, f);
        fclose(f);
        ESP_LOGI("Example", "保存成功");
    }
}

SettingWrapper load_setting_from_spiffs(const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) {
        ESP_LOGE("Example", "打开文件失败");
        throw std::runtime_error("打开文件失败");
    }
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // 读取数据
    uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);
    
    // 解码
    return SettingWrapper(buffer, read_size);
}
```

### 示例 3: 通过 WebSocket 传输

```cpp
#include "setting.hpp"
#include "http/websocket_server.h"

void send_setting_via_websocket(int sockfd, const SettingWrapper& setting) {
    // 分配缓冲区并编码
    uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
    size_t encoded_size = setting.encode(buffer, sizeof(buffer));
    
    // 发送
    websocket_server_send_binary(sockfd, buffer, encoded_size);
    ESP_LOGI("Example", "发送 Setting，大小: %zu 字节", encoded_size);
}

void receive_setting_via_websocket(int sockfd, uint8_t* data, size_t size) {
    // 解码
    SettingWrapper setting(data, size);
    
    ESP_LOGI("Example", "接收到 Setting，TCP 端口: %d", setting->wifi.tcp_port);
}
```

### 示例 4: 异常处理

```cpp
#include "setting.hpp"

void example_with_error_handling() {
    try {
        // 创建 SettingWrapper
        SettingWrapper setting;
        
        // 设置值
        setting->wifi.tcp_port = 8080;
        
        // 编码
        uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
        size_t encoded_size = setting.encode(buffer, sizeof(buffer));
        
        // 从字节数组解码
        SettingWrapper setting2(buffer, encoded_size);
        
    } catch (const std::exception& e) {
        ESP_LOGE("Example", "发生异常: %s", e.what());
    }
}
```

### 示例 5: 拷贝和移动

```cpp
#include "setting.hpp"

void example_copy_move() {
    // 拷贝构造
    SettingWrapper setting1;
    setting1->wifi.tcp_port = 8080;
    
    SettingWrapper setting2(setting1);  // 深拷贝
    setting2->wifi.tcp_port = 9090;     // 不影响 setting1
    
    ESP_LOGI("Example", "setting1 TCP: %d", setting1->wifi.tcp_port);  // 8080
    ESP_LOGI("Example", "setting2 TCP: %d", setting2->wifi.tcp_port);  // 9090
    
    // 拷贝赋值
    SettingWrapper setting3;
    setting3 = setting2;
    
    // 移动构造
    SettingWrapper setting4(std::move(setting3));
    // setting3 现在不再拥有 Setting 对象
    
    // 移动赋值
    SettingWrapper setting5;
    setting5 = std::move(setting4);
}
```

### 示例 6: 使用栈分配的缓冲区（避免堆分配）

```cpp
#include "setting.hpp"

void example_stack_buffer() {
    SettingWrapper setting;
    setting->wifi.tcp_port = 8080;
    
    // 使用栈分配的缓冲区，避免堆分配
    uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
    size_t encoded_size = setting.encode(buffer, sizeof(buffer));
    
    // 直接使用 buffer 数据，无需 vector
    ESP_LOGI("Example", "编码数据: %p, 大小: %zu", buffer, encoded_size);
}
```

## API 参考

### 构造函数

| 函数 | 说明 |
|------|------|
| `SettingWrapper()` | 默认构造，创建零初始化的 Setting |
| `SettingWrapper(const uint8_t* data, size_t size)` | 从字节数组构造并解码 |
| `SettingWrapper(const SettingWrapper& other)` | 拷贝构造函数 |
| `SettingWrapper(SettingWrapper&& other)` | 移动构造函数 |

### 成员函数

| 函数 | 说明 |
|------|------|
| `size_t encode(uint8_t* buffer, size_t size) const` | 编码到缓冲区，返回实际编码字节数 |
| `static constexpr size_t getMaxEncodeSize()` | 获取编码所需的最大缓冲区大小 |
| `void decode(const uint8_t* data, size_t size)` | 从字节数组解码 |
| `Setting* get()` | 获取原始指针 |
| `const Setting* get() const` | 获取常量原始指针 |
| `Setting* operator->()` | 箭头运算符 |
| `Setting& operator*()` | 解引用运算符 |
| `void reset()` | 重置为零值 |
| `bool isValid() const` | 验证是否有效 |

### 运算符

| 运算符 | 说明 |
|--------|------|
| `operator=(const SettingWrapper&)` | 拷贝赋值 |
| `operator=(SettingWrapper&&)` | 移动赋值 |
| `operator->()` | 成员访问 |
| `operator*()` | 解引用 |

## 注意事项

1. **内存管理**: `SettingWrapper` 使用 `std::unique_ptr` 管理 Setting 结构体，确保对象离开作用域时自动释放内存。

2. **缓冲区大小**: 编码时传入的缓冲区大小必须至少为 `SettingWrapper::getMaxEncodeSize()`（通常为 1086 字节）。

3. **异常安全**: 所有构造函数在异常发生时会正确清理已分配的资源并重新抛出异常。

4. **字符串字段**: Setting 结构体中的字符串字段（如 `ssid`、`password`）是固定大小的字符数组，使用时注意不要超出最大长度（通常是 64 字节 + 空终止符）。

5. **指针有效性**: 传入 `decode()` 和构造函数的 `data` 指针必须有效且指向足够大小的内存区域。

6. **线程安全**: `SettingWrapper` 本身不是线程安全的，如果在多线程环境中使用，需要外部同步。

7. **日志**: 所有错误都会通过 `ESP_LOGx` 输出，标签为 `"SettingWrapper"`。

8. **缓冲区生命周期**: 编码时使用的缓冲区必须保持有效直到数据被使用完毕，编码函数不会管理缓冲区的生命周期。

## 依赖

- `nanopb` 库 (已包含在 `components/nanopb/`)
- `proto/setting.pb.h` (生成的 protobuf 头文件)

## 错误处理

当编码或解码失败时，会抛出 `std::runtime_error` 异常，包含错误信息。

```cpp
try {
    SettingWrapper setting;
    uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
    size_t size = setting.encode(buffer, sizeof(buffer));
} catch (const std::runtime_error& e) {
    ESP_LOGE("Example", "操作失败: %s", e.what());
}
```

## 性能考虑

- **零堆分配**: 使用栈分配的缓冲区可以完全避免堆分配，适合实时性要求高的场景。
- **编码大小**: 编码后的数据大小取决于实际设置的值，最大为 `Setting_size`（1086 字节）。
- **拷贝开销**: 拷贝构造会完整复制 Setting 结构体，如果结构体很大，建议使用移动语义。
- **缓冲区重用**: 对于高频编码/解码场景，可以重用同一个缓冲区，减少内存分配次数。

## 最佳实践

### 1. 编码前预检查缓冲区大小

```cpp
// 错误示例：缓冲区可能太小
uint8_t buffer[500];  // 可能在某些情况下不够
size_t size = setting.encode(buffer, sizeof(buffer));

// 正确示例：使用 getMaxEncodeSize() 获取所需大小
uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
size_t size = setting.encode(buffer, sizeof(buffer));
```

### 2. 使用栈分配减少堆分配

```cpp
// 推荐方式：栈分配
void encode_and_send(const SettingWrapper& setting) {
    uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
    size_t size = setting.encode(buffer, sizeof(buffer));
    send_to_network(buffer, size);
}

// 不推荐：使用 vector 会额外分配堆内存
void encode_and_send_vector(const SettingWrapper& setting) {
    std::vector<uint8_t> buffer(SettingWrapper::getMaxEncodeSize());
    size_t size = setting.encode(buffer.data(), buffer.size());
    send_to_network(buffer.data(), size);
}
```

### 3. 检查返回的编码大小

```cpp
uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
size_t size = setting.encode(buffer, sizeof(buffer));

if (size == 0) {
    ESP_LOGE("Example", "编码失败");
    return;
}

// 使用实际编码的数据大小
send_to_network(buffer, size);
```

### 4. 异常安全使用

```cpp
void safe_operation() {
    try {
        SettingWrapper setting;
        uint8_t buffer[SettingWrapper::getMaxEncodeSize()];
        size_t size = setting.encode(buffer, sizeof(buffer));
        // 处理编码后的数据...
    } catch (const std::exception& e) {
        ESP_LOGE("Example", "操作失败: %s", e.what());
        // 错误处理...
    }
}
```
