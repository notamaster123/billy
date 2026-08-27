# Hardware inventory

Running record of the exact parts this project targets, so pin
assignments and transport choices can be traced back to real
datasheets rather than guesswork.

## Controller — Espressif ESP32-WROOM-32E

- Original ESP32 (Xtensa dual-core), **not** an S2/S3/C3 variant.
- Supports **classic Bluetooth (BR/EDR + SPP)** as well as BLE and
  WiFi. Classic Bluetooth is what this project uses, and it is
  unavailable on the S2/S3/C3 parts — so those boards are not
  drop-in substitutes.
- PlatformIO board id: `esp32dev`.

### Pinout (from the devkit reference)

Pins this project currently uses:

| Function        | Pin    |
|-----------------|--------|
| I2C SDA (`Wire`)| IO21   |
| I2C SCL (`Wire`)| IO22   |
| Serial TX       | TXD0 / IO1 |
| Serial RX       | RXD0 / IO3 |

Notable pins kept free for future use:

- VSPI/SPI: MOSI IO23, MISO IO19, SCK IO18, SS IO5
- HSPI: MOSI IO13, MISO IO12, SCK IO14, SS IO15
- DAC: IO25 (DAC1), IO26 (DAC2)
- Input-only (no pullups, ADC-capable): IO34, IO35, IO36 (SENSOR_VP),
  IO39 (SENSOR_VN)
- Strapping pins — avoid for general I/O: IO0, IO2, IO15
- **Do not use** IO6–IO11 (SD0/SD1/SD2/SD3/CLK/CMD): wired to the
  module's internal SPI flash.

## Display — Inland 1.3" OLED graphic display

- 128x64, I2C, **SH1106** controller.
- Default I2C address `0x3C` (a few clones use `0x3D`).
- Driven via Adafruit GFX + Adafruit SH110X.
- Config flag `DISPLAY_IS_SH1106` in `include/config.h` switches to
  SSD1306 if a different panel is ever substituted.

## OBD-II module

- Base module part number: **C210-591-307-AA**
- Wireless hat part number: **0210-591-313-AC**
- Hat's wireless chip: **Feasycom FSC-BT825**

### FSC-BT825 findings

Per Feasycom's datasheet, the FSC-BT825 is built on a **Realtek
RTL8761BTV** transceiver and is a **Bluetooth 5.3 dual-mode
(BR/EDR + BLE) module — it contains no WiFi radio**. It exposes a
UART host interface and supports SPP, GATT, HID, HFP, A2DP, and
AVRCP profiles.

Sources:
- https://www.feasycom.com/fsc-bt825
- https://www.feasycom.com/datasheet/fsc-bt825.pdf

**Consequence for this project:** the link to the OBD-II module is
**Bluetooth SPP (classic)**, not WiFi. SPP is exactly what the ESP32's
`BluetoothSerial` speaks, so the ESP32 acts as the SPP master and the
module as the slave. If a WiFi path is ever wanted, it would require a
separate WiFi radio on the OBD module — the FSC-BT825 cannot provide
one.

### Open questions

- The adapter's advertised Bluetooth **device name and MAC are not yet
  known**. Neither part number resolves in public parts databases, so
  these have to be read off the hardware. Set
  `OBD_SCAN_ON_BOOT` to `1` in `include/config.h` and watch the serial
  monitor at 115200 baud — the firmware will list every nearby classic
  Bluetooth device with its name, MAC, and RSSI. Put the right one in
  `OBD_BT_DEVICE_NAME` (or `OBD_MAC_ADDRESS`).
- Pairing PIN is unconfirmed. Most ELM327-style adapters use `1234` or
  `0000`; set `OBD_PAIRING_PIN` if the module requires one.
- Whether the module speaks the **ELM327 AT command set** over SPP is
  unconfirmed. The firmware assumes it does (via the ELMduino
  library), which is the norm for OBD-II dongles. If this module uses
  a proprietary protocol instead, `src/obd_manager.cpp` is the only
  file that would need to change.
