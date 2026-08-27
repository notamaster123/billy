#include "display_manager.h"

#include "config.h"

#if DISPLAY_USE_SPI
#include <SPI.h>
#else
#include <Wire.h>
#endif

#if DISPLAY_IS_SH1106
#include <Adafruit_SH110X.h>
#define OLED_WHITE SH110X_WHITE
#define OLED_BLACK SH110X_BLACK
#if !DISPLAY_USE_SPI
static Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#elif DISPLAY_SPI_HARDWARE
static Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC_PIN, OLED_RES_PIN,
                                OLED_CS_PIN);
#else
static Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI_PIN, OLED_CLK_PIN,
                                OLED_DC_PIN, OLED_RES_PIN, OLED_CS_PIN);
#endif

#else  // SSD1306
#include <Adafruit_SSD1306.h>
#define OLED_WHITE SSD1306_WHITE
#define OLED_BLACK SSD1306_BLACK
#if !DISPLAY_USE_SPI
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#elif DISPLAY_SPI_HARDWARE
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC_PIN, OLED_RES_PIN,
                                OLED_CS_PIN);
#else
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI_PIN, OLED_CLK_PIN,
                                OLED_DC_PIN, OLED_RES_PIN, OLED_CS_PIN);
#endif
#endif

namespace {

// Header occupies the top 11px, the page dots the bottom 7px; screens
// draw between them.
constexpr int16_t BODY_TOP = 13;

const char *SCREEN_TITLES[SCREEN_COUNT] = {
    "DASH", "TEMPS", "TRIMS", "CODES", "STATUS",
};

// Right-aligns text at `rightEdge` for the current text size, so
// changing digit counts don't make numbers jump around.
void printRightAligned(const char *text, int16_t rightEdge, int16_t y, uint8_t size) {
    int16_t width = static_cast<int16_t>(strlen(text)) * 6 * size;
    display.setCursor(rightEdge - width, y);
    display.print(text);
}

void formatMetric(const VehicleData &data, Metric m, char *out, size_t len) {
    const MetricInfo &info = METRICS[static_cast<uint8_t>(m)];
    if (!data.has(m)) {
        snprintf(out, len, "--");
        return;
    }
    float v = data.get(m);
    if (info.decimals == 0) {
        snprintf(out, len, "%s%d", (info.bipolar && v > 0) ? "+" : "", static_cast<int>(lroundf(v)));
    } else {
        snprintf(out, len, "%s%.1f", (info.bipolar && v > 0) ? "+" : "", v);
    }
}

}  // namespace

bool DisplayManager::begin() {
#if !DISPLAY_USE_SPI
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
#endif

#if DISPLAY_IS_SH1106
    // On SPI the address argument is ignored by the driver.
    if (!display.begin(DISPLAY_USE_SPI ? 0 : OLED_I2C_ADDRESS, true)) {
        return false;
    }
#else
#if DISPLAY_USE_SPI
    if (!display.begin(SSD1306_SWITCHCAPVCC)) {
        return false;
    }
#else
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        return false;
    }
#endif
#endif

    display.setTextWrap(false);
    display.clearDisplay();
    display.display();
    return true;
}

void DisplayManager::showSplash() {
    display.clearDisplay();
    display.setTextColor(OLED_WHITE);

    display.setTextSize(1);
    display.setCursor(10, 14);
    display.print("ESP32 OBD-II");
    display.setTextSize(2);
    display.setCursor(22, 28);
    display.print("GAUGES");
    display.setTextSize(1);
    display.setCursor(28, 50);
    display.print("starting...");

    display.display();
}

// Title bar plus page dots, drawn on every screen so the knob's
// position in the carousel is always visible.
void DisplayManager::drawChrome(Screen screen, const SystemStatus &status) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(SCREEN_TITLES[static_cast<uint8_t>(screen)]);

    const char *badge = status.simulating ? "SIM" : (status.linkUp ? "LIVE" : "----");
    printRightAligned(badge, SCREEN_WIDTH, 0, 1);

    display.drawFastHLine(0, 10, SCREEN_WIDTH, OLED_WHITE);

    // Page dots
    const int16_t spacing = 9;
    const int16_t totalWidth = spacing * (SCREEN_COUNT - 1);
    const int16_t startX = (SCREEN_WIDTH - totalWidth) / 2;
    for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
        int16_t x = startX + i * spacing;
        if (i == static_cast<uint8_t>(screen)) {
            display.fillCircle(x, SCREEN_HEIGHT - 3, 2, OLED_WHITE);
        } else {
            display.drawPixel(x, SCREEN_HEIGHT - 3, OLED_WHITE);
        }
    }
}

void DisplayManager::drawDash(const VehicleData &data) {
    char buf[12];

    // RPM gets the most pixels: it is the value you read at a glance.
    formatMetric(data, Metric::RPM, buf, sizeof(buf));
    display.setTextSize(3);
    printRightAligned(buf, 98, BODY_TOP + 2, 3);
    display.setTextSize(1);
    display.setCursor(102, BODY_TOP + 12);
    display.print("rpm");

    formatMetric(data, Metric::SPEED, buf, sizeof(buf));
    display.setTextSize(2);
    display.setCursor(0, BODY_TOP + 26);
    display.print(buf);
    display.setTextSize(1);
    display.print(" mph");

    formatMetric(data, Metric::THROTTLE, buf, sizeof(buf));
    display.setTextSize(1);
    char line[20];
    snprintf(line, sizeof(line), "THR %s%%", buf);
    printRightAligned(line, SCREEN_WIDTH, BODY_TOP + 32, 1);
}

void DisplayManager::drawTemps(const VehicleData &data) {
    char buf[12];

    display.setTextSize(1);
    display.setCursor(0, BODY_TOP);
    display.print("COOLANT");
    formatMetric(data, Metric::COOLANT, buf, sizeof(buf));
    display.setTextSize(3);
    display.setCursor(0, BODY_TOP + 10);
    display.print(buf);
    display.setTextSize(1);
    display.print("F");

    display.setCursor(0, BODY_TOP + 36);
    formatMetric(data, Metric::INTAKE, buf, sizeof(buf));
    display.printf("INTAKE AIR  %sF", buf);
}

void DisplayManager::drawTrims(const VehicleData &data) {
    const Metric order[4] = {Metric::STFT1, Metric::LTFT1, Metric::STFT2, Metric::LTFT2};
    char buf[12];

    display.setTextSize(1);
    for (uint8_t i = 0; i < 4; i++) {
        int16_t y = BODY_TOP + i * 11;
        const MetricInfo &info = METRICS[static_cast<uint8_t>(order[i])];

        display.setCursor(0, y);
        display.print(info.label);

        formatMetric(data, order[i], buf, sizeof(buf));
        char line[12];
        snprintf(line, sizeof(line), "%s%%", buf);
        printRightAligned(line, 62, y, 1);

        // A short bar growing from a centre tick: zero means the ECU
        // is not correcting, which a left-anchored bar would hide.
        const int16_t barLeft = 68, barWidth = 60, mid = barLeft + barWidth / 2;
        display.drawFastVLine(mid, y - 1, 9, OLED_WHITE);
        if (data.has(order[i])) {
            float v = constrain(data.get(order[i]), -25.0f, 25.0f);
            int16_t half = static_cast<int16_t>(fabsf(v) / 25.0f * (barWidth / 2));
            if (half > 0) {
                display.fillRect(v >= 0 ? mid : mid - half, y + 1, half, 5, OLED_WHITE);
            }
        }
    }
}

void DisplayManager::drawCodes(const VehicleData &data) {
    display.setTextSize(1);

    if (!data.dtcsRead) {
        display.setCursor(0, BODY_TOP + 8);
        display.print("Press to read codes");
        return;
    }

    display.setCursor(0, BODY_TOP);
    display.print(data.milOn ? "MIL ON" : "MIL off");
    char count[16];
    snprintf(count, sizeof(count), "%u stored", data.storedCount);
    printRightAligned(count, SCREEN_WIDTH, BODY_TOP, 1);

    if (data.dtcCount == 0) {
        display.setCursor(0, BODY_TOP + 16);
        display.print("No trouble codes");
        return;
    }

    // Two columns; six codes fit before the page dots.
    for (uint8_t i = 0; i < data.dtcCount && i < 6; i++) {
        display.setCursor((i % 2) ? 66 : 0, BODY_TOP + 14 + (i / 2) * 10);
        display.print(data.dtc[i]);
    }
    if (data.dtcCount > 6) {
        printRightAligned("...", SCREEN_WIDTH, BODY_TOP + 34, 1);
    }
}

void DisplayManager::drawStatus(const SystemStatus &status) {
    display.setTextSize(1);

    // Five rows at 9px pitch is the most that fits between the header
    // rule and the page dots.
    display.setCursor(0, BODY_TOP);
    display.printf("Mode  %s", status.simulating ? "SIMULATION" : "LIVE");

    display.setCursor(0, BODY_TOP + 9);
    display.printf("Link  %s", status.linkText);

    display.setCursor(0, BODY_TOP + 18);
    display.print(status.adapterName);

    display.setCursor(0, BODY_TOP + 27);
    display.print(status.adapterMac);

    display.setCursor(0, BODY_TOP + 36);
    if (status.simulating) {
        display.print("hold: go live");
    } else {
        display.print(status.linkUp ? "press: disconnect" : "press: connect");
    }
}

void DisplayManager::render(Screen screen, const VehicleData &data, const SystemStatus &status) {
    display.clearDisplay();
    display.setTextColor(OLED_WHITE);

    drawChrome(screen, status);

    switch (screen) {
        case Screen::DASH:   drawDash(data);      break;
        case Screen::TEMPS:  drawTemps(data);     break;
        case Screen::TRIMS:  drawTrims(data);     break;
        case Screen::CODES:  drawCodes(data);     break;
        case Screen::STATUS: drawStatus(status);  break;
        default: break;
    }

    display.display();
}
