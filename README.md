# billy

An ESP32-based OBD-II gauge cluster. A generic ELM327 Bluetooth OBD-II
adapter plugs into a car's OBD-II port; an ESP32-WROOM-32E dev board
connects to it over classic Bluetooth (SPP), polls a handful of PIDs
(RPM, speed, coolant temp, throttle position), and renders them on an
Inland 1.3" OLED display.

## Hardware

- ESP32-WROOM-32E dev board (any generic 30/38-pin devkit works; the
  `esp32dev` PlatformIO board definition targets this class of board)
- Generic ELM327 Bluetooth (classic/SPP) OBD-II adapter
- Inland 1.3" OLED display, 128x64, I2C (SH1106 driver)
- Jumper wires, USB cable for flashing/power

## Wiring

I2C only — 4 wires between the ESP32 and the OLED:

| OLED pin | ESP32 pin        |
|----------|------------------|
| VCC      | 3V3              |
| GND      | GND              |
| SDA      | GPIO21           |
| SCL      | GPIO22           |

The OBD-II adapter is wireless (Bluetooth) — nothing to wire there
besides plugging it into the car's OBD-II port and powering the ESP32
(USB power bank, or a 5V buck converter off the car's 12V system).

## Software setup

This is a [PlatformIO](https://platformio.org/) project.

1. Install the PlatformIO extension for VS Code, or the `pio` CLI.
2. Open this folder in VS Code / run PlatformIO from this folder.
3. Edit `include/config.h`:
   - `OBD_BT_DEVICE_NAME` — the Bluetooth name your adapter broadcasts
     (find it by scanning for Bluetooth devices with a phone first;
     common names are `OBDII`, `OBDLink`, `Vlink`, or a MAC-looking
     string). If name-based connect is unreliable, set
     `OBD_USE_MAC_ADDRESS` to `1` and fill in `OBD_MAC_ADDRESS`
     instead.
   - `DISPLAY_IS_SH1106` — leave at `1` for the Inland 1.3" panel
     (SH1106 driver). Set to `0` only if you swap in a 0.96"
     SSD1306-based panel.
4. Build and flash: `pio run -t upload` (or the PlatformIO toolbar
   buttons). Open the serial monitor at 115200 baud (`pio device
   monitor`) to see connection/debug logs.

## How it works

- `src/obd_manager.*` opens a classic Bluetooth (SPP) connection to
  the ELM327 adapter via `BluetoothSerial`, initializes it with the
  [ELMduino](https://github.com/PowerBroker2/ELMduino) library, and
  round-robins non-blocking PID queries (RPM, speed, coolant temp,
  throttle) once connected. It automatically retries the connection
  every `OBD_RECONNECT_INTERVAL_MS` if the link drops.
- `src/display_manager.*` drives the OLED via Adafruit's GFX +
  SH110X/SSD1306 libraries, showing a splash screen on boot, a
  "searching for adapter" screen while disconnected, and a dashboard
  (big RPM readout, speed/coolant/throttle) once connected.
- `src/main.cpp` wires the two together: poll the OBD manager every
  loop iteration, redraw the display on a fixed interval.

## Troubleshooting

- **Blank/garbled OLED**: wrong driver selected. Try flipping
  `DISPLAY_IS_SH1106` in `config.h`, and double check `OLED_I2C_ADDRESS`
  (0x3C is standard; a few clones use 0x3D).
- **Won't connect to the adapter**: some ELM327 clones need to be
  paired once via the phone's Bluetooth settings (PIN is usually
  `1234` or `0000`) before the ESP32 can connect. Also confirm the
  adapter is a *classic* Bluetooth (SPP) adapter, not BLE-only — the
  ESP32's `BluetoothSerial` only speaks classic Bluetooth.
  Note: classic Bluetooth requires the "default" `esp32dev` chip
  variant; ESP32-S2/S3/C3 boards do not support classic Bluetooth and
  will not work with this project as-is.
- **Adapter connects but no PID data**: the car's ignition needs to be
  on (accessory power is usually enough) for the ECU to respond to
  OBD-II queries.

## Possible next steps

- Persist adapter name/MAC and calibration in NVS instead of a
  compile-time constant, with a small on-device menu.
- Add more PIDs (fuel level, intake air temp, battery voltage via
  `AT RV`) and a way to cycle between multiple gauge screens.
- Datalogging to an SD card or over WiFi.
