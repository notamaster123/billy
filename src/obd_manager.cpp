#include "obd_manager.h"

#include "config.h"

#include <Arduino.h>

namespace {

const char *INIT_COMMANDS[] = {
    "ATZ",    // reset
    "ATE0",   // echo off
    "ATL0",   // no linefeeds
    "ATS0",   // no spaces
    "ATH0",   // headers off (this adapter sends them anyway)
    "ATSP0",  // auto-detect protocol
};
constexpr uint8_t INIT_COMMAND_COUNT = sizeof(INIT_COMMANDS) / sizeof(INIT_COMMANDS[0]);

// Replies the ELM327 sends instead of data. Checked before hex
// extraction, because "NO DATA" is made of characters that would
// otherwise survive a hex filter and decode into nonsense.
bool isErrorReply(const String &reply) {
    return reply.indexOf("NO DATA") >= 0 || reply.indexOf("UNABLE") >= 0 ||
           reply.indexOf("STOPPED") >= 0 || reply.indexOf("ERROR") >= 0 ||
           reply.indexOf("SEARCHING") >= 0 || reply.indexOf("BUS INIT") >= 0 ||
           reply.indexOf('?') >= 0;
}

String compactHex(const String &s) {
    String out;
    out.reserve(s.length());
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isxdigit(static_cast<unsigned char>(c))) {
            out += static_cast<char>(toupper(c));
        }
    }
    return out;
}

// Finds "41<pid>" anywhere in the frame and reads the data bytes that
// follow. The leading ISO 9141-2 header means position is not fixed.
bool extractPid(const String &hex, uint8_t pid, uint8_t count, uint8_t *out) {
    char tag[5];
    snprintf(tag, sizeof(tag), "41%02X", pid);

    int at = hex.indexOf(tag);
    if (at < 0) {
        return false;
    }
    int start = at + 4;
    if (hex.length() < static_cast<unsigned>(start + count * 2)) {
        return false;
    }
    for (uint8_t i = 0; i < count; i++) {
        char byteText[3] = {hex[start + i * 2], hex[start + i * 2 + 1], '\0'};
        out[i] = static_cast<uint8_t>(strtoul(byteText, nullptr, 16));
    }
    return true;
}

// Two bytes per DTC: top two bits select the letter, next two are the
// first digit, remaining 12 bits are three hex digits. 0000 is padding.
bool decodeDtc(uint8_t a, uint8_t b, char *out) {
    if (a == 0 && b == 0) {
        return false;
    }
    const char letters[] = "PCBU";
    snprintf(out, 6, "%c%u%X%X%X", letters[(a >> 6) & 0x03], (a >> 4) & 0x03, a & 0x0F,
             (b >> 4) & 0x0F, b & 0x0F);
    return true;
}

}  // namespace

void OBDManager::begin() {
    serialBT.begin(OBD_LOCAL_BT_NAME, true);
}

const char *OBDManager::statusText() const {
    switch (state) {
        case State::DISCONNECTED: return "not connected";
        case State::CONNECTING:   return "connecting...";
        case State::INIT:         return "initialising";
        case State::POLL:         return "streaming";
        case State::DTC:          return "reading codes";
    }
    return "";
}

void OBDManager::connect() {
    if (state != State::DISCONNECTED) {
        return;
    }
    state = State::CONNECTING;
    lastAttemptAt = millis();
}

void OBDManager::disconnect() {
    serialBT.disconnect();
    state = State::DISCONNECTED;
    waiting = false;
    vehicleData.clear();
}

void OBDManager::requestDtcs() {
    dtcRequested = true;
}

void OBDManager::sendCommand(const char *cmd, uint32_t timeoutMs) {
    while (serialBT.available()) {
        serialBT.read();  // drop anything left from the previous reply
    }
    serialBT.print(cmd);
    serialBT.print('\r');

    rxBuf = "";
    waiting = true;
    lastTimedOut = false;
    cmdSentAt = millis();
    lastRxAt = cmdSentAt;
    cmdTimeout = timeoutMs;
}

// Non-blocking: call every loop until it returns true. The timeout
// applies to the first byte; once bytes are arriving an idle gap ends
// the read, since a slow init stalls and then bursts.
bool OBDManager::commandComplete() {
    if (!waiting) {
        return true;
    }

    while (serialBT.available()) {
        char c = static_cast<char>(serialBT.read());
        rxBuf += c;
        lastRxAt = millis();
        if (c == '>') {
            waiting = false;
            return true;
        }
    }

    if (rxBuf.length() > 0 && millis() - lastRxAt > 1000) {
        waiting = false;
        return true;
    }
    if (millis() - cmdSentAt > cmdTimeout) {
        waiting = false;
        lastTimedOut = true;
        return true;
    }
    return false;
}

void OBDManager::applyReply(Metric m, const String &reply) {
    const MetricInfo &info = METRICS[static_cast<uint8_t>(m)];
    uint8_t idx = static_cast<uint8_t>(m);

    if (lastTimedOut || isErrorReply(reply)) {
        vehicleData.valid[idx] = false;
        return;
    }

    uint8_t bytes[4];
    if (!extractPid(compactHex(reply), info.pid, info.bytes, bytes)) {
        vehicleData.valid[idx] = false;
        return;
    }
    vehicleData.set(m, decodeMetric(m, bytes));
}

void OBDManager::runInit() {
    if (!commandComplete()) {
        return;
    }

    if (initStep >= INIT_COMMAND_COUNT) {
        state = State::POLL;
        pollIndex = 0;
        pollCycle = 1;
        Serial.println("ELM327 ready, streaming");
        return;
    }

    // ATZ resets the adapter and takes noticeably longer than the rest.
    uint32_t timeout = (initStep == 0) ? 6000 : 3000;
    sendCommand(INIT_COMMANDS[initStep], timeout);
    initStep++;
}

void OBDManager::runPoll() {
    if (!commandComplete()) {
        return;
    }

    // Attribute the reply to whatever was asked for last.
    if (pollIndex > 0) {
        applyReply(static_cast<Metric>(pollIndex - 1), rxBuf);
    }

    if (dtcRequested) {
        dtcRequested = false;
        state = State::DTC;
        dtcStep = 0;
        return;
    }

    // Find the next metric due on this cycle.
    while (pollIndex < METRIC_COUNT) {
        const MetricInfo &info = METRICS[pollIndex];
        if (pollCycle % info.pollEvery == 0) {
            break;
        }
        pollIndex++;
    }

    if (pollIndex >= METRIC_COUNT) {
        pollIndex = 0;
        pollCycle++;
        return;
    }

    char cmd[8];
    snprintf(cmd, sizeof(cmd), "01%02X", METRICS[pollIndex].pid);
    // The first query after connecting triggers the ISO 9141-2 slow
    // init, which can take far longer than a steady-state read.
    sendCommand(cmd, (pollCycle == 1) ? 15000 : 4000);
    pollIndex++;
}

void OBDManager::runDtc() {
    if (!commandComplete()) {
        return;
    }

    switch (dtcStep) {
        case 0:
            sendCommand("0101", 5000);
            dtcStep++;
            return;

        case 1: {
            uint8_t bytes[1];
            if (!lastTimedOut && !isErrorReply(rxBuf) &&
                extractPid(compactHex(rxBuf), 0x01, 1, bytes)) {
                vehicleData.milOn = (bytes[0] & 0x80) != 0;
                vehicleData.storedCount = bytes[0] & 0x7F;
            }
            vehicleData.dtcCount = 0;
            sendCommand("03", 8000);
            dtcStep++;
            return;
        }

        case 2: {
            if (!lastTimedOut && !isErrorReply(rxBuf)) {
                String hex = compactHex(rxBuf);
                // Walk each frame from its own "43" marker: long lists
                // are split across frames, each with its own header.
                int at = hex.indexOf("43");
                while (at >= 0 && vehicleData.dtcCount < MAX_DTCS) {
                    int p = at + 2;
                    while (p + 4 <= static_cast<int>(hex.length()) &&
                           vehicleData.dtcCount < MAX_DTCS) {
                        char aText[3] = {hex[p], hex[p + 1], '\0'};
                        char bText[3] = {hex[p + 2], hex[p + 3], '\0'};
                        uint8_t a = static_cast<uint8_t>(strtoul(aText, nullptr, 16));
                        uint8_t b = static_cast<uint8_t>(strtoul(bText, nullptr, 16));
                        char code[6];
                        if (decodeDtc(a, b, code)) {
                            bool seen = false;
                            for (uint8_t i = 0; i < vehicleData.dtcCount; i++) {
                                if (strcmp(vehicleData.dtc[i], code) == 0) seen = true;
                            }
                            if (!seen) {
                                strncpy(vehicleData.dtc[vehicleData.dtcCount], code, 6);
                                vehicleData.dtcCount++;
                            }
                        }
                        p += 4;
                    }
                    at = hex.indexOf("43", at + 2);
                }
            }
            vehicleData.dtcsRead = true;
            state = State::POLL;
            pollIndex = 0;
            return;
        }

        default:
            state = State::POLL;
            return;
    }
}

void OBDManager::loop() {
    switch (state) {
        case State::DISCONNECTED:
            return;

        case State::CONNECTING: {
            // connect() blocks for several seconds. Give the UI one
            // frame first, otherwise the screen freezes on the old
            // content and never shows "connecting...".
            if (millis() - lastAttemptAt < 150) {
                return;
            }

            // ESP_SPP_SEC_NONE: this adapter refuses connect()'s
            // authenticated default. See HARDWARE.md.
            bool linked = serialBT.connect(OBD_MAC_ADDRESS, 0, ESP_SPP_SEC_NONE);
            if (!linked) {
                Serial.println("Bluetooth connect failed");
                state = State::DISCONNECTED;
                return;
            }
            Serial.println("SPP link up");
            state = State::INIT;
            initStep = 0;
            waiting = false;
            return;
        }

        case State::INIT:
        case State::POLL:
        case State::DTC:
            if (!serialBT.connected()) {
                Serial.println("Bluetooth link dropped");
                state = State::DISCONNECTED;
                vehicleData.clear();
                return;
            }
            if (state == State::INIT) runInit();
            else if (state == State::POLL) runPoll();
            else runDtc();
            return;
    }
}
