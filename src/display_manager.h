#pragma once

#include "vehicle_data.h"

class DisplayManager {
public:
    // Initializes I2C and the OLED panel. Returns false if the panel
    // did not ACK at OLED_I2C_ADDRESS (check wiring/address).
    bool begin();

    void showSplash();
    void showConnecting();
    void showDashboard(const VehicleData &data);
    void showError(const char *message);
};
