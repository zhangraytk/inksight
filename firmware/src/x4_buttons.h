#ifndef INKSIGHT_X4_BUTTONS_H
#define INKSIGHT_X4_BUTTONS_H

#include <Arduino.h>

#if defined(BOARD_PROFILE_XTEINK_X4)

enum class X4ButtonId : uint8_t {
    NONE = 0,
    BACK,
    CONFIRM,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    POWER
};

enum class X4ButtonAction : uint8_t {
    NONE = 0,
    NEXT_MODE,        // Right button click -> next mode
    PREV_MODE,        // Left button click -> previous mode
    REFRESH,          // Confirm button click -> manual refresh
    ENTER_PORTAL,     // Back button long press (> 2000ms) -> portal
    POWER_SLEEP,      // Power button click -> timed deep sleep
    POWER_SHUTDOWN,   // Power button long press (> 3000ms) -> power off
    UP_CLICK,         // Up button click
    DOWN_CLICK,       // Down button click
    BACK_CLICK        // Back button click
};

void x4ButtonsInit();
void x4ButtonsUpdate();
X4ButtonAction x4ButtonsGetAction();
bool x4ButtonsCheckAbort();
bool x4ButtonsIsAnyPressed();

#endif // BOARD_PROFILE_XTEINK_X4
#endif // INKSIGHT_X4_BUTTONS_H
