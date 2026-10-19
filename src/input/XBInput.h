// The XDK sample framework's input devices (XBInput): the per-port device
// tables it fills in every frame. Library code; only the fields UnleashX
// reads are named.
#pragma once

#include "../common/Platform.h"

#ifndef XINPUT_GAMEPAD_START
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#endif

// The start of XBGAMEPAD: what a gamepad looks like to most code.
struct SGamepadState
{
	unsigned short wButtons;           // +0x00
	unsigned char bAnalogButtons[8];   // +0x02 A, B, X, Y, BLACK, WHITE, LTRIGGER, RTRIGGER
	short sThumbLX, sThumbLY;          // +0x0A
	short sThumbRX, sThumbRY;          // +0x0E
	float fX1;                         // +0x14 -1..1
	float fY1;                         // +0x18
	float fX2;                         // +0x1C
	float fY2;                         // +0x20
};

// A gamepad. 0xE0 bytes.
struct XBGAMEPAD : public SGamepadState
{
	WORD wLastButtons;                 // +0x24
	BOOL bLastAnalogButtons[8];        // +0x28
	WORD wPressedButtons;              // +0x48
	BOOL bPressedAnalogButtons[8];     // +0x4C
	int nAction;                       // +0x6C 1-14: an action to report (see CGamepadInput::GetAction). TODO: verify (who sets it)
	BYTE unknown70[0xD4 - 0x70];
	HANDLE hDevice;                    // +0xD4 NULL = no gamepad in the port
	BOOL bInserted;                    // +0xD8
	BOOL bRemoved;                     // +0xDC
};
// An IR remote (the DVD playback kit). 0x84 bytes.
struct SIRRemote
{
	BYTE unknown00[0x0A];
	WORD wKeyCode;                     // +0x0A the button held, 0 = none
	BYTE unknown0C[0x28 - 0x0C];
	HANDLE hDevice;                    // +0x28 NULL = no receiver in the port
	BYTE unknown2C[0x84 - 0x2C];
};
struct SKeyboardState
{
	HANDLE hDevice;                    // +0x00 TODO: verify
	BYTE unknown04[0x10];
};
extern XBGAMEPAD g_Gamepads[4];                   // 0x00271B30
extern SIRRemote g_irRemotes[4];                  // 0x00271ED8
extern SKeyboardState g_keyboards[4];             // 0x002720EC

// A key from the debug keyboard (XINPUT_DEBUG_KEYSTROKE).
struct SKeystroke
{
	BYTE virtualKey;                   // +0x00 VK_*
	char ascii;                        // +0x01 > 0 for a character
	BYTE flags;                        // +0x02 4 = Alt
};
// The last debug keyboard keystroke (XBInput). (0x002720E8)
SKeystroke *GetKeystroke();                       // 0x000985A4
