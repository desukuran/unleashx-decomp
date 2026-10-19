// Clock, power, fan and LED control through the SMC (the PIC16LC on SMBus
// address 0x20).
#pragma once

#include "../common/Platform.h"

// Sets the clock; years outside 2000-2099 are replaced by a fixed default
// date. local: st is local time and gets converted to UTC.
void SetXboxSystemTime(SYSTEMTIME st, BOOL local);   // 0x0009DD2F

// Fan speed in SMC units (0-50; 10 = 20%). Values up to 10 are ignored.
void SetFanSpeed(int speed);                         // 0x0009F552
int GetFanSpeed();                                   // 0x0009E66D

void SmcPowerCycle();                                // 0x0008F9EF
void SmcPowerOff();                                  // 0x0008FA04
void SmcReset();                                     // 0x0008FA5C
void SmcSetLedAuto();                                // 0x0008FA1C
// sequence: four 2-bit steps, green in the low nibble, red in the high one.
void SmcSetLed(BYTE sequence);                       // 0x0008FA31
void SmcSetResetOnEject(BOOL reset);                 // 0x0008F947 / 0x0008F95C

// Reboots into the default dashboard (XLaunchNewImage(NULL, ...)).
HRESULT RebootToDashboard();                         // 0x0009A240
void LaunchDefaultDashboard();                       // 0x0008FA6A
