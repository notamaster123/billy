#pragma once

#include "vehicle_data.h"

enum class Screen : uint8_t {
    DASH,
    TEMPS,
    TRIMS,
    CODES,
    STATUS,
    COUNT,
};

constexpr uint8_t SCREEN_COUNT = static_cast<uint8_t>(Screen::COUNT);

// What the status screen needs to render, gathered so the display
// layer never has to reach into the OBD or simulator internals.
struct SystemStatus {
    bool simulating = false;
    bool linkUp = false;
    const char *linkText = "";
    const char *adapterName = "";
    const char *adapterMac = "";
};

class DisplayManager {
public:
    // Returns false if the panel did not ACK at OLED_I2C_ADDRESS.
    bool begin();

    void showSplash();
    void render(Screen screen, const VehicleData &data, const SystemStatus &status);

private:
    void drawChrome(Screen screen, const SystemStatus &status);
    void drawDash(const VehicleData &data);
    void drawTemps(const VehicleData &data);
    void drawTrims(const VehicleData &data);
    void drawCodes(const VehicleData &data);
    void drawStatus(const SystemStatus &status);
};
