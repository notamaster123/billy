#pragma once

#include "vehicle_data.h"

enum class Screen : uint8_t {
    DASH,
    TEMPS,
    TRIMS,
    CODES,
    STATUS,
    SETUP,
    COUNT,
};

constexpr uint8_t SCREEN_COUNT = static_cast<uint8_t>(Screen::COUNT);

// Which SETUP row the knob is editing. NONE means the knob still
// moves between screens.
enum class SetupField : uint8_t {
    NONE,
    BRIGHTNESS,
    INVERT,
    COUNT,
};

// What the chrome and the status/setup screens need, gathered so the
// display layer never reaches into the OBD or simulator internals.
struct SystemStatus {
    bool simulating = false;
    bool linkUp = false;
    const char *linkText = "";
    const char *adapterName = "";
    const char *adapterMac = "";

    uint8_t brightness = 0xCF;
    bool inverted = false;
    SetupField editing = SetupField::NONE;
};

class DisplayManager {
public:
    // Returns false if the panel did not ACK at OLED_I2C_ADDRESS.
    bool begin();

    void showSplash();
    void render(Screen screen, const VehicleData &data, const SystemStatus &status);

    // A mono panel's colour is fixed in hardware; these are the only
    // appearance controls it actually has.
    void setContrast(uint8_t level);
    void setInverted(bool inverted);

private:
    void drawChrome(Screen screen, const SystemStatus &status);
    void drawDash(const VehicleData &data);
    void drawTemps(const VehicleData &data);
    void drawTrims(const VehicleData &data);
    void drawCodes(const VehicleData &data);
    void drawStatus(const SystemStatus &status);
    void drawSetup(const SystemStatus &status);
};
