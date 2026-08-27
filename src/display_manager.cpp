#include "display_manager.h"
#include "config.h"

#include <Wire.h>

#if DISPLAY_IS_SH1106
#include <Adafruit_SH110X.h>
static Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define OLED_WHITE SH110X_WHITE
#else
#include <Adafruit_SSD1306.h>
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define OLED_WHITE SSD1306_WHITE
#endif

bool DisplayManager::begin() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

#if DISPLAY_IS_SH1106
    if (!display.begin(OLED_I2C_ADDRESS, true)) {
        return false;
    }
#else
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        return false;
    }
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
    display.setCursor(4, 10);
    display.print("ESP32 OBD-II Gauge");
    display.setCursor(4, 26);
    display.print("Bluetooth + ELM327");
    display.setCursor(20, 48);
    display.print("starting...");

    display.display();
}

void DisplayManager::showConnecting() {
    display.clearDisplay();
    display.setTextColor(OLED_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("OBD-II");
    display.setCursor(104, 0);
    display.print("--");
    display.drawFastHLine(0, 10, SCREEN_WIDTH, OLED_WHITE);

    display.setCursor(0, 28);
    display.print("Searching for");
    display.setCursor(0, 40);
    display.print("adapter...");

    display.display();
}

void DisplayManager::showDashboard(const VehicleData &d) {
    display.clearDisplay();
    display.setTextColor(OLED_WHITE);

    // Header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("OBD-II");
    display.setCursor(104, 0);
    display.print("BT");
    display.drawFastHLine(0, 10, SCREEN_WIDTH, OLED_WHITE);

    // RPM, large
    display.setTextSize(3);
    display.setCursor(0, 14);
    if (d.rpmValid) {
        display.printf("%4d", d.rpm);
    } else {
        display.print("----");
    }
    display.setTextSize(1);
    display.setCursor(98, 30);
    display.print("RPM");

    display.drawFastHLine(0, 40, SCREEN_WIDTH, OLED_WHITE);

    // Speed + coolant temp + throttle
    display.setTextSize(1);
    display.setCursor(0, 44);
    if (d.speedValid) {
        display.printf("SPD %3d km/h", d.speedKph);
    } else {
        display.print("SPD --- km/h");
    }

    display.setCursor(0, 54);
    if (d.coolantValid) {
        display.printf("TMP %3dC", d.coolantTempC);
    } else {
        display.print("TMP ---C");
    }

    display.setCursor(70, 54);
    if (d.throttleValid) {
        display.printf("THR %3d%%", d.throttlePct);
    } else {
        display.print("THR ---%");
    }

    display.display();
}

void DisplayManager::showError(const char *message) {
    display.clearDisplay();
    display.setTextColor(OLED_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("ERROR");
    display.drawFastHLine(0, 10, SCREEN_WIDTH, OLED_WHITE);
    display.setCursor(0, 20);
    display.print(message);

    display.display();
}
