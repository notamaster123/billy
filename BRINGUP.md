# Bring-up guide

Step-by-step first contact with the OBD-II module. Work through this
in order — each step isolates one failure mode, so when something
breaks you know exactly which layer is at fault.

Everything here runs on **your** machine; the firmware talks to the
hardware, and the serial monitor is where you read the results.

## 0. Flash the bring-up tool

Plug the OBD-II module into the car, **turn the ignition on** (accessory
position is usually enough — the ECU won't answer a dead bus), and
connect the ESP32 to your computer by USB.

```
pio run -e bringup -t upload
pio device monitor
```

You should see the banner and a command list. The monitor has local
echo on, so you can type commands straight into it.

> Note: the bring-up build does **not** touch the OLED or ELMduino. If
> it works, the Bluetooth/OBD path is proven independently of the
> display and the parsing library.

## 1. Find the module — `/scan`

```
/scan
```

Lists every nearby classic Bluetooth device with name, MAC, and RSSI.

- **Module appears** → note the exact name and MAC. If the name isn't
  the default `OBDII`, put the real one in `OBD_BT_DEVICE_NAME`
  (`include/config.h`) — or set `OBD_USE_MAC_ADDRESS 1` and fill in
  `OBD_MAC_ADDRESS`, which is more reliable. Re-flash.
- **Nothing found** → check the module has power (ignition on, any LED
  lit). If it's already paired to a phone it may not be discoverable —
  disconnect it there first.
- **Found, but only over BLE on a phone scanner** → the FSC-BT825 is
  dual-mode, so it *may* be advertising BLE only. `BluetoothSerial`
  cannot reach a BLE-only device; that would mean switching to a BLE
  GATT client. Report back if this happens.

## 2. Open the link — `/connect`

```
/connect
```

- **`SPP link UP`** → the transport works. Move on.
- **`SPP link FAILED`** → try pairing from a phone first (PIN `1234`
  or `0000`), then set `OBD_PAIRING_PIN` in `config.h` and re-flash.
  Connecting by MAC instead of name also fixes a lot of these.

## 3. Probe the module — `/init`

```
/init
```

This runs the standard ELM327 startup sequence and prints every raw
reply, control characters included:

| Command | Meaning | Healthy reply |
|---------|---------|---------------|
| `ATZ`   | reset   | version banner, e.g. `ELM327 v1.5` |
| `ATE0`  | echo off | `OK` |
| `ATL0`  | linefeeds off | `OK` |
| `ATS0`  | spaces off | `OK` |
| `ATH0`  | headers off | `OK` |
| `ATSP0` | auto-detect protocol | `OK` |
| `ATI`   | adapter version | version string |
| `ATRV`  | battery voltage | e.g. `12.4V` |
| `ATDP`  | detected protocol | e.g. `AUTO, ISO 15765-4 (CAN 11/500)` |
| `0100`  | supported PIDs | `41 00 BE 3E B8 11` or similar |

How to read the outcome:

- **`ATI`/`ATZ` return a version banner** → the module speaks ELM327.
  This is the key result; everything else follows.
- **`ATRV` returns a voltage but `ATDP`/`0100` don't** → the adapter
  is alive but not talking to the ECU. Ignition off, or a protocol the
  adapter can't auto-detect. Try forcing one: `ATSP6` (CAN 11-bit
  500 kbps, most cars since ~2008), `ATSP7`, `ATSP5`.
- **`0100` returns `NO DATA` / `UNABLE TO CONNECT`** → no ECU response.
  Ignition on? Engine bus awake? Some cars need the engine actually
  running.
- **Garbage or nothing at all** → the module likely does **not** use
  the ELM327 command set. Paste the raw output and we'll work out what
  protocol it does speak — this is the scenario `HARDWARE.md` flags as
  an open question.

`/init` also decodes the `0100` bitmask into a list of the PIDs your
specific car supports. That list decides which gauges are worth
building.

## 4. Sniff live data — `/mon`

```
/mon 010C     engine RPM
/mon 010D     vehicle speed
/mon 0105     coolant temperature
/mon 0111     throttle position
```

Polls continuously; send any character to stop. Rev the engine while
monitoring `010C` and confirm the hex payload moves — that's the
end-to-end proof.

Reading the replies (mode 01 responses come back as `41 <pid> <data>`):

| PID | Reply | Formula | Example |
|-----|-------|---------|---------|
| `010C` | `41 0C A B` | `(256*A + B) / 4` RPM | `41 0C 1A F8` → 1726 rpm |
| `010D` | `41 0D A` | `A` km/h | `41 0D 41` → 65 km/h |
| `0105` | `41 05 A` | `A - 40` °C | `41 05 5A` → 50 °C |
| `0111` | `41 11 A` | `A * 100 / 255` % | `41 11 40` → 25 % |

## 5. What to send back

Paste the full serial log from `/init`, plus one `/mon 010C` sample
with the engine running. That tells us:

1. whether the module is ELM327-compatible,
2. which protocol the car uses,
3. which PIDs are actually supported,
4. the real response framing and timing.

With that, the gauge firmware (`pio run -e esp32dev`) can be tuned to
this specific module and car — including switching away from ELMduino
if the module turns out to speak something else.

## Free-form commands

Anything not starting with `/` is sent verbatim, so you can poke at the
module directly:

```
ATI            adapter identity
AT@1           device description
ATDPN          protocol number
03             read stored diagnostic trouble codes
0902           read VIN
/hex 010C      send 010C and dump the reply as hex bytes
```
