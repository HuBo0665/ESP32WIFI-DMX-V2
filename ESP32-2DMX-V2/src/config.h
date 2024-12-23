#pragma once

#include <Arduino.h>

// 系统配置
#define FIRMWARE_VERSION "2.0.0"
#define RESTART_TIMEOUT 1000         // 重启超时时间(ms)
#define WDT_TIMEOUT 10              // 看门狗超时时间(秒)

#ifndef TASK_STACK_SIZE
#define TASK_STACK_SIZE 8192        // 任务堆栈大小
#endif

#define TASK_PRIORITY 1             // 任务优先级

// UART和DMX配置
#define UART_BUFFER_SIZE 512

#ifndef DMX_BUFFER_SIZE
#define DMX_BUFFER_SIZE 512  // 仅当未定义时才定义
#endif

#define DMX_BAUD_RATE 250000       // DMX波特率
#define DMX_MAX_CHANNELS 512
#define DMX_UNIVERSE_SIZE 512
#define DMX_START_ADDRESS 1

// DMX引脚定义
#define DMX_TX_A_PIN GPIO_NUM_17
#define DMX_DIR_A_PIN GPIO_NUM_16
#define DMX_TX_B_PIN GPIO_NUM_18
#define DMX_DIR_B_PIN GPIO_NUM_19

// 像素LED配置
#define PIXEL_PIN GPIO_NUM_5
#define MAX_PIXELS 1360
#define DEFAULT_PIXELS 170
#define PIXEL_COUNT 170
#define PIXEL_TYPE (NEO_GRB + NEO_KHZ800)  // 添加像素类型定义

// WiFi配置
#define WIFI_SSID "542628277"
#define WIFI_PASS "542628277"
#define WIFI_CONNECT_TIMEOUT 10000  // WiFi连接超时时间(ms)
#define WIFI_RETRY_COUNT 3          // WiFi重试次数

// AP模式配置
#define DEFAULT_AP_SSID "HuBo-DMX-AP"
#define DEFAULT_AP_PASS "542628277"
#define AP_CHANNEL 1
#define MAX_AP_CONNECTIONS 4
#define AP_IP_ADDRESS IPAddress(192, 168, 4, 1)
#define AP_GATEWAY IPAddress(192, 168, 4, 1)
#define AP_SUBNET IPAddress(255, 255, 255, 0)

// Art-Net配置
#define ARTNET_PORT 6454
#define START_UNIVERSE 0
#define START_SUBNET 0
#define MAX_UNIVERSES 4            // 最大支持的宇宙数
#define ARTNET_POLL_TIMEOUT 5000   // Art-Net轮询超时时间(ms)

// 设备配置
#define DEVICE_NAME "HuBo-ArtNode"
#define DEVICE_LONG_NAME "HuBo Art-Net Node"
#define MANUFACTURER_NAME "HuBo"
#define OEM_CODE 0x0000            // 自定义OEM代码

// 系统状态检查间隔
#define STATUS_CHECK_INTERVAL 10000    // 状态检查间隔(ms)
#define HEAP_REPORT_INTERVAL 30000     // 堆内存报告间隔(ms)
#define MINIMUM_FREE_HEAP 10000        // 最小可用堆内存(bytes)

// UART配置常量
#define UART_CONFIG_DEFAULT {          \
    .baud_rate = DMX_BAUD_RATE,       \
    .data_bits = UART_DATA_8_BITS,    \
    .parity = UART_PARITY_DISABLE,    \
    .stop_bits = UART_STOP_BITS_2,    \
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE \
}

// 调试配置
#ifdef DEBUG_MODE
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(format, ...) Serial.printf(format, __VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(format, ...)
#endif

// 错误代码定义
enum ErrorCodes {
    ERR_NONE = 0,
    ERR_WIFI_CONNECT_FAILED = 1,
    ERR_AP_START_FAILED = 2,
    ERR_DMX_INIT_FAILED = 3,
    ERR_ARTNET_INIT_FAILED = 4,
    ERR_PIXEL_INIT_FAILED = 5,
    ERR_TASK_CREATE_FAILED = 6,
    ERR_LOW_MEMORY = 7
};

// 网络状态定义
enum NetworkStatus {
    NET_DISCONNECTED = 0,
    NET_CONNECTING = 1,
    NET_CONNECTED = 2,
    NET_AP_MODE = 3,
    NET_ERROR = 4
};

// DMX端口定义
enum DMXPorts {
    DMX_PORT_A = 0,
    DMX_PORT_B = 1
};

// 确保关键配置值有效
static_assert(UART_BUFFER_SIZE >= DMX_BUFFER_SIZE, "UART buffer size must be >= DMX buffer size");
static_assert(MAX_PIXELS <= 1360, "MAX_PIXELS exceeds hardware limit");
static_assert(PIXEL_COUNT <= MAX_PIXELS, "PIXEL_COUNT exceeds MAX_PIXELS");