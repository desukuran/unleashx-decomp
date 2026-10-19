// Clock and SMC (fan) control.
#pragma once

#include "../common/Platform.h"

// Sets the clock; years outside 2000-2099 are replaced by a fixed default
// date. local: st is local time and gets converted to UTC.
void SetXboxSystemTime(SYSTEMTIME st, BOOL local);   // 0x0009DD2F TODO: decompile

void SetFanSpeed(int speed);                         // 0x0009F552 TODO: decompile
int GetFanSpeed();                                   // 0x0009E66D TODO: decompile
