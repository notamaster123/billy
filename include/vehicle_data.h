#pragma once

#include <Arduino.h>

// Everything the dashboard can show, in one indexable set so the UI
// can iterate over metrics rather than hard-coding a field per screen.
enum class Metric : uint8_t {
    RPM,
    SPEED,
    COOLANT,
    INTAKE,
    THROTTLE,
    LOAD,
    STFT1,
    LTFT1,
    STFT2,
    LTFT2,
    COUNT,
};

constexpr uint8_t METRIC_COUNT = static_cast<uint8_t>(Metric::COUNT);

struct MetricInfo {
    const char *label;  // short enough for a 21-column display
    const char *unit;
    uint8_t pid;        // mode 01 PID
    uint8_t bytes;      // data bytes in the reply
    uint8_t decimals;
    bool bipolar;       // signed, centred on zero (fuel trims)
    uint8_t pollEvery;  // poll on every Nth cycle; slow values get a
                        // larger number so RPM does not lag behind
                        // coolant on a slow bus
};

extern const MetricInfo METRICS[METRIC_COUNT];

// Converts raw OBD data bytes into display units (Fahrenheit, mph).
float decodeMetric(Metric m, const uint8_t *bytes);

constexpr uint8_t MAX_DTCS = 8;

struct VehicleData {
    float value[METRIC_COUNT];
    bool valid[METRIC_COUNT];

    bool milOn;              // check engine light
    uint8_t storedCount;     // DTC count reported by PID 01
    char dtc[MAX_DTCS][6];   // e.g. "P0171"
    uint8_t dtcCount;        // how many of dtc[] are populated
    bool dtcsRead;           // false until a mode 03 query has run

    VehicleData() { clear(); }

    void clear() {
        for (uint8_t i = 0; i < METRIC_COUNT; i++) {
            value[i] = 0.0f;
            valid[i] = false;
        }
        milOn = false;
        storedCount = 0;
        dtcCount = 0;
        dtcsRead = false;
        for (uint8_t i = 0; i < MAX_DTCS; i++) {
            dtc[i][0] = '\0';
        }
    }

    float get(Metric m) const { return value[static_cast<uint8_t>(m)]; }
    bool has(Metric m) const { return valid[static_cast<uint8_t>(m)]; }

    void set(Metric m, float v) {
        value[static_cast<uint8_t>(m)] = v;
        valid[static_cast<uint8_t>(m)] = true;
    }
};
