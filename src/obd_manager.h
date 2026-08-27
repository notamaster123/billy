#pragma once

#include <BluetoothSerial.h>
#include <ELMduino.h>

#include "vehicle_data.h"

enum class ObdState {
    DISCONNECTED,
    CONNECTED,
};

// Owns the Bluetooth link to the ELM327 adapter and round-robins
// through the OBD-II PIDs we care about, one non-blocking query at a
// time, keeping the latest decoded values in `vehicleData`.
class OBDManager {
public:
    void begin();
    void loop();

    // Logs nearby classic Bluetooth devices to Serial. Blocks for
    // OBD_SCAN_DURATION_SEC; intended as a one-off setup aid to find
    // the adapter's name/MAC.
    void scanAndLogDevices();

    bool isConnected() const { return state == ObdState::CONNECTED; }
    const VehicleData &data() const { return vehicleData; }

private:
    enum class Pid { RPM, SPEED, COOLANT, THROTTLE, COUNT };

    BluetoothSerial serialBT;
    ELM327 elm;
    ObdState state = ObdState::DISCONNECTED;
    VehicleData vehicleData;
    Pid currentPid = Pid::RPM;
    unsigned long lastReconnectAttempt = 0;

    void tryConnect();
    void pollNextPid();
    void advancePid();
};
