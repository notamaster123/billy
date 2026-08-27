#pragma once

#include <BluetoothSerial.h>

#include "vehicle_data.h"

// Talks to the ELM327 adapter directly rather than through ELMduino.
//
// Two reasons. This adapter emits ISO 9141-2 headers even after ATH0
// returns OK, so the payload is not at the start of the frame and
// parsing has to search for "41 <pid>" -- the same approach the
// browser console uses, verified against captured frames. And the
// ISO 9141-2 slow init needs a first-byte timeout measured in tens of
// seconds while still leaving the UI responsive, which needs the
// non-blocking command loop below.
class OBDManager {
public:
    void begin();
    void loop();

    void connect();     // starts a connection attempt
    void disconnect();
    void requestDtcs(); // queue a trouble-code read on the next pass

    bool isConnected() const { return state >= State::INIT; }
    bool isBusy() const { return state == State::CONNECTING; }
    const char *statusText() const;
    const VehicleData &data() const { return vehicleData; }
    VehicleData &mutableData() { return vehicleData; }

private:
    enum class State : uint8_t {
        DISCONNECTED,
        CONNECTING,
        INIT,
        POLL,
        DTC,
    };

    BluetoothSerial serialBT;
    State state = State::DISCONNECTED;
    VehicleData vehicleData;

    // Non-blocking command in flight
    bool waiting = false;
    bool lastTimedOut = false;
    String rxBuf;
    unsigned long cmdSentAt = 0;
    unsigned long lastRxAt = 0;
    unsigned long cmdTimeout = 0;

    uint8_t initStep = 0;
    uint8_t pollIndex = 0;
    uint32_t pollCycle = 0;
    uint8_t dtcStep = 0;
    bool dtcRequested = false;
    unsigned long lastAttemptAt = 0;

    void sendCommand(const char *cmd, uint32_t timeoutMs);
    bool commandComplete();

    void runInit();
    void runPoll();
    void runDtc();

    void applyReply(Metric m, const String &reply);
};
