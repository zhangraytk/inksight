#include "x4_buttons.h"
#include "config.h"

#if defined(BOARD_PROFILE_XTEINK_X4)

namespace {
    // Button Bit Index definitions (matching Xtelink-screen InputManager)
    static constexpr uint8_t BTN_BIT_BACK = 0;
    static constexpr uint8_t BTN_BIT_CONFIRM = 1;
    static constexpr uint8_t BTN_BIT_LEFT = 2;
    static constexpr uint8_t BTN_BIT_RIGHT = 3;
    static constexpr uint8_t BTN_BIT_UP = 4;
    static constexpr uint8_t BTN_BIT_DOWN = 5;
    static constexpr uint8_t BTN_BIT_POWER = 6;
    static constexpr uint8_t NUM_TOTAL_BUTTONS = 7;

    // ADC ladder range thresholds (from Xtelink-screen / FreeInk field telemetry)
    // Group 1 (GPIO1):
    //   Back: 3100 ~ 3800 (avg 3512, range 3470-3597)
    //   Confirm: 2090 ~ 3100 (avg 2694, range 2655-2760)
    //   Left: 750 ~ 2090 (avg 1493, range 1470-1530)
    //   Right: <= 750 (avg ~5, range 3-6)
    //   No button: > 3800 (idle pulled up to 3.3V, reads ~4000-4095)
    // Group 2 (GPIO2):
    //   Up: 1120 ~ 3200 (avg 2242, range 2205-2300)
    //   Down: <= 1120 (avg ~5, range 3-6)
    //   No button: > 3200 (idle pulled up to 3.3V, reads ~4000-4095)

    static constexpr int ADC_NO_BTN_1 = 3800;
    static const int ADC_RANGES_1[] = {ADC_NO_BTN_1, 3100, 2090, 750, INT32_MIN};
    static constexpr int NUM_BTNS_GROUP1 = 4; // Back, Confirm, Left, Right

    static constexpr int ADC_NO_BTN_2 = 3200;
    static const int ADC_RANGES_2[] = {ADC_NO_BTN_2, 1120, INT32_MIN};
    static constexpr int NUM_BTNS_GROUP2 = 2; // Up, Down

    int getButtonFromADC(int adcValue, const int ranges[], int count) {
        for (int i = 0; i < count; i++) {
            if (ranges[i + 1] < adcValue && adcValue <= ranges[i]) {
                return i;
            }
        }
        return -1;
    }

    uint8_t readRawState() {
        uint8_t state = 0;

        // 1. Digital Power Key (GPIO3, active LOW with pullup)
        if (digitalRead(PIN_CFG_BTN) == LOW) {
            state |= (1 << BTN_BIT_POWER);
        }

        // 2. ADC Group 1 (GPIO1) - independent check
        int a1 = analogRead(PIN_BTN_ADC1);
        int b1 = getButtonFromADC(a1, ADC_RANGES_1, NUM_BTNS_GROUP1);
        if (b1 >= 0) {
            state |= (1 << b1); // 0=Back, 1=Confirm, 2=Left, 3=Right
        }

        // 3. ADC Group 2 (GPIO2) - independent check
        int a2 = analogRead(PIN_BTN_ADC2);
        int b2 = getButtonFromADC(a2, ADC_RANGES_2, NUM_BTNS_GROUP2);
        if (b2 >= 0) {
            state |= (1 << (b2 + 4)); // 4=Up, 5=Down
        }

        return state;
    }

    // Action Queue to prevent dropping events during refresh or burst presses
    static constexpr int QUEUE_SIZE = 8;
    X4ButtonAction actionQueue[QUEUE_SIZE];
    uint8_t queueHead = 0;
    uint8_t queueTail = 0;

    void enqueueAction(X4ButtonAction act) {
        uint8_t next = (queueHead + 1) % QUEUE_SIZE;
        if (next != queueTail) {
            actionQueue[queueHead] = act;
            queueHead = next;
        }
    }

    // State Tracking
    uint8_t currentState = 0;
    uint8_t lastSampleState = 0;
    unsigned long lastDebounceTime = 0;
    unsigned long pressStartTime[NUM_TOTAL_BUTTONS] = {0};
    bool longPressFired[NUM_TOTAL_BUTTONS] = {false};
} // namespace

void x4ButtonsInit() {
    pinMode(PIN_BTN_ADC1, INPUT);
    pinMode(PIN_BTN_ADC2, INPUT);
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    analogSetAttenuation(ADC_11db);
    currentState = 0;
    lastSampleState = 0;
    lastDebounceTime = millis();
    queueHead = 0;
    queueTail = 0;
}

void x4ButtonsUpdate() {
    unsigned long now = millis();
    uint8_t sample = readRawState();

    if (sample != lastSampleState) {
        lastSampleState = sample;
        lastDebounceTime = now;
    }

    // 10ms debounce filter
    if ((now - lastDebounceTime) >= 10) {
        if (sample != currentState) {
            uint8_t pressed = sample & ~currentState;
            uint8_t released = currentState & ~sample;

            // Handle button press edges
            for (uint8_t i = 0; i < NUM_TOTAL_BUTTONS; i++) {
                if (pressed & (1 << i)) {
                    pressStartTime[i] = now;
                    longPressFired[i] = false;
                }
            }

            // Handle button release edges
            for (uint8_t i = 0; i < NUM_TOTAL_BUTTONS; i++) {
                if (released & (1 << i)) {
                    unsigned long duration = now - pressStartTime[i];
                    if (!longPressFired[i] && duration >= 30 && duration < 2000) {
                        switch (i) {
                            case BTN_BIT_RIGHT:
                                enqueueAction(X4ButtonAction::NEXT_MODE);
                                Serial.println("[X4-BTN] Right click -> NEXT_MODE");
                                break;
                            case BTN_BIT_LEFT:
                                enqueueAction(X4ButtonAction::PREV_MODE);
                                Serial.println("[X4-BTN] Left click -> PREV_MODE");
                                break;
                            case BTN_BIT_CONFIRM:
                                enqueueAction(X4ButtonAction::REFRESH);
                                Serial.println("[X4-BTN] Confirm click -> REFRESH");
                                break;
                            case BTN_BIT_BACK:
                                enqueueAction(X4ButtonAction::BACK_CLICK);
                                Serial.println("[X4-BTN] Back click -> BACK_CLICK");
                                break;
                            case BTN_BIT_UP:
                                enqueueAction(X4ButtonAction::UP_CLICK);
                                Serial.println("[X4-BTN] Up click -> UP_CLICK");
                                break;
                            case BTN_BIT_DOWN:
                                enqueueAction(X4ButtonAction::DOWN_CLICK);
                                Serial.println("[X4-BTN] Down click -> DOWN_CLICK");
                                break;
                            case BTN_BIT_POWER:
                                enqueueAction(X4ButtonAction::POWER_SLEEP);
                                Serial.println("[X4-BTN] Power click -> POWER_SLEEP");
                                break;
                        }
                    }
                }
            }

            currentState = sample;
        }
    }

    // Continuous hold checks for currently pressed buttons
    for (uint8_t i = 0; i < NUM_TOTAL_BUTTONS; i++) {
        if ((currentState & (1 << i)) && !longPressFired[i]) {
            unsigned long held = now - pressStartTime[i];
            if (i == BTN_BIT_BACK && held >= 2000) {
                longPressFired[i] = true;
                enqueueAction(X4ButtonAction::ENTER_PORTAL);
                Serial.println("[X4-BTN] Back long press -> ENTER_PORTAL");
            } else if (i == BTN_BIT_POWER && held >= 3000) {
                longPressFired[i] = true;
                enqueueAction(X4ButtonAction::POWER_SHUTDOWN);
                Serial.println("[X4-BTN] Power long press -> POWER_SHUTDOWN");
            }
        }
    }
}

X4ButtonAction x4ButtonsGetAction() {
    if (queueHead == queueTail) {
        return X4ButtonAction::NONE;
    }
    X4ButtonAction act = actionQueue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    return act;
}

bool x4ButtonsCheckAbort() {
    // Only abort if Power or Back is genuinely held
    return (currentState & ((1 << BTN_BIT_POWER) | (1 << BTN_BIT_BACK))) != 0;
}

bool x4ButtonsIsAnyPressed() {
    return currentState != 0;
}

#endif // BOARD_PROFILE_XTEINK_X4
