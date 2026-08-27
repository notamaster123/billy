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

## Confirmed by bring-up

Everything below was measured against the real hardware with
`src/bringup.cpp`, not assumed.

| Property | Value |
|----------|-------|
| Bluetooth name | `OBDII` |
| MAC | `dc:0d:30:a9:c0:5e` |
| Link | Classic Bluetooth SPP |
| Security | **None** — see below |
| Pairing PIN | none; the adapter does not want one |
| Firmware | `ELM327 v1.3a` (clone) |
| Battery (`ATRV`) | 12.5 V |
| Vehicle protocol | **ISO 9141-2** (pre-CAN) |

### Connecting: security must be ESP_SPP_SEC_NONE

`BluetoothSerial::connect()` defaults its `sec_mask` to
`ESP_SPP_SEC_ENCRYPT | ESP_SPP_SEC_AUTHENTICATE`, which *demands*
authenticated pairing. This adapter publishes SPP with no security and
refuses that demand. The stack reports the refusal as:

```
esp_bt_gap_cb(): authentication failed, status:9
```

which is the **same error a wrong PIN produces** — the two are
indistinguishable from the log alone. Connecting with
`connect(addr, 0, ESP_SPP_SEC_NONE)` succeeds on the first attempt.
No pairing, no PIN, no bond.

### The car speaks ISO 9141-2, not CAN

The `0100` response came back headered:

```
48 6B 12 | 41 00 | BF 9F F9 91 | EE
header     mode    bitmask       checksum
```

`48 6B 12` is an ISO 9141-2 header. Two consequences:

- **The bus init is slow.** The first PID query after connecting
  triggers a slow init that can take well over five seconds to answer.
  Anything that gives up sooner will look like a dead adapter.
- **Per-query latency is much higher than CAN**, so the achievable
  refresh rate across several PIDs is modest. Worth remembering when
  tuning the gauge poll loop.

This clone also stubs out `ATDP` (returns `OK` instead of a protocol
name), and it emits headers even after `ATH0` returns `OK`. Parsing
should locate `41 <pid>` inside the reply rather than assuming the
response starts there.

### Supported PIDs (mask `BF 9F F9 91`)

`01 03 04 05 06 07 08 09 0C 0D 0E 0F 10 11 12 13 14 15 18 19 1C 20`

All four dashboard values are available: `010C` RPM, `010D` speed,
`0105` coolant temp, `0111` throttle. Also present and worth
considering later: `0104` engine load, `010F` intake air temp, `0110`
MAF, `010E` timing advance, and fuel trims `0106`–`0109`.

`0120` is set, so a further `0120` query would reveal PIDs 21–40.
