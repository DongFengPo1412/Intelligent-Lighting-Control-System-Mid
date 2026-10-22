#ifndef CONFIG_H
#define CONFIG_H

#include "MatrixLayout.h"

// --- 1. 单块 16x16 逐行蛇形灯光矩阵配置（左上角 DIN） ---
#define MATRIX_WIDTH  MatrixLayout::kWidth
#define MATRIX_HEIGHT MatrixLayout::kHeight
#define LED_PIN     4      // WS2812B 数据信号线
#define NUM_LEDS    MatrixLayout::kPixelCount // 256 颗灯珠
#define BRIGHTNESS  30     // 初始亮度 (0-255)

// --- 2. 跨板串口通信配置 (与 S3 对接) ---
// One-way link: S3 TX GPIO17 -> WROOM RX GPIO16, plus common ground.
// Reverse transmission is unused. Do not connect WROOM GPIO17 to S3 GPIO18,
// which the selected S3 board configuration reserves for the lamp output.
#define S3_RX       16     // 对应 WROOM-32 的 RX2 接口
#define S3_TX       (-1)   // Reverse TX is unused; do not wire it to S3 GPIO18 (lamp).

// --- 3. NVIDIA Jetson Nano 串口通信配置 ---
// 使用 GPIO 25 (RX) 和 GPIO 26 (TX) 进行杜邦线串口通信，以完全避开板载 USB 调试串口的电平冲突。
// 物理接线：
//   - Jetson Nano J41 Pin 8 (TX) -> ESP32 GPIO 25
//   - Jetson Nano J41 Pin 10 (RX) -> ESP32 GPIO 26
//   - Jetson Nano GND -> ESP32 GND
#define JETSON_RX   25     
#define JETSON_TX   26     

// --- 4. 统一最大功耗约束 ---
#define MAX_POWER_VOLTS     5      // 最大电压 5V
#define MAX_POWER_MA        1200   // 最大电流 1200mA (1.2A)

// --- 5. Blinker & WiFi 凭证 ---
#if __has_include("Config.private.h")
#include "Config.private.h"
#else
// 如果没有私有配置文件，使用默认占位符（用户复制模版后修改）
const char auth[] = "YOUR_BLINKER_AUTH"; 
const char ssid[] = "YOUR_WIFI_SSID";
const char pswd[] = "YOUR_WIFI_PASSWORD"; 
#endif

#endif
