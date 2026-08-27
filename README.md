# billy

An ESP32-based OBD-II gauge cluster. A Bluetooth OBD-II module plugs
into a car's OBD-II port; an ESP32-WROOM-32E dev board connects to it
over classic Bluetooth (SPP), polls a handful of PIDs (RPM, speed,
coolant temp, throttle position), and renders them on an Inland 1.3"
OLED display.

## Hardware

- **Espressif ESP32-WROOM-32E** dev board (`esp32dev` in PlatformIO)
- **OBD-II module C210-591-307-AA** with wireless hat
  **0210-591-313-AC** (Feasycom **FSC-BT825** chip)
- **Inland 1.3" OLED**, 128x64, I2C, SH1106 driver
- Jumper wires, USB cable for flashing/power

Full part details, pinout, and datasheet findings live in
[HARDWARE.md](HARDWARE.md).

### Why Bluetooth and not WiFi

The FSC-BT825 on the wireless hat is a Bluetooth 5.3 dual-mode module
built on a Realtek RTL8761BTV — it has **no WiFi radio**, but it does
support **SPP**, the classic-Bluetooth serial profile. So the ESP32
talks to it as an SPP master over `BluetoothSerial`. A WiFi transport
would require a separate WiFi radio on the OBD module.

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

## Easiest path: the browser console

`tools/obd-console.html` is a point-and-click front end for the
bring-up firmware — no terminal needed. Flash once, then drive
everything from a web page:

```
pio run -t upload            # flash the bring-up firmware
open -a "Google Chrome" tools/obd-console.html
```

Click **Connect USB**, pick the ESP32's port, then **Scan** → click
your adapter in the list → **Probe** → **Start live data**.

It shows discovered adapters with signal bars, live gauges for RPM,
speed, coolant, throttle, engine load and intake air temp, and a
colour-coded log of every byte exchanged. There's a box for sending
raw commands too, so nothing the terminal can do is lost.

**Chrome or Edge only** — it uses WebSerial, which Safari and Firefox
don't implement. If the page loads but can't see the port, serve it
over localhost instead of `file://`:

```
python3 -m http.server -d tools 8000
# then open http://localhost:8000/obd-console.html
```

Close the page (or any `pio device monitor`) before flashing — the
serial port only allows one reader at a time.

## Prefer the terminal? Start with bring-up

Before flashing the gauge firmware, prove the Bluetooth link and find
out what the module actually speaks. There is a dedicated raw terminal
build for this:

```
pio run -e bringup -t upload
pio device monitor
```

Then follow **[BRINGUP.md](BRINGUP.md)** — scan, connect, probe, sniff.

## Software setup

This is a [PlatformIO](https://platformio.org/) project.

1. Install the PlatformIO extension for VS Code, or the `pio` CLI.
2. Open this folder in VS Code / run PlatformIO from this folder.
3. **Find the OBD module's Bluetooth name/MAC.** This isn't documented
   for the FSC-BT825 hat, so read it off the hardware: set
   `OBD_SCAN_ON_BOOT` to `1` in `include/config.h`, flash, and watch
   the serial monitor at 115200 baud. Every nearby classic Bluetooth
   device is listed with its name, MAC, and signal strength.
4. Edit `include/config.h` with what the scan found:
   - `OBD_BT_DEVICE_NAME` — the name the module broadcasts. If
     name-based connect is unreliable, set `OBD_USE_MAC_ADDRESS` to
     `1` and fill in `OBD_MAC_ADDRESS` instead.
   - `OBD_PAIRING_PIN` — only if the module demands one (`1234` and
     `0000` are the usual suspects).
   - `DISPLAY_IS_SH1106` — leave at `1` for the Inland 1.3" panel.
     Set to `0` only if you swap in a 0.96" SSD1306-based panel.
   - Set `OBD_SCAN_ON_BOOT` back to `0` once configured.
5. Build and flash the gauge firmware: `pio run -e esp32dev -t upload`.
   Open the serial monitor at 115200 baud (`pio device monitor`) to see
   connection/debug logs.

## How it works

- `src/obd_manager.*` opens a classic Bluetooth (SPP) connection to
  the OBD-II module via `BluetoothSerial`, initializes it with the
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
- **Won't connect to the module**: run the boot scan
  (`OBD_SCAN_ON_BOOT`) to confirm the name/MAC actually being
  broadcast. Some modules also need to be paired once from a phone
  (PIN `1234` or `0000`) before the ESP32 can connect — or set
  `OBD_PAIRING_PIN`. Note the FSC-BT825 is dual-mode: make sure it is
  advertising **classic/SPP**, not BLE-only, since `BluetoothSerial`
  only speaks classic Bluetooth. Classic Bluetooth also requires the
  original ESP32 (this board); ESP32-S2/S3/C3 do not support it.
- **Module connects but no PID data**: the car's ignition needs to be
  on (accessory power is usually enough) for the ECU to respond. If
  the link is up but every PID errors out, the module may not speak
  the ELM327 AT command set — see the open questions in
  [HARDWARE.md](HARDWARE.md).

## Possible next steps

- Persist adapter name/MAC and calibration in NVS instead of a
  compile-time constant, with a small on-device menu.
- Add more PIDs (fuel level, intake air temp, battery voltage via
  `AT RV`) and a way to cycle between multiple gauge screens.
- Datalogging to an SD card or over WiFi.
