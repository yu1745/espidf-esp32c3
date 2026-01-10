#pragma once

#include <cstdint>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// USB事件基声明
ESP_EVENT_DECLARE_BASE(USB_MONITOR_EVENT);

// USB事件ID定义
typedef enum {
    USB_MONITOR_EVENT_CONNECTED = 0,      /**< USB已连接 */
    USB_MONITOR_EVENT_DISCONNECTED = 1,   /**< USB已断开 */
} usb_monitor_event_id_t;

// USB事件数据结构（如果需要传递额外数据，可以扩展）
typedef struct {
    bool connected;                        /**< 连接状态 */
    int64_t timestamp;                     /**< 时间戳（微秒） */
} usb_monitor_event_data_t;

/**
 * @brief 初始化USB监控模块（不启动定时器）
 *
 * 创建定时器和事件循环，但不启动监控
 * 需要调用 usb_monitor_start() 来启动监控
 *
 * @return esp_err_t
 *         - ESP_OK: 初始化成功
 *         - ESP_FAIL: 初始化失败
 */
esp_err_t usb_monitor_init(void);

/**
 * @brief 启动USB监控定时器
 *
 * 启动定时器，开始监控USB连接状态
 * 应在所有模块初始化完成后调用
 *
 * @return esp_err_t
 *         - ESP_OK: 启动成功
 *         - ESP_FAIL: 启动失败
 */
esp_err_t usb_monitor_start(void);

/**
 * @brief 注册USB事件处理器
 *
 * 内置USB事件处理逻辑（控制LED显示）：
 * - USB已连接：绿灯常亮
 * - USB已断开：绿灯闪烁（1秒周期）
 *
 * @return esp_err_t
 *         - ESP_OK: 注册成功
 *         - ESP_FAIL: 注册失败
 */
esp_err_t usb_monitor_register_handler(void);

/**
 * @brief 反初始化USB监控模块
 *
 * 停止定时器并释放资源
 *
 * @return esp_err_t
 *         - ESP_OK: 反初始化成功
 */
esp_err_t usb_monitor_deinit(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief USB监控类（单例模式）
 *
 * 使用FreeRTOS定时器每秒检测USB连接状态
 * 当状态变化时发布ESP事件循环事件
 */
class UsbMonitor {
public:
    /**
     * @brief 获取单例实例
     * @return UsbMonitor单例实例的引用
     */
    static UsbMonitor& getInstance();

    /**
     * @brief 初始化USB监控（不启动定时器）
     * @return true 初始化成功，false 初始化失败
     */
    bool init();

    /**
     * @brief 启动USB监控定时器
     * @return true 启动成功，false 启动失败
     */
    bool start();

    /**
     * @brief 反初始化
     */
    void deinit();

    // 删除拷贝构造和拷贝赋值
    UsbMonitor(const UsbMonitor&) = delete;
    UsbMonitor& operator=(const UsbMonitor&) = delete;

private:
    /**
     * @brief 构造函数
     */
    UsbMonitor() = default;

    /**
     * @brief 析构函数
     */
    ~UsbMonitor();

    /**
     * @brief 初始化定时器
     * @return true 初始化成功，false 初始化失败
     */
    bool initTimer();

    /**
     * @brief 定时器回调函数
     * @param timer 定时器句柄
     */
    static void timerCallback(TimerHandle_t timer);

    /**
     * @brief 检查USB连接状态
     */
    void checkUsbConnection();

    static constexpr uint32_t CHECK_INTERVAL_MS = 1000; /**< 检测间隔（毫秒） */
    static constexpr char* TAG = (char*)"UsbMonitor";   /**< 日志标签 */

    TimerHandle_t m_timer = nullptr;    /**< FreeRTOS定时器句柄 */
    bool m_last_connected = false;      /**< 上次的连接状态 */
    bool m_initialized = false;         /**< 初始化状态 */
};
