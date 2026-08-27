# Display, encoder and simulation

The gauge firmware (`pio run -e esp32dev -t upload`, the default env)
drives the OLED and a rotary encoder, and can run entirely on
simulated data so the UI can be built and judged at a desk with no car
and no adapter.

## Wiring

**OLED — Inland 1.3", SH1106, SPI**

This panel has 7 pins, which makes it the SPI variant (a 4-pin
`GND VCC SDA SCL` module would be I2C — set `DISPLAY_USE_SPI` to 0 in
`include/config.h` for that).

| OLED | ESP32 | Notes |
|------|-------|-------|
| GND  | GND | |
| VCC  | 3V3 | |
| CLK  | GPIO18 | VSPI SCK — fixed |
| MOSI | GPIO23 | VSPI MOSI — fixed |
| RES  | GPIO16 | configurable |
| DC   | GPIO17 | configurable |
| CS   | GPIO5  | configurable |

CLK and MOSI are fixed because hardware SPI uses the ESP32's VSPI
peripheral and the Adafruit driver calls `SPI.begin()` itself, which
would undo any pin remapping. To put them elsewhere, set
`DISPLAY_SPI_HARDWARE` to 0 in `config.h` to bit-bang instead — then
`OLED_CLK_PIN` and `OLED_MOSI_PIN` take effect. Slower, but a 1 KB
mono frame at 10 fps has plenty of headroom.

**Rotary encoder — KY-040 style**

| Encoder | ESP32 |
|---------|-------|
| CLK | GPIO32 |
| DT  | GPIO33 |
| SW  | GPIO25 |
| +   | 3V3 |
| GND | GND |

Those three pins are free on the WROOM-32E: not strapping pins, not
input-only, and clear of I2C. Internal pull-ups are enabled, so no
external resistors are needed. Pins are set in `include/config.h`.

If the knob scrolls backwards, swap CLK and DT.

## Controls

| Action | Effect |
|--------|--------|
| Turn | Move between screens (wraps both ways) |
| Short press | Acts on the current screen |
| Long press (0.6 s) | Toggle SIMULATION ⇄ LIVE, from any screen |

Short press does nothing on screens with no action. On **CODES** it
re-reads trouble codes; on **STATUS** it connects or disconnects the
adapter.

## Screens

Turn the knob to move between five screens. The header shows the
screen name and the mode badge (`SIM` / `LIVE` / `----`), and dots
along the bottom show your position.

1. **DASH** — RPM large, speed and throttle beneath
2. **TEMPS** — coolant large, intake air beneath, both °F
3. **TRIMS** — short and long term fuel trims, both banks, each with a
   bar growing from a centre tick
4. **CODES** — check-engine state, stored count, and the codes
5. **STATUS** — mode, link state, adapter name and MAC, and what the
   button will do

The fuel trim bars are centred rather than left-anchored because 0 %
means "the ECU is not correcting" — the meaningful reference point. A
bar filling from the left would make no correction look like a minimum
reading.

## Simulation mode

`START_IN_SIMULATION` in `include/config.h` decides the boot mode
(default: simulate). Long-press the encoder to switch at runtime.

The simulator runs a repeating drive cycle — idle, accelerate, cruise,
decelerate — with values that are **physically coupled** rather than
independently random: throttle leads RPM, RPM leads road speed,
coolant warms once to thermostat temperature and holds, intake air
heat-soaks at idle and cools with airflow.

That coupling is the point. Uncorrelated noise makes a dashboard look
fine when the layout is actually wrong; watching speed rise as the
revs climb is what proves the screen reads correctly. The simulator
also reports two trouble codes and leaves bank 2 trims invalid, so the
`--` and `n/a` paths get exercised rather than only the happy path.

## Why not ELMduino

The gauge firmware parses ELM327 replies directly instead of using
ELMduino. This adapter emits ISO 9141-2 headers even after `ATH0`
returns `OK`, so the payload is not at the start of the frame and
parsing has to search for `41 <pid>` — the same approach the browser
console uses, verified against captured frames. The ISO 9141-2 slow
init also needs a first-byte timeout in the tens of seconds while
keeping the UI responsive, which is why commands run as a non-blocking
state machine. See `HARDWARE.md`.
