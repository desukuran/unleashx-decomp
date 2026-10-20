// The XDK sample framework's input devices (XBInput): the per-port device
// tables it fills in every frame. Library code; only the fields UnleashX
// reads are named.
#pragma once

#include "../common/Platform.h"

#ifndef XINPUT_GAMEPAD_START
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
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
	// A cooked event (the samples' XBGAMEPAD_EVENT): 1 START, 2 BACK, 3-8 A B
	// X Y BLACK WHITE, 9-12 the d-pad, 13-14 the triggers. Reported as
	// actions by CGamepadInput::GetAction.
	int nAction;                       // +0x6C
	BYTE feedback[0xBA - 0x70];        // +0x70 Rumble and Feedback (unused)
	BYTE caps[0xD4 - 0xBA];            // +0xBA XINPUT_CAPABILITIES
	HANDLE hDevice;                    // +0xD4 NULL = no gamepad in the port
	BOOL bInserted;                    // +0xD8
	BOOL bRemoved;                     // +0xDC
};
// An IR remote (the DVD playback kit). 0x84 bytes. Every 0.14 s the first
// receiver found is read; a key repeat comes faster (0.1 s) than a new key.
struct SIRRemote
{
	BYTE unknown00;                    // +0x00 the reported key (copied into the merged state)
	BYTE unknown01;
	BYTE bKeyFlags;                    // +0x02 TODO: verify
	BYTE unknown03;
	WORD wPreviousKey;                 // +0x04 the key reported before last
	WORD wLastKey;                     // +0x06 the last key reported
	WORD unknown08;                    // +0x08 the key held (copied into the merged state)
	WORD wKeyCode;                     // +0x0A the button held, 0 = none
	BYTE unknown0C[0x28 - 0x0C];
	HANDLE hDevice;                    // +0x28 NULL = no receiver in the port
	BYTE unknown2C[0x78 - 0x2C];
	BOOL bInserted;                    // +0x78
	BOOL bRemoved;                     // +0x7C
	DWORD dwPort;                      // +0x80 the merged state: which port it came from
};

// A debug keyboard port. 0x14 bytes.
struct SKeyboardState
{
	HANDLE hDevice;                    // +0x00
	BOOL bInserted;                    // +0x04
	BOOL bRemoved;                     // +0x08
	DWORD dwPort;                      // +0x0C
	DWORD unknown10;
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

// The XBInput functions CXBApplicationEx::Create/Run and UpdateGamepads call
// (library code, not reconstructed). NULL means the global table.
HRESULT XBInput_CreateGamepads(XBGAMEPAD **gamepads);            // 0x00098A46
void XBInput_RefreshDeviceList(XBGAMEPAD *gamepads);              // 0x00098943 (port changes)
void XBInput_GetInput(XBGAMEPAD *gamepads);                       // 0x00098B11
HRESULT XBInput_CreateIR_Remotes(SIRRemote **remotes);            // 0x0009888E
void XBInput_GetIR_RemoteInput(SIRRemote *remotes, float time);   // 0x000986B9
HRESULT XBInput_InitDebugKeyboard(SKeyboardState **keyboards);    // 0x000985AA (the samples' DebugKeyboard)
HRESULT XBInput_GetKeyboardInput(SKeystroke *keystroke);          // 0x00098478

// A USB mouse on a controller port (the debug mouse). 0x30 bytes.
struct SMouseState
{
	DWORD dwPacketNumber;              // +0x00 XINPUT_STATE
	BYTE bButtons;                     // +0x04 1 left, 2 right, 4 middle
	signed char cMickeysX;             // +0x05
	signed char cMickeysY;             // +0x06
	signed char cWheel;                // +0x07
	BYTE unknown08[0x10];
	HANDLE hDevice;                    // +0x18
	BOOL bInserted;                    // +0x1C
	BOOL bRemoved;                     // +0x20
	DWORD dwPort;                      // +0x24
	BYTE bClickButtons;                // +0x28 the buttons again within 180 ms (a double click), else 0
	BYTE unknown29[3];
	DWORD dwLastTime;                  // +0x2C
};
extern SMouseState g_mice[4];                     // 0x00272170
// Reads the mice; the ports with new data as bits. The XDK's DebugMouse
// sample (Input/DebugMouse/mouse.cpp) extended with the connected flag,
// insert/remove tracking and the 180 ms double click. (Library code.)
// Opens the mice plugged in at startup. (The debug mouse sample.)
HRESULT XBInput_InitDebugMouse(SMouseState **mice);   // 0x0009748A
DWORD XBInput_GetMouseInput();                    // 0x000972F6
// Whether a mouse is plugged in (as of the last read).
BOOL XBInput_IsMouseConnected();                  // 0x000972F0
