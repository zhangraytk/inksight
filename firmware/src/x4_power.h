#ifndef INKSIGHT_X4_POWER_H
#define INKSIGHT_X4_POWER_H

#include <Arduino.h>

#if defined(BOARD_PROFILE_XTEINK_X4)

// Initialize X4 power latch on GPIO13 first thing during boot
void x4PowerInit();

// Check if device is powered by external USB (GPIO20)
bool x4IsUsbConnected();

// Enter Timed Deep Sleep (Mode 1): keeps GPIO13 latched HIGH, enables RTC timer & GPIO3 wake
void x4PowerDeepSleep(int minutes);

// Enter Complete Power Off / Shutdown (Mode 2): releases latch and drives GPIO13 LOW
void x4PowerShutdown();

#endif // BOARD_PROFILE_XTEINK_X4
#endif // INKSIGHT_X4_POWER_H
