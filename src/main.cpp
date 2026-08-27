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

// Formats the adapter MAC once; it never changes.
static char macText[18];

static void handleRotation(int detents) {
    if (detents == 0) {
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
            if (!simulating) {
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

        default:
            break;
    }
}

// Long press toggles simulation, from any screen.
static void handleLongPress() {
    simulating = !simulating;
    if (simulating) {
        obdManager.disconnect();
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
    displayManager.showSplash();

    encoder.begin();
    obdManager.begin();
    simulator.begin();

    snprintf(macText, sizeof(macText), "%02X:%02X:%02X:%02X:%02X:%02X", OBD_MAC_ADDRESS[0],
             OBD_MAC_ADDRESS[1], OBD_MAC_ADDRESS[2], OBD_MAC_ADDRESS[3], OBD_MAC_ADDRESS[4],
             OBD_MAC_ADDRESS[5]);

    delay(1200);

    if (!simulating) {
        obdManager.connect();
    }
}

void loop() {
    handleRotation(encoder.consumeRotation());

    switch (encoder.consumeButton()) {
        case ButtonEvent::SHORT_PRESS: handleShortPress(); break;
        case ButtonEvent::LONG_PRESS:  handleLongPress();  break;
        case ButtonEvent::NONE:        break;
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

    displayManager.render(currentScreen, simulating ? simData : obdManager.data(), status);
}
