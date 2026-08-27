#pragma once

#include <Arduino.h>

// ============================================================
// Bluetooth OBD-II adapter
// ============================================================
//
// Target module: C210-591-307-AA with wireless hat 0210-591-313-AC
// (Feasycom FSC-BT825). The FSC-BT825 is a Bluetooth 5.3 dual-mode
// part with no WiFi radio, so the link is classic Bluetooth SPP --
// see HARDWARE.md.

// Name this ESP32 advertises as (only matters for pairing/debugging).
#define OBD_LOCAL_BT_NAME "ESP32-OBD2"

// Name the OBD-II adapter broadcasts. Not yet confirmed for the
// FSC-BT825 hat; generic dongles use "OBDII" or "OBDLink". Set
// OBD_SCAN_ON_BOOT below to discover the real name.
#define OBD_BT_DEVICE_NAME "OBDII"

// Logs every nearby classic Bluetooth device (name, MAC, RSSI) to the
// serial monitor at boot, then continues normally. Use this to find
// the adapter's name/MAC, then set it back to 0.
#define OBD_SCAN_ON_BOOT 0
#define OBD_SCAN_DURATION_SEC 10

// Name-based discovery (SDP lookup) is slow and flaky with most
// ELM327 clones -- connecting straight to the MAC skips it. Confirmed
// by scan: the adapter reports itself as "OBDII" at the MAC below.
#define OBD_USE_MAC_ADDRESS 1
static uint8_t OBD_MAC_ADDRESS[6] = {0xDC, 0x0D, 0x30, 0xA9, 0xC0, 0x5E};

// Legacy pairing PIN. Unused by the gauge firmware: bring-up showed
// this adapter wants no pairing at all and connects with
// ESP_SPP_SEC_NONE. Kept only for the bring-up tool's /trypins, in
// case a different adapter is swapped in later.
#define OBD_PAIRING_PIN "1234"

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
