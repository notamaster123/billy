// ============================================================
// Bring-up / sniffing tool
//
// Build with:  pio run -e bringup -t upload && pio device monitor
//
// This is deliberately a RAW terminal over Bluetooth SPP. It does not
// use ELMduino, because at bring-up we have not yet confirmed the
// module speaks the ELM327 AT command set at all -- a parsing library
// would hide the module's actual replies behind decoded values. Here
// every byte the module sends is printed verbatim.
//
// Type "/help" in the serial monitor for the command list.
// ============================================================

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <esp_gap_bt_api.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

static BluetoothSerial SerialBT;
static bool linkUp = false;

// Last /scan results, so /connect can target one by index without
// rebuilding to change config.h.
struct ScanEntry {
    String name;
    uint8_t mac[6];
};
static const int MAX_SCAN_ENTRIES = 20;
static ScanEntry scanTable[MAX_SCAN_ENTRIES];
static int scanCount = 0;

// Legacy pairing PIN, changeable at runtime so a wrong guess costs a
// retry rather than a rebuild.
static String currentPin = OBD_PAIRING_PIN;

// Must be called after SerialBT.begin() -- the GAP call underneath
// needs an initialized Bluetooth stack.
static void applyPin() {
    if (currentPin.length() == 0) {
        Serial.println("No pairing PIN set (use /pin 1234)");
        return;
    }
    // setPin() gained a length argument in Arduino-ESP32 core 3.x.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    SerialBT.setPin(currentPin.c_str(), currentPin.length());
#else
    SerialBT.setPin(currentPin.c_str());
#endif
    Serial.printf("Pairing PIN set to \"%s\"\n", currentPin.c_str());
}

// A failed pairing attempt can leave a half-formed bond cached, which
// makes every later attempt fail the same way. Clearing it forces a
// fresh pairing handshake.
static void doUnpair() {
    int num = esp_bt_gap_get_bond_device_num();
    if (num <= 0) {
        Serial.println("No bonded devices to remove");
        return;
    }

    esp_bd_addr_t *list = static_cast<esp_bd_addr_t *>(malloc(sizeof(esp_bd_addr_t) * num));
    if (list == nullptr) {
        Serial.println("Out of memory listing bonds");
        return;
    }

    if (esp_bt_gap_get_bond_device_list(&num, list) == ESP_OK) {
        for (int i = 0; i < num; i++) {
            Serial.printf("Removing bond %02X:%02X:%02X:%02X:%02X:%02X ... %s\n",
                          list[i][0], list[i][1], list[i][2], list[i][3], list[i][4], list[i][5],
                          esp_bt_gap_remove_bond_device(list[i]) == ESP_OK ? "ok" : "failed");
        }
    } else {
        Serial.println("Could not read the bond list");
    }
    free(list);
}

// Scanning and connecting block for seconds at a time. Anything typed
// meanwhile queues in the UART buffer and merges into the next command,
// so drop it rather than executing a corrupted line.
static void flushSerialInput() {
    while (Serial.available()) {
        Serial.read();
    }
}

// Parses "dc:0d:30:a9:c0:5e" into 6 bytes.
static bool parseMac(const String &s, uint8_t out[6]) {
    unsigned int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        if (v[i] > 0xFF) {
            return false;
        }
        out[i] = static_cast<uint8_t>(v[i]);
    }
    return true;
}

// ------------------------------------------------------------
// Output helpers
// ------------------------------------------------------------

// Prints a response with control characters made visible, so CR/LF
// framing and the ELM327 '>' prompt are unambiguous in the log.
static void printEscaped(const String &s) {
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '\r') {
            Serial.print("<CR>");
        } else if (c == '\n') {
            Serial.println("<LF>");
        } else if (c == '>') {
            Serial.print("<PROMPT>");
        } else if (isprint(static_cast<unsigned char>(c))) {
            Serial.print(c);
        } else {
            Serial.printf("<%02X>", static_cast<uint8_t>(c));
        }
    }
    Serial.println();
}

static void printHex(const String &s) {
    Serial.print("  hex:");
    for (size_t i = 0; i < s.length(); i++) {
        Serial.printf(" %02X", static_cast<uint8_t>(s[i]));
    }
    Serial.println();
}

// ------------------------------------------------------------
// Bluetooth link
// ------------------------------------------------------------

static void doScan() {
    Serial.printf("Scanning %d s for classic Bluetooth devices (don't type)...\n",
                  OBD_SCAN_DURATION_SEC);

    BTScanResults *results = SerialBT.discover(OBD_SCAN_DURATION_SEC * 1000);
    flushSerialInput();
    if (results == nullptr) {
        Serial.println("  scan failed to start");
        return;
    }

    int count = results->getCount();
    Serial.printf("Found %d device(s):\n", count);
    scanCount = 0;
    int bestRssi = -127;
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice *d = results->getDevice(i);
        String addr = d->getAddress().toString().c_str();
        String name = d->haveName() ? String(d->getName().c_str()) : String("(no name)");
        int rssi = d->getRSSI();
        if (rssi > bestRssi) {
            bestRssi = rssi;
        }

        // An inquiry reply can get through at a signal level far too
        // weak for the multi-round pairing handshake, so flag it.
        const char *quality = rssi > -70 ? "good" : (rssi > -85 ? "weak" : "TOO WEAK");
        Serial.printf("  [%d] %-24s %s  RSSI %d (%s)\n", i, name.c_str(), addr.c_str(), rssi,
                      quality);

        if (scanCount < MAX_SCAN_ENTRIES && parseMac(addr, scanTable[scanCount].mac)) {
            scanTable[scanCount].name = name;
            scanCount++;
        }
    }

    if (count == 0) {
        Serial.println("  Nothing found. Is the module powered (ignition on)");
        Serial.println("  and not already paired to a phone?");
        return;
    }

    if (bestRssi <= -85) {
        Serial.println();
        Serial.println("  !! Everything here is very weak. Pairing needs a better link");
        Serial.println("  !! than discovery does, so move the ESP32 next to the module");
        Serial.println("  !! and scan again before trying to connect.");
    }
    Serial.println("  Connect with /connect <index>, e.g. /connect 1");
}

static void printConnectResult() {
    if (linkUp) {
        Serial.println("SPP link UP");
        return;
    }
    Serial.println("SPP link FAILED on every security mode.");
    Serial.println("  Next things to rule out:");
    Serial.println("    /unpair     clear a cached half-formed bond, then retry");
    Serial.println("    - is another device (phone, Mac) holding the module?");
    Serial.println("      it accepts exactly one connection at a time");
    Serial.printf("    /trypins 0  if it really does want a PIN (current: \"%s\")\n",
                  currentPin.c_str());
}

// Connects by MAC. Preferred over name: it skips the SDP name lookup,
// which is the flaky part with most ELM327 clones.
//
// connect() defaults to demanding ESP_SPP_SEC_ENCRYPT|AUTHENTICATE.
// Plenty of these adapters publish SPP with no security and refuse
// that demand -- which surfaces as "authentication failed", the same
// error a wrong PIN gives. So work down from secure to open, and
// report which combination actually took.
static bool connectToMac(const uint8_t mac[6], const char *label) {
    struct Attempt {
        const char *description;
        int channel;
        esp_spp_sec_t sec;
    };
    static const Attempt attempts[] = {
        {"no security, auto channel", 0, ESP_SPP_SEC_NONE},
        {"no security, channel 1", 1, ESP_SPP_SEC_NONE},
        {"authenticated + encrypted, auto channel", 0,
         static_cast<esp_spp_sec_t>(ESP_SPP_SEC_ENCRYPT | ESP_SPP_SEC_AUTHENTICATE)},
    };
    const int attemptCount = sizeof(attempts) / sizeof(attempts[0]);

    Serial.printf("Connecting to %s [%02X:%02X:%02X:%02X:%02X:%02X]\n", label, mac[0], mac[1],
                  mac[2], mac[3], mac[4], mac[5]);

    for (int i = 0; i < attemptCount; i++) {
        Serial.printf("  [%d/%d] %s ... ", i + 1, attemptCount, attempts[i].description);

        uint8_t addr[6];
        memcpy(addr, mac, 6);  // connect() takes a non-const pointer
        linkUp = SerialBT.connect(addr, attempts[i].channel, attempts[i].sec);

        if (linkUp) {
            Serial.println("OK");
            Serial.printf("SPP link UP (%s)\n", attempts[i].description);
            return true;
        }
        Serial.println("failed");

        SerialBT.disconnect();
        delay(800);
    }

    printConnectResult();
    return false;
}

// Resolves a /command argument to a MAC: a scan index, an explicit
// MAC, or (empty) the one in config.h.
static bool resolveTarget(const String &arg, uint8_t out[6], String &label) {
    if (arg.length() == 0) {
        memcpy(out, OBD_MAC_ADDRESS, 6);
        label = "config.h MAC";
        return true;
    }

    if (arg.indexOf(':') >= 0) {
        if (!parseMac(arg, out)) {
            Serial.println("Could not parse that MAC (expected aa:bb:cc:dd:ee:ff)");
            return false;
        }
        label = arg;
        return true;
    }

    for (size_t i = 0; i < arg.length(); i++) {
        if (!isdigit(static_cast<unsigned char>(arg[i]))) {
            Serial.println("Expected a scan index or a MAC");
            return false;
        }
    }

    int idx = arg.toInt();
    if (idx < 0 || idx >= scanCount) {
        Serial.printf("No scan entry [%d]. Run /scan first (%d known).\n", idx, scanCount);
        return false;
    }
    memcpy(out, scanTable[idx].mac, 6);
    label = scanTable[idx].name;
    return true;
}

// Works through the PINs these modules commonly ship with, clearing
// any bond between attempts, and stops at the first success. Beats
// hand-cycling: each attempt needs an unpair and a settle, and getting
// that order wrong makes a correct PIN look wrong.
static void doTryPins(const String &arg) {
    if (linkUp) {
        Serial.println("Already connected. /disconnect first.");
        return;
    }

    uint8_t mac[6];
    String label;
    if (!resolveTarget(arg, mac, label)) {
        return;
    }

    static const char *candidates[] = {"1234", "0000", "6789", "1111", "5678", "123456"};
    const int candidateCount = sizeof(candidates) / sizeof(candidates[0]);

    Serial.printf("Trying %d PINs against %s [%02X:%02X:%02X:%02X:%02X:%02X]\n", candidateCount,
                  label.c_str(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    for (int i = 0; i < candidateCount; i++) {
        Serial.printf("\n--- [%d/%d] PIN \"%s\" ---\n", i + 1, candidateCount, candidates[i]);

        doUnpair();
        currentPin = candidates[i];
        applyPin();
        delay(300);

        uint8_t addr[6];
        memcpy(addr, mac, 6);
        linkUp = SerialBT.connect(addr);

        if (linkUp) {
            Serial.printf("\n*** SPP link UP with PIN \"%s\" ***\n", candidates[i]);
            Serial.println("Put this in include/config.h as OBD_PAIRING_PIN, then /init");
            flushSerialInput();
            return;
        }

        Serial.println("  failed");
        SerialBT.disconnect();
        delay(1000);
    }

    Serial.println("\nNone of the common PINs worked.");
    Serial.println("Next: /ssp  (some modules want Secure Simple Pairing, not a PIN)");
    flushSerialInput();
}

// Re-initializes the stack with Secure Simple Pairing enabled. Modules
// that negotiate SSP reject legacy PIN pairing with the same auth
// failure a wrong PIN produces, so the two are worth separating.
// One-way until the next reset, as the core has no disableSSP().
static void doEnableSsp() {
    Serial.println("Restarting Bluetooth with SSP enabled...");
    SerialBT.end();
    delay(200);
    SerialBT.enableSSP();
    SerialBT.begin(OBD_LOCAL_BT_NAME, true);
    delay(500);
    Serial.println("SSP mode on (press EN to get back to PIN mode). Now /connect <index>");
    Serial.println("Note: /scan results are cleared, so scan again first.");
    scanCount = 0;
}

// arg is empty (use config.h), a scan index, a MAC, or a device name.
static bool doConnect(const String &arg) {
    if (linkUp) {
        Serial.println("Already connected. /disconnect first.");
        return true;
    }

    // A scan leaves the controller busy for a moment; connecting
    // immediately afterwards tends to fail spuriously.
    delay(500);
    flushSerialInput();

    if (arg.length() > 0) {
        if (arg.indexOf(':') >= 0) {
            uint8_t mac[6];
            if (!parseMac(arg, mac)) {
                Serial.println("Could not parse that MAC (expected aa:bb:cc:dd:ee:ff)");
                return false;
            }
            return connectToMac(mac, arg.c_str());
        }

        bool numeric = true;
        for (size_t i = 0; i < arg.length(); i++) {
            if (!isdigit(static_cast<unsigned char>(arg[i]))) {
                numeric = false;
                break;
            }
        }

        if (numeric) {
            int idx = arg.toInt();
            if (idx < 0 || idx >= scanCount) {
                Serial.printf("No scan entry [%d]. Run /scan first (%d known).\n", idx, scanCount);
                return false;
            }
            return connectToMac(scanTable[idx].mac, scanTable[idx].name.c_str());
        }

        Serial.printf("Connecting to name \"%s\" ...\n", arg.c_str());
        linkUp = SerialBT.connect(arg);
        printConnectResult();
        return linkUp;
    }

#if OBD_USE_MAC_ADDRESS
    return connectToMac(OBD_MAC_ADDRESS, "config.h MAC");
#else
    Serial.printf("Connecting to \"%s\" (from config.h) ...\n", OBD_BT_DEVICE_NAME);
    linkUp = SerialBT.connect(OBD_BT_DEVICE_NAME);
    printConnectResult();
    return linkUp;
#endif
}

// Reads until the ELM327 '>' prompt or timeout. Returns everything
// received, prompt included, so nothing is silently dropped.
static String readReply(uint32_t timeoutMs) {
    String buf;
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        while (SerialBT.available()) {
            char c = static_cast<char>(SerialBT.read());
            buf += c;
            if (c == '>') {
                return buf;
            }
        }
        delay(1);
    }
    return buf;
}

// Sends one command (CR-terminated, as ELM327 expects) and prints the
// raw reply. Returns the reply for callers that want to parse it.
static String sendCommand(const String &cmd, uint32_t timeoutMs = ELM327_TIMEOUT_MS, bool hex = false) {
    if (!linkUp) {
        Serial.println("Not connected. /connect first.");
        return "";
    }

    while (SerialBT.available()) {  // drop anything stale
        SerialBT.read();
    }

    Serial.printf(">> %s\n", cmd.c_str());
    SerialBT.print(cmd);
    SerialBT.print('\r');

    String reply = readReply(timeoutMs);
    if (reply.length() == 0) {
        Serial.println("<< (no response - timeout)");
    } else {
        Serial.print("<< ");
        printEscaped(reply);
        if (hex) {
            printHex(reply);
        }
    }
    return reply;
}

// ------------------------------------------------------------
// OBD helpers
// ------------------------------------------------------------

// Strips spaces/CR/LF/prompt so responses can be pattern-matched.
static String compact(const String &s) {
    String out;
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c != ' ' && c != '\r' && c != '\n' && c != '>') {
            out += static_cast<char>(toupper(c));
        }
    }
    return out;
}

static const char *pidName(uint8_t pid) {
    switch (pid) {
        case 0x01: return "Monitor status since DTCs cleared";
        case 0x03: return "Fuel system status";
        case 0x04: return "Calculated engine load";
        case 0x05: return "Engine coolant temperature";
        case 0x06: return "Short term fuel trim B1";
        case 0x07: return "Long term fuel trim B1";
        case 0x0A: return "Fuel pressure";
        case 0x0B: return "Intake manifold abs pressure";
        case 0x0C: return "Engine RPM";
        case 0x0D: return "Vehicle speed";
        case 0x0E: return "Timing advance";
        case 0x0F: return "Intake air temperature";
        case 0x10: return "MAF air flow rate";
        case 0x11: return "Throttle position";
        case 0x1C: return "OBD standards";
        case 0x1F: return "Run time since engine start";
        case 0x20: return "PIDs supported 21-40";
        default:   return "";
    }
}

// Decodes the 4-byte bitmask from mode 01 PID 00 into a list of the
// PIDs this car actually supports.
static void decodeSupportedPids(const String &reply) {
    String c = compact(reply);
    int idx = c.indexOf("4100");
    if (idx < 0) {
        Serial.println("  (no '41 00' in reply - cannot decode supported PIDs)");
        return;
    }

    String payload = c.substring(idx + 4);
    if (payload.length() < 8) {
        Serial.println("  (reply too short to decode)");
        return;
    }

    uint32_t mask = strtoul(payload.substring(0, 8).c_str(), nullptr, 16);
    Serial.printf("  Supported PID bitmask: %08X\n", mask);
    Serial.println("  Car supports:");
    for (int bit = 0; bit < 32; bit++) {
        if (mask & (1UL << (31 - bit))) {
            uint8_t pid = static_cast<uint8_t>(bit + 1);
            const char *name = pidName(pid);
            Serial.printf("    01%02X  %s\n", pid, name[0] ? name : "(see OBD-II PID table)");
        }
    }
}

// Standard ELM327 init sequence. Each step is printed raw so a module
// that deviates from the ELM327 command set is immediately obvious.
static void doInit() {
    Serial.println("--- ELM327 init sequence ---");
    sendCommand("ATZ", 5000);    // reset (slow; returns version banner)
    delay(1000);
    sendCommand("ATE0");         // echo off
    sendCommand("ATL0");         // linefeeds off
    sendCommand("ATS0");         // spaces off
    sendCommand("ATH0");         // headers off
    sendCommand("ATSP0", 5000);  // auto-detect protocol
    Serial.println("--- identity / link ---");
    sendCommand("ATI");          // version string
    sendCommand("ATRV");         // battery voltage (adapter-side, no ECU needed)
    Serial.println("--- vehicle ---");
    sendCommand("ATDP", 5000);   // describe detected protocol
    String supported = sendCommand("0100", 5000);
    decodeSupportedPids(supported);
    Serial.println("--- done ---");
}

// Polls one PID repeatedly until a key is pressed.
static void doMonitor(const String &pid) {
    Serial.printf("Monitoring %s - send any character to stop\n", pid.c_str());
    while (Serial.available()) {
        Serial.read();
    }

    while (!Serial.available()) {
        sendCommand(pid, ELM327_TIMEOUT_MS);
        delay(200);
        if (!SerialBT.connected()) {
            Serial.println("Link dropped, stopping monitor");
            linkUp = false;
            return;
        }
    }
    while (Serial.available()) {
        Serial.read();
    }
    Serial.println("Monitor stopped");
}

// ------------------------------------------------------------
// Command loop
// ------------------------------------------------------------

static void printHelp() {
    Serial.println();
    Serial.println("=== OBD-II bring-up terminal ===");
    Serial.println("  /scan            list nearby classic Bluetooth devices");
    Serial.println("  /connect         open the SPP link (target from config.h)");
    Serial.println("  /connect <n>     connect to scan result n, e.g. /connect 1");
    Serial.println("  /connect <mac>   connect to an explicit aa:bb:cc:dd:ee:ff");
    Serial.println("  /pin <code>      set the pairing PIN, e.g. /pin 1234");
    Serial.println("  /trypins <n>     try the common PINs against scan entry n");
    Serial.println("  /ssp             retry using Secure Simple Pairing instead");
    Serial.println("  /unpair          forget all bonds (clears a failed pairing)");
    Serial.println("  /disconnect      close the SPP link");
    Serial.println("  /status          show link state");
    Serial.println("  /init            run the standard ELM327 init + probe");
    Serial.println("  /pids            re-read supported PIDs (01 00)");
    Serial.println("  /mon <cmd>       poll a command repeatedly (e.g. /mon 010C)");
    Serial.println("  /hex <cmd>       send a command, show reply in hex too");
    Serial.println("  /help            this list");
    Serial.println();
    Serial.println("  Anything else is sent verbatim to the module.");
    Serial.println("  Useful raw commands:");
    Serial.println("    ATI    adapter version     ATRV   battery voltage");
    Serial.println("    010C   engine RPM          010D   vehicle speed");
    Serial.println("    0105   coolant temp        0111   throttle position");
    Serial.println("    03     read stored DTCs");
    Serial.println();
}

static void handleLine(String line) {
    // Strip control characters before trimming: a stray byte inside an
    // argument silently changes how it is interpreted.
    String clean;
    for (size_t i = 0; i < line.length(); i++) {
        if (static_cast<unsigned char>(line[i]) >= 0x20) {
            clean += line[i];
        }
    }
    clean.trim();
    line = clean;

    if (line.length() == 0) {
        return;
    }

    // Echo what was actually parsed, so a garbled line is visible
    // rather than quietly doing the wrong thing.
    Serial.printf("[cmd] \"%s\"\n", line.c_str());

    if (!line.startsWith("/")) {
        sendCommand(line);
        return;
    }

    if (line == "/help") {
        printHelp();
    } else if (line == "/scan") {
        doScan();
    } else if (line == "/connect") {
        doConnect("");
    } else if (line.startsWith("/connect ")) {
        String arg = line.substring(9);
        arg.trim();
        doConnect(arg);
    } else if (line == "/pin") {
        Serial.printf("Current pairing PIN: \"%s\" (set with /pin 1234)\n", currentPin.c_str());
    } else if (line.startsWith("/pin ")) {
        currentPin = line.substring(5);
        currentPin.trim();
        applyPin();
    } else if (line == "/unpair") {
        doUnpair();
    } else if (line == "/trypins") {
        doTryPins("");
    } else if (line.startsWith("/trypins ")) {
        String arg = line.substring(9);
        arg.trim();
        doTryPins(arg);
    } else if (line == "/ssp") {
        doEnableSsp();
    } else if (line == "/disconnect") {
        SerialBT.disconnect();
        linkUp = false;
        Serial.println("Disconnected");
    } else if (line == "/status") {
        Serial.printf("Link: %s\n", (linkUp && SerialBT.connected()) ? "UP" : "DOWN");
    } else if (line == "/init") {
        doInit();
    } else if (line == "/pids") {
        decodeSupportedPids(sendCommand("0100", 5000));
    } else if (line.startsWith("/mon ")) {
        doMonitor(line.substring(5));
    } else if (line.startsWith("/hex ")) {
        sendCommand(line.substring(5), ELM327_TIMEOUT_MS, true);
    } else {
        Serial.printf("Unknown command: %s (try /help)\n", line.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("ESP32 OBD-II bring-up tool");
    Serial.printf("Target: %s\n",
#if OBD_USE_MAC_ADDRESS
                  "(MAC from config.h)"
#else
                  OBD_BT_DEVICE_NAME
#endif
    );

    SerialBT.begin(OBD_LOCAL_BT_NAME, true);
    applyPin();

    printHelp();
    Serial.println("Tip: run /scan first, then /connect, then /init");
}

void loop() {
    // Unsolicited bytes from the module (some adapters chatter on
    // connect) -- surface them rather than swallowing them.
    if (linkUp && SerialBT.available()) {
        String unsolicited;
        while (SerialBT.available()) {
            unsolicited += static_cast<char>(SerialBT.read());
            delay(2);
        }
        Serial.print("<< (unsolicited) ");
        printEscaped(unsolicited);
    }

    if (linkUp && !SerialBT.connected()) {
        Serial.println("!! SPP link dropped");
        linkUp = false;
    }

    static String line;
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (line.length() > 0) {
                handleLine(line);
                line = "";
            }
        } else {
            line += c;
        }
    }
}
