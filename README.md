# ESP32 WebSocket 服务器

这是一个基于ESP-IDF v4.4的WebSocket服务器实现，使用ESP32的HTTP服务器组件创建。

## 功能特性

- 支持多客户端连接（最多4个）
- 实时消息双向通信
- 自动连接管理和断线重连
- 定期广播消息
- 简单的HTML测试客户端

## 项目结构

```
src/
├── main.c                 # 主程序入口，WiFi初始化和服务器启动
├── websocket_server.h     # WebSocket服务器头文件
├── websocket_server.c     # WebSocket服务器实现
└── websocket_client.html  # HTML测试客户端
```

## 硬件要求

- ESP32开发板（本项目使用ESP32-C3-DevKitM-1）
- WiFi网络环境

## 软件依赖

- ESP-IDF v4.4
- PlatformIO（可选）

## 配置说明

### 1. WiFi配置

在 `src/main.c` 中修改WiFi配置：

```c
#define WIFI_SSID "YourWiFiSSID"      // 替换为你的WiFi名称
#define WIFI_PASS "YourWiFiPassword"  // 替换为你的WiFi密码
```

### 2. 服务器配置

在 `src/websocket_server.h` 中可以修改服务器配置：

```c
#define WEBSOCKET_SERVER_PORT 80      // WebSocket服务器端口
#define WEBSOCKET_BUFFER_SIZE 1024    // 缓冲区大小
#define MAX_CLIENTS 4                 // 最大客户端连接数
```

## 编译和烧录

### 使用PlatformIO

```bash
# 编译项目
pio run

# 上传到ESP32
pio run --target upload

# 监控串口输出
pio device monitor
```

### 使用ESP-IDF

```bash
# 配置项目
idf.py menuconfig

# 编译项目
idf.py build

# 上传到ESP32
idf.py -p COM_PORT flash

# 监控串口输出
idf.py -p COM_PORT monitor
```

## 使用方法

### 1. 启动服务器

1. 配置WiFi信息
2. 编译并上传代码到ESP32
3. 查看串口输出，获取ESP32的IP地址

### 2. 连接WebSocket客户端

1. 打开 `src/websocket_client.html` 文件（使用现代浏览器）
2. 在服务器地址中输入 `ws://ESP32_IP_ADDRESS/ws`
3. 点击"连接"按钮
4. 发送消息进行测试

### 3. API说明

#### WebSocket端点
- `ws://ESP32_IP_ADDRESS/ws` - WebSocket连接端点

#### 主要功能
- **连接管理**: 自动处理客户端连接和断开
- **消息回显**: 服务器会回显收到的消息
- **广播功能**: 每10秒自动发送广播消息
- **多客户端**: 支持最多4个客户端同时连接

## 技术实现

### WebSocket握手

服务器实现了标准的WebSocket握手协议：
1. 接收客户端的Upgrade请求
2. 提取Sec-WebSocket-Key
3. 计算Sec-WebSocket-Accept值
4. 发送101 Switching Protocols响应

### 帧格式

支持基本的WebSocket帧格式：
- 文本帧（0x1）
- 二进制帧（0x2）
- Ping帧（0x9）
- Pong帧（0xA）
- 关闭帧（0x8）

### 错误处理

- 连接失败自动重试
- 客户端断开自动清理资源
- 帧格式错误处理

## 故障排除

### 常见问题

1. **无法连接WiFi**
   - 检查WiFi名称和密码是否正确
   - 确认WiFi信号强度足够

2. **WebSocket连接失败**
   - 确认ESP32已获取IP地址
   - 检查防火墙设置
   - 确认端口号80未被占用

3. **消息无法发送**
   - 检查客户端是否已连接
   - 查看ESP32串口输出的错误信息

### 调试信息

ESP32会输出详细的调试信息，包括：
- WiFi连接状态
- 客户端连接信息
- 消息收发日志
- 错误信息

## 扩展功能

可以基于此实现进一步扩展：
- 添加用户认证
- 实现消息加密
- 支持更大的消息体
- 添加消息持久化
- 实现房间/频道功能

## 许可证

本项目仅供学习和研究使用。