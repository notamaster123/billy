#pragma once

// Snapshot of the most recently decoded OBD-II values. Each field has
// a "Valid" flag that is false until the first successful read (or
// after the adapter disconnects), so the display can show "--"
// instead of a stale or zeroed-out reading.
struct VehicleData {
    int rpm = 0;
    bool rpmValid = false;

    int speedKph = 0;
    bool speedValid = false;

    int coolantTempC = 0;
    bool coolantValid = false;

    int throttlePct = 0;
    bool throttleValid = false;
};
