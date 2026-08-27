#include "vehicle_data.h"

// pollEvery: RPM/speed/throttle/load every pass; temperatures rarely
// move, trims move slowly. On ISO 9141-2 a full sweep of all ten PIDs
// takes long enough that this matters.
const MetricInfo METRICS[METRIC_COUNT] = {
    /* RPM      */ {"RPM", "rpm", 0x0C, 2, 0, false, 1},
    /* SPEED    */ {"SPD", "mph", 0x0D, 1, 0, false, 1},
    /* COOLANT  */ {"CLT", "F", 0x05, 1, 0, false, 4},
    /* INTAKE   */ {"IAT", "F", 0x0F, 1, 0, false, 4},
    /* THROTTLE */ {"THR", "%", 0x11, 1, 0, false, 1},
    /* LOAD     */ {"LOAD", "%", 0x04, 1, 0, false, 1},
    /* STFT1    */ {"STFT B1", "%", 0x06, 1, 1, true, 2},
    /* LTFT1    */ {"LTFT B1", "%", 0x07, 1, 1, true, 2},
    /* STFT2    */ {"STFT B2", "%", 0x08, 1, 1, true, 2},
    /* LTFT2    */ {"LTFT B2", "%", 0x09, 1, 1, true, 2},
};

float decodeMetric(Metric m, const uint8_t *b) {
    switch (m) {
        case Metric::RPM:
            return (b[0] * 256.0f + b[1]) / 4.0f;
        case Metric::SPEED:
            return b[0] * 0.621371f;  // km/h -> mph
        case Metric::COOLANT:
        case Metric::INTAKE:
            return (b[0] - 40) * 9.0f / 5.0f + 32.0f;  // C -> F
        case Metric::THROTTLE:
        case Metric::LOAD:
            return b[0] * 100.0f / 255.0f;
        case Metric::STFT1:
        case Metric::LTFT1:
        case Metric::STFT2:
        case Metric::LTFT2:
            // 128 is "no correction"; the scale runs -100%..+99.2%.
            return (b[0] - 128) * 100.0f / 128.0f;
        default:
            return 0.0f;
    }
}
