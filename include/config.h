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

// How long to wait for a response before giving up. The car is on
// ISO 9141-2 (confirmed -- see HARDWARE.md), which is far slower than
// CAN, and the first query after connecting triggers a slow bus init
// that can take several seconds.
#define ELM327_TIMEOUT_MS 5000

// ============================================================
// OLED display (Inland 1.3" - SH1106 driver, 128x64, I2C)
// ============================================================

// 1 for SH1106-based panels (most 1.3" 128x64 modules, including
// Inland's), 0 for SSD1306-based panels (common on 0.96" modules).
// Wrong setting typically shows a blank or garbled screen.
#define DISPLAY_IS_SH1106 1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Interface. This panel has 7 pins (GND VCC CLK MOSI RES DC CS), so
// it is the SPI variant. Set to 0 only for a 4-pin I2C module
// (GND VCC SDA SCL).
#define DISPLAY_USE_SPI 1

// --- SPI wiring ---------------------------------------------------
//
// Hardware SPI uses the ESP32's VSPI peripheral, whose CLK and MOSI
// pins are fixed:
//
//   module CLK  -> GPIO18   (VSPI SCK)
//   module MOSI -> GPIO23   (VSPI MOSI)
//
// Those two are not configurable below, because the Adafruit driver
// calls SPI.begin() itself and would undo any remapping. DC, RES and
// CS are ordinary GPIOs and can be moved freely.
//
// If you need CLK/MOSI on different pins, set DISPLAY_SPI_HARDWARE to
// 0 to bit-bang instead; then OLED_CLK_PIN and OLED_MOSI_PIN apply.
#define DISPLAY_SPI_HARDWARE 1

#define OLED_CLK_PIN 18    // only used when DISPLAY_SPI_HARDWARE is 0
#define OLED_MOSI_PIN 23   // only used when DISPLAY_SPI_HARDWARE is 0
#define OLED_RES_PIN 16
#define OLED_DC_PIN 17
#define OLED_CS_PIN 5

// --- I2C wiring (only when DISPLAY_USE_SPI is 0) ------------------
#define OLED_I2C_ADDRESS 0x3C
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22

// ============================================================
// Rotary encoder (KY-040 style: CLK, DT, SW)
// ============================================================
//
// These three are free on the WROOM-32E: not strapping pins, not
// input-only, and clear of the I2C pins the display uses. Wire the
// encoder's + to 3V3 and GND to GND; internal pull-ups are enabled so
// no external resistors are needed.

#define ENCODER_CLK_PIN 32
#define ENCODER_DT_PIN 33
#define ENCODER_SW_PIN 25

// Held longer than this counts as a long press.
#define ENCODER_LONG_PRESS_MS 600
#define ENCODER_DEBOUNCE_MS 25

// ============================================================
// Simulation
// ============================================================

// Boot into simulated data so the UI can be built and reviewed at a
// desk with no car and no adapter. Long-press the encoder to switch
// between SIM and LIVE at runtime.
#define START_IN_SIMULATION 1

// ============================================================
// Timing
// ============================================================

#define OBD_RECONNECT_INTERVAL_MS 5000
#define DISPLAY_REFRESH_INTERVAL_MS 100
