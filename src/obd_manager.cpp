#include "obd_manager.h"
#include "config.h"

#include <Arduino.h>

void OBDManager::begin() {
    serialBT.begin(OBD_LOCAL_BT_NAME, true);

    if (sizeof(OBD_PAIRING_PIN) > 1) {
        serialBT.setPin(OBD_PAIRING_PIN, sizeof(OBD_PAIRING_PIN) - 1);
    }
}

void OBDManager::scanAndLogDevices() {
    Serial.println("Scanning for classic Bluetooth devices...");

    BTScanResults *results = serialBT.discover(OBD_SCAN_DURATION_SEC * 1000);
    if (results == nullptr) {
        Serial.println("Scan failed to start");
        return;
    }

    int count = results->getCount();
    Serial.printf("Found %d device(s):\n", count);
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice *device = results->getDevice(i);
        Serial.printf("  [%d] %-24s %s  RSSI %d\n", i,
                      device->haveName() ? device->getName().c_str() : "(no name)",
                      device->getAddress().toString().c_str(),
                      device->getRSSI());
    }
    Serial.println("Set OBD_BT_DEVICE_NAME or OBD_MAC_ADDRESS in config.h");
}

void OBDManager::loop() {
    if (state == ObdState::DISCONNECTED) {
        if (millis() - lastReconnectAttempt >= OBD_RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = millis();
            tryConnect();
        }
        return;
    }

    if (!serialBT.connected()) {
        Serial.println("Bluetooth link to OBD-II adapter dropped");
        state = ObdState::DISCONNECTED;
        vehicleData = VehicleData();
        return;
    }

    pollNextPid();
}

void OBDManager::tryConnect() {
    Serial.println("Connecting to OBD-II adapter...");

#if OBD_USE_MAC_ADDRESS
    bool linked = serialBT.connect(OBD_MAC_ADDRESS);
#else
    bool linked = serialBT.connect(OBD_BT_DEVICE_NAME);
#endif

    if (!linked) {
        Serial.println("Bluetooth connect failed, will retry");
        return;
    }

    if (!elm.begin(serialBT, false, ELM327_TIMEOUT_MS)) {
        Serial.println("ELM327 did not respond, will retry");
        serialBT.disconnect();
        return;
    }

    Serial.println("ELM327 ready");
    state = ObdState::CONNECTED;
    currentPid = Pid::RPM;
}

void OBDManager::advancePid() {
    currentPid = static_cast<Pid>((static_cast<int>(currentPid) + 1) % static_cast<int>(Pid::COUNT));
}

void OBDManager::pollNextPid() {
    switch (currentPid) {
        case Pid::RPM: {
            float value = elm.rpm();
            if (elm.nb_rx_state == ELM_SUCCESS) {
                vehicleData.rpm = static_cast<int>(value);
                vehicleData.rpmValid = true;
                advancePid();
            } else if (elm.nb_rx_state != ELM_GETTING_MSG) {
                vehicleData.rpmValid = false;
                advancePid();
            }
            break;
        }

        case Pid::SPEED: {
            int32_t value = elm.kph();
            if (elm.nb_rx_state == ELM_SUCCESS) {
                vehicleData.speedKph = static_cast<int>(value);
                vehicleData.speedValid = true;
                advancePid();
            } else if (elm.nb_rx_state != ELM_GETTING_MSG) {
                vehicleData.speedValid = false;
                advancePid();
            }
            break;
        }

        case Pid::COOLANT: {
            float value = elm.engineCoolantTemp();
            if (elm.nb_rx_state == ELM_SUCCESS) {
                vehicleData.coolantTempC = static_cast<int>(value);
                vehicleData.coolantValid = true;
                advancePid();
            } else if (elm.nb_rx_state != ELM_GETTING_MSG) {
                vehicleData.coolantValid = false;
                advancePid();
            }
            break;
        }

        case Pid::THROTTLE: {
            float value = elm.throttle();
            if (elm.nb_rx_state == ELM_SUCCESS) {
                vehicleData.throttlePct = static_cast<int>(value);
                vehicleData.throttleValid = true;
                advancePid();
            } else if (elm.nb_rx_state != ELM_GETTING_MSG) {
                vehicleData.throttleValid = false;
                advancePid();
            }
            break;
        }

        default:
            advancePid();
            break;
    }
}
