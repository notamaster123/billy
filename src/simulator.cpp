#include "simulator.h"

#include <Arduino.h>

namespace {

float approach(float current, float target, float rate) {
    float delta = target - current;
    if (fabsf(delta) <= rate) {
        return target;
    }
    return current + (delta > 0 ? rate : -rate);
}

float jitter(float amplitude) {
    return (random(-1000, 1001) / 1000.0f) * amplitude;
}

}  // namespace

void Simulator::begin() {
    startedAt = millis();
    phaseStartedAt = startedAt;
    lastStep = startedAt;
    phase = Phase::IDLE;
    rpm = 700.0f;
    mph = 0.0f;
    throttle = 0.0f;
    coolantF = 90.0f;
    intakeF = 75.0f;
}

void Simulator::advancePhase() {
    unsigned long held = millis() - phaseStartedAt;

    switch (phase) {
        case Phase::IDLE:
            if (held > 4000) phase = Phase::ACCELERATE;
            break;
        case Phase::ACCELERATE:
            if (held > 6000 || mph > 55.0f) phase = Phase::CRUISE;
            break;
        case Phase::CRUISE:
            if (held > 9000) phase = Phase::DECELERATE;
            break;
        case Phase::DECELERATE:
            if (held > 5000 || mph < 1.0f) phase = Phase::IDLE;
            break;
    }
}

void Simulator::update(VehicleData &data) {
    unsigned long now = millis();
    if (now - lastStep < 50) {
        return;
    }
    float dt = (now - lastStep) / 1000.0f;
    lastStep = now;

    Phase before = phase;
    advancePhase();
    if (phase != before) {
        phaseStartedAt = now;
    }

    // Throttle drives everything downstream.
    float targetThrottle;
    switch (phase) {
        case Phase::IDLE:       targetThrottle = 0.0f;  break;
        case Phase::ACCELERATE: targetThrottle = 42.0f; break;
        case Phase::CRUISE:     targetThrottle = 16.0f; break;
        case Phase::DECELERATE: targetThrottle = 0.0f;  break;
    }
    throttle = approach(throttle, targetThrottle, 90.0f * dt);

    // Speed follows throttle; drag and engine braking pull it back.
    float accel = throttle * 0.55f - 6.0f - mph * 0.12f;
    mph += accel * dt;
    if (mph < 0.0f) mph = 0.0f;

    // RPM: idle when stopped, otherwise roughly proportional to road
    // speed with a gear ratio, plus extra revs under throttle.
    float targetRpm = (mph < 1.0f) ? 700.0f : 700.0f + mph * 42.0f + throttle * 14.0f;
    rpm = approach(rpm, targetRpm, 2200.0f * dt) + jitter(12.0f);
    if (rpm < 550.0f) rpm = 550.0f;

    // Coolant warms to thermostat temperature and holds.
    coolantF = approach(coolantF, 192.0f, 4.0f * dt) + jitter(0.3f);

    // Intake air heat-soaks at idle, cools with airflow.
    float targetIntake = 78.0f + (mph > 5.0f ? -0.35f * mph : 85.0f * (1.0f - throttle / 100.0f));
    intakeF = approach(intakeF, constrain(targetIntake, 70.0f, 170.0f), 6.0f * dt);

    data.set(Metric::RPM, rpm);
    data.set(Metric::SPEED, mph);
    data.set(Metric::THROTTLE, throttle);
    data.set(Metric::COOLANT, coolantF);
    data.set(Metric::INTAKE, intakeF);
    data.set(Metric::LOAD, constrain(12.0f + throttle * 1.5f + jitter(2.0f), 0.0f, 100.0f));

    // Short trims chase the mixture and wander; long trims drift
    // slowly. Bank 2 stays invalid, as on a single-bank engine, so the
    // "n/a" rendering gets exercised too.
    float t = (now - startedAt) / 1000.0f;
    data.set(Metric::STFT1, sinf(t * 1.7f) * 6.0f + jitter(1.2f));
    data.set(Metric::LTFT1, 3.9f + sinf(t * 0.13f) * 2.0f);

    data.dtcsRead = true;
    if (codesPresent) {
        data.milOn = true;
        data.storedCount = 2;
        data.dtcCount = 2;
        strncpy(data.dtc[0], "P0171", sizeof(data.dtc[0]));
        strncpy(data.dtc[1], "P0420", sizeof(data.dtc[1]));
    } else {
        data.milOn = false;
        data.storedCount = 0;
        data.dtcCount = 0;
    }
}
