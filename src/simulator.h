#pragma once

#include "vehicle_data.h"

// Generates a plausible drive cycle with no car and no adapter, so the
// display and menus can be built and judged at a desk.
//
// Values are physically coupled rather than independently random --
// throttle leads RPM, RPM leads speed, coolant warms once and stays --
// because uncorrelated noise makes a dashboard look fine when it is
// actually wrong. Watching speed rise as revs climb is what proves the
// layout reads correctly.
class Simulator {
public:
    void begin();
    void update(VehicleData &data);

private:
    enum class Phase { IDLE, ACCELERATE, CRUISE, DECELERATE };

    Phase phase = Phase::IDLE;
    unsigned long phaseStartedAt = 0;
    unsigned long startedAt = 0;
    unsigned long lastStep = 0;

    float rpm = 700.0f;
    float mph = 0.0f;
    float throttle = 0.0f;
    float coolantF = 90.0f;
    float intakeF = 75.0f;

    void advancePhase();
};
