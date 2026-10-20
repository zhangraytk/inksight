#include "x4_power.h"
#include "config.h"
#include "epd_driver.h"
#include "display.h"

#if defined(BOARD_PROFILE_XTEINK_X4)
#include <WiFi.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

void x4PowerInit() {
    // GPIO13 gates the battery MOSFET.
    // Release any lingering hold from prior sleep cycle:
    gpio_hold_dis(GPIO_NUM_13);
    pinMode(PIN_PWR_LATCH, OUTPUT);
    digitalWrite(PIN_PWR_LATCH, HIGH);

    // USB detect pin (GPIO20)
    pinMode(PIN_USB_DETECT, INPUT);

    // Power button (GPIO3)
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
}

bool x4IsUsbConnected() {
    return digitalRead(PIN_USB_DETECT) == HIGH;
}

static void waitForPowerButtonRelease(unsigned long maxWaitMs = 3000) {
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    unsigned long start = millis();
    while (digitalRead(PIN_CFG_BTN) == LOW && (millis() - start < maxWaitMs)) {
        delay(10);
    }
    delay(50);
}

void x4PowerDeepSleep(int minutes) {
    Serial.printf("[X4-PWR] Preparing timed deep sleep for %d min (USB=%s)\n",
                  minutes, x4IsUsbConnected() ? "YES" : "NO");
    Serial.flush();

    // 1. Show sleep badge in bottom-right corner
    showSleepBadge(minutes);

    // 2. Disconnect and turn off Wi-Fi
    if (WiFi.getMode() != WIFI_MODE_NULL) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }

    // 3. Put EPD into hibernate mode
    epdSleep();

    // 3. Wait for power button to be released to avoid immediate re-wake
    waitForPowerButtonRelease();

    // 4. Ensure GPIO13 is driven HIGH and hold is latched across deep sleep
    gpio_hold_dis(GPIO_NUM_13);
    pinMode(PIN_PWR_LATCH, OUTPUT);
    digitalWrite(PIN_PWR_LATCH, HIGH);
    gpio_hold_en(GPIO_NUM_13);
    gpio_deep_sleep_hold_en();

    // 5. Configure wake sources
    // Power button (GPIO3) wake on LOW
    esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_CFG_BTN, ESP_GPIO_WAKEUP_GPIO_LOW);

    // RTC Timer wake
    if (minutes > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)minutes * 60ULL * 1000000ULL);
    }

    // 6. Start deep sleep
    esp_deep_sleep_start();
}

void x4PowerShutdown() {
    Serial.println("[X4-PWR] Performing full shutdown (cutting battery MOSFET)...");
    Serial.flush();

    // 1. Show full shutdown screen
    showShutdownScreen();

    // 2. Disconnect and turn off Wi-Fi
    if (WiFi.getMode() != WIFI_MODE_NULL) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }

    // 3. Put EPD into hibernate
    epdSleep();

    // 3. Wait for power button release
    waitForPowerButtonRelease();

    // 4. Release hold and pull GPIO13 LOW to turn off battery MOSFET
    gpio_hold_dis(GPIO_NUM_13);
    pinMode(PIN_PWR_LATCH, OUTPUT);
    digitalWrite(PIN_PWR_LATCH, LOW);
    gpio_hold_en(GPIO_NUM_13);
    gpio_deep_sleep_hold_en();

    // 5. Enable GPIO3 wake in case USB is still connected
    esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_CFG_BTN, ESP_GPIO_WAKEUP_GPIO_LOW);

    // 6. Enter sleep (battery power is cut; if USB connected, MCU enters low power)
    esp_deep_sleep_start();
}

#endif // BOARD_PROFILE_XTEINK_X4
