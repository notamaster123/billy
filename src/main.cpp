#include <Arduino.h>

#include "config.h"
#include "display_manager.h"
#include "obd_manager.h"

DisplayManager displayManager;
OBDManager obdManager;

void setup() {
    Serial.begin(115200);

    if (!displayManager.begin()) {
        Serial.println("OLED not found - check wiring/I2C address");
    }
    displayManager.showSplash();
    delay(1500);

    obdManager.begin();
}

void loop() {
    obdManager.loop();

    static unsigned long lastRender = 0;
    if (millis() - lastRender < DISPLAY_REFRESH_INTERVAL_MS) {
        return;
    }
    lastRender = millis();

    if (obdManager.isConnected()) {
        displayManager.showDashboard(obdManager.data());
    } else {
        displayManager.showConnecting();
    }
}
