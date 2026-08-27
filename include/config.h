#pragma once

#include <Arduino.h>

// ============================================================
// Bluetooth OBD-II adapter
// ============================================================

// Name this ESP32 advertises as (only matters for pairing/debugging).
#define OBD_LOCAL_BT_NAME "ESP32-OBD2"

// Name of the ELM327 Bluetooth adapter to connect to. Most generic
// clones broadcast "OBDII" or "OBDLink"; scan with a phone first and
// update this if yours differs.
#define OBD_BT_DEVICE_NAME "OBDII"

// Name-based discovery (SDP lookup) can be slow or flaky with some
// ELM327 clones. If connecting by name fails, set this to 1 and fill
// in OBD_MAC_ADDRESS with the adapter's Bluetooth MAC instead (find it
// via a phone's Bluetooth settings or a BLE/BT scanner app).
#define OBD_USE_MAC_ADDRESS 0
static const uint8_t OBD_MAC_ADDRESS[6] = {0x00, 0x1D, 0xA5, 0x00, 0x00, 0x00};

// How long to wait for a response to an AT/PID command before giving up.
#define ELM327_TIMEOUT_MS 2000

// ============================================================
// OLED display (Inland 1.3" - SH1106 driver, 128x64, I2C)
// ============================================================

// 1 for SH1106-based panels (most 1.3" 128x64 modules, including
// Inland's), 0 for SSD1306-based panels (common on 0.96" modules).
// Wrong setting typically shows a blank or garbled screen.
#define DISPLAY_IS_SH1106 1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_I2C_ADDRESS 0x3C

// ESP32 default I2C pins; change if the display is wired elsewhere.
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22

// ============================================================
// Timing
// ============================================================

#define OBD_RECONNECT_INTERVAL_MS 5000
#define DISPLAY_REFRESH_INTERVAL_MS 100
