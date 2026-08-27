#include <Arduino.h>

#include "config.h"
#include "display_manager.h"
#include "obd_manager.h"
#include "rotary_encoder.h"
#include "simulator.h"

static DisplayManager displayManager;
static OBDManager obdManager;
static RotaryEncoder encoder;
static Simulator simulator;

static Screen currentScreen = Screen::DASH;
static bool simulating = START_IN_SIMULATION;
static VehicleData simData;

static uint8_t brightness = OLED_CONTRAST;
static bool inverted = OLED_START_INVERTED;
static SetupField editing = SetupField::NONE;
static unsigned long lastPressAt = 0;

// Formats the adapter MAC once; it never changes.
static char macText[18];

static void handleRotation(int detents) {
    if (detents == 0) {
        return;
    }

    // While a SETUP row is selected the knob drives that value rather
    // than moving between screens.
    if (editing == SetupField::BRIGHTNESS) {
        int next = constrain(static_cast<int>(brightness) + detents * 16, 0, 255);
        brightness = static_cast<uint8_t>(next);
        displayManager.setContrast(brightness);
        return;
    }
    if (editing == SetupField::INVERT) {
        inverted = !inverted;
        displayManager.setInverted(inverted);
        return;
    }

    int next = (static_cast<int>(currentScreen) + detents) % SCREEN_COUNT;
    if (next < 0) {
        next += SCREEN_COUNT;
    }
    currentScreen = static_cast<Screen>(next);
}

// Short press acts on whatever screen you are looking at.
static void handleShortPress() {
    switch (currentScreen) {
        case Screen::CODES:
            if (simulating) {
                // Toggle between "codes present" and "clean" so both
                // renderings can be reviewed without a car.
                simulator.toggleCodes();
            } else {
                obdManager.requestDtcs();
            }
            break;

        case Screen::STATUS:
            if (!simulating) {
                if (obdManager.isConnected()) {
                    obdManager.disconnect();
                } else {
                    obdManager.connect();
                }
            }
            break;

        case Screen::SETUP: {
            // Cycle: navigate -> brightness -> invert -> navigate.
            uint8_t next = static_cast<uint8_t>(editing) + 1;
            if (next >= static_cast<uint8_t>(SetupField::COUNT)) {
                next = 0;
            }
            editing = static_cast<SetupField>(next);
            break;
        }

        default:
            break;
    }
}

// Long press toggles simulation, from any screen.
static void handleLongPress() {
    simulating = !simulating;
    if (simulating) {
        obdManager.shutdown();  // power the radio down, not just disconnect
        simulator.begin();
    } else {
        simData.clear();
        obdManager.connect();
    }
}

void setup() {
    Serial.begin(115200);

    if (!displayManager.begin()) {
        Serial.println("OLED not found - check wiring and I2C address");
    }
    displayManager.setContrast(brightness);
    displayManager.setInverted(inverted);
    displayManager.showSplash();

    encoder.begin();
    simulator.begin();
    // The Bluetooth radio is started lazily by connect(), so
    // simulation mode never powers it up.

    snprintf(macText, sizeof(macText), "%02X:%02X:%02X:%02X:%02X:%02X", OBD_MAC_ADDRESS[0],
             OBD_MAC_ADDRESS[1], OBD_MAC_ADDRESS[2], OBD_MAC_ADDRESS[3], OBD_MAC_ADDRESS[4],
             OBD_MAC_ADDRESS[5]);

    delay(1200);

    if (!simulating) {
        obdManager.connect();
    }
}

void loop() {
    int detents = encoder.consumeRotation();
    if (detents != 0) {
        Serial.printf("[enc] turn %+d\n", detents);
    }
    handleRotation(detents);

    // Logged and flashed unconditionally: a press that lands on a
    // screen with no action would otherwise be indistinguishable from
    // broken hardware.
    switch (encoder.consumeButton()) {
        case ButtonEvent::SHORT_PRESS:
            Serial.println("[enc] short press");
            lastPressAt = millis();
            handleShortPress();
            break;
        case ButtonEvent::LONG_PRESS:
            Serial.println("[enc] long press");
            lastPressAt = millis();
            handleLongPress();
            break;
        case ButtonEvent::NONE:
            break;
    }

    if (simulating) {
        simulator.update(simData);
    } else {
        obdManager.loop();
    }

    static unsigned long lastRender = 0;
    if (millis() - lastRender < DISPLAY_REFRESH_INTERVAL_MS) {
        return;
    }
    lastRender = millis();

    SystemStatus status;
    status.simulating = simulating;
    status.linkUp = !simulating && obdManager.isConnected();
    status.linkText = simulating ? "simulated" : obdManager.statusText();
    status.adapterName = OBD_BT_DEVICE_NAME;
    status.adapterMac = macText;
    status.brightness = brightness;
    status.inverted = inverted;
    status.editing = editing;
    status.pressFlash = (millis() - lastPressAt) < 180;

    displayManager.render(currentScreen, simulating ? simData : obdManager.data(), status);
}
