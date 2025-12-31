# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-C3 multi-axis motion control project based on ESP-IDF v5.5.1. It implements a T-Code compatible servo controller with multiple communication interfaces (BLE, HTTP/WebSocket, TCP/UDP), supporting various executor modes for different robotic configurations (OSR, SR6, TrRMax, SR6CAN, O6).

## Build System

ESP-IDF build tool `idf.py` is available in the environment path.

### Common Commands

```powershell
# Build project
idf.py build

# Flash to device
idf.py -p COM_PORT flash

# Monitor serial output
idf.py -p COM_PORT monitor

# Clean build
idf.py fullclean

# Configuration menu
idf.py menuconfig

# Build, flash and monitor in one command
idf.py -p COM_PORT flash monitor
```

## Project Architecture

### Core Communication Flow

```
┌─────────────┐
│   BLE/      │
│   HTTP/     │───┐
│   TCP/UDP   │   │
│  WebSocket  │   │
└─────────────┘   │
                  │
                  ▼
          ┌───────────────┐
          │ global_rx_queue│ (FreeRTOS queue)
          └───────────────┘
                  │
                  ▼
          ┌───────────────┐
          │   Executor    │ (Abstract base class)
          │  - TCode      │
          │  - compute()  │
          │  - execute()  │
          └───────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
        ▼                   ▼
┌──────────────┐    ┌──────────────┐
│ Actuator     │    │ CAN/TWAI     │
│ (LED/PWM/SPI)│    │ Communication │
└──────────────┘    └──────────────┘
```

### Key Components

1. **Communication Interfaces**
   - **BLE**: Defined in `main/include/ble/def.hpp` using macro-based registration (`BLE_SERVICE`, `READ_CHR`, `WRITE_CHR`, `READ_WRITE_CHR`)
   - **HTTP/WebSocket**: Defined in `main/include/http/def.hpp` using lambda-based route registration (`GET`, `POST`)
   - **TCP/UDP**: Server implementations in `main/src/tcp_server.cpp`, `main/src/udp_server.cpp`
   - **UART**: Dual UART support in `main/src/uart/`

2. **Executor System** (`main/src/executor/`)
   - **Abstract base**: `Executor` class with `compute()` and `execute()` pure virtual methods
   - **Factory pattern**: `ExecutorFactory::createExecutor()` creates specific executor based on `setting.servo.MODE`
   - **Supported modes**:
     - 0: OSR (Multi-Axis Motion)
     - 3: SR6
     - 6: TrRMax
     - 8: SR6CAN
     - 9: O6 (6-Axis Parallel Robot)
   - **T-Code parsing**: All executors use `TCode` class to parse T-Code protocol strings

3. **Configuration Management**
   - **Protobuf-based**: Settings stored in nanopb format (`main/proto/setting.pb`)
   - **Storage**: SPIFFS filesystem at `/spiffs/setting.bin`
   - **Wrapper**: `SettingWrapper` class provides encoding/decoding and file I/O
   - **WiFi reconfigure**: When settings are updated via BLE/HTTP, WiFi automatically reconfigures if config changed

4. **Actuator Control** (`main/src/actuator/`)
   - **LEDC**: PWM-based servo control
   - **RMT**: Remote Control peripheral for precise timing
   - **SPI**: High-speed serial communication

5. **Global State** (`main/include/globals.hpp`)
   - `global_rx_queue`: FreeRTOS queue for incoming command packets
   - `g_executor`: `std::unique_ptr<Executor>` - current executor instance
   - `g_http_server`: HTTP server handle

### Data Flow

1. **Command reception**: BLE/HTTP/TCP/UDP receives data → creates `data_packet_t` → sends to `global_rx_queue`
2. **Parsing**: Executor's parser task reads from queue → parses T-Code strings
3. **Computation**: `compute()` calculates servo positions
4. **Execution**: `execute()` sends commands to actuators via LEDC/RMT/SPI

## Code Conventions

From `.cursor/rules/rules.mdc`:

1. **Header guards**: Always use `#pragma once`
2. **Logging**: Use `ESP_LOGx` macros, NOT `std::cout`
3. **Memory allocation**: Prefer smart pointers (`std::unique_ptr`, `std::make_unique`)
4. **Mutex**: Use `std::mutex` with `std::lock_guard`, NOT FreeRTOS semaphores
5. **HTTP/BLE endpoints**: Use smart pointers for memory allocation, never stack allocation
6. **Exception handling**: Wrap constructors in try-catch, log errors with `ESP_LOGE`, cleanup resources, then rethrow
7. **ESP-IDF version**: v5.5.1 for ESP32-C3

### Context7 for Documentation

When looking up ESP-IDF APIs, use:
```
Library ID: /websites/espressif_projects_esp-idf_en_stable_esp32c3
```

## Important Implementation Details

### BLE Characteristics Definition

BLE endpoints are defined in `main/include/ble/def.hpp` using special macros:

```cpp
BLE_SERVICE(&service_uuid)
  READ_CHR(&uuid, handler_fn, handle)
  WRITE_CHR(&uuid, handler_fn, handle)
  READ_WRITE_CHR(&uuid, handler_fn, handle)
BLE_SERVICE_END()
```

These macros use `EXEC_LAMBDA` to register handlers at static initialization time.

### HTTP Routes Definition

HTTP endpoints are defined in `main/include/http/def.hpp` using lambda macros:

```cpp
GET("/path", [](httpd_req_t *req) -> esp_err_t { ... })
POST("/path", [](httpd_req_t *req) -> esp_err_t { ... })
```

### WiFi Reconfiguration

The `wifi_init()` function is idempotent (can be called multiple times). When WiFi settings change:
1. Load old config
2. Save new config
3. Compare using `SettingWrapper::isWifiConfigChanged()`
4. If changed, call `wifi_reconfigure()` which stops and restarts WiFi with new settings

### Executor Factory Pattern

When settings change, recreate the executor:
```cpp
g_executor = ExecutorFactory::createExecutor(setting);
```

This replaces the entire executor instance with the appropriate type based on `setting.servo.MODE`.

## Configuration

- **WiFi settings**: Stored in `Setting.wifi` (SSID, password, AP mode)
- **Servo settings**: Stored in `Setting.servo` (pins, PWM frequencies, zero positions, scaling)
- **mDNS hostname**: Configurable in `Setting.mdns.name`
- **LED control**: Configurable via Kconfig options

## File Structure Highlights

- `main/src/main.cpp`: Entry point, initializes all subsystems
- `main/include/ble/def.hpp`: BLE characteristics and handlers
- `main/include/http/def.hpp`: HTTP routes and handlers
- `main/src/executor/`: Executor implementations for different modes
- `main/src/actuator/`: Actuator drivers (LED, RMT, SPI)
- `main/include/setting.hpp`: Configuration management
- `main/proto/`: Protocol buffer definitions
- `components/`: Custom components (nanopb for protobuf)
