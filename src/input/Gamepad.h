// Menu-level gamepad input (Gamepad.cpp).
#pragma once

#include "../app/XBApp.h"
#include "../common/Platform.h"
#include "XBInput.h"

class CXBApplicationEx;

// What CGamepadInput::GetAction returns: one action per frame, with
// auto-repeat for the triggers and thumbsticks.
enum InputAction
{
	INPUT_NONE = 0,
	INPUT_A = 1,
	INPUT_START = 2,
	INPUT_B = 3,
	INPUT_BACK = 4,
	INPUT_X = 5,
	INPUT_Y = 6,
	INPUT_WHITE = 7,
	INPUT_BLACK = 8,
	INPUT_LTRIGGER = 9,
	INPUT_RTRIGGER = 10,
	INPUT_UP = 11,
	INPUT_DOWN = 12,
	INPUT_LEFT = 13,
	INPUT_RIGHT = 14,
	INPUT_LTHUMB = 15,         // left stick moved (see SGamepadState::fY1)
	INPUT_LTHUMB_CLICK = 16,
	INPUT_RTHUMB = 17,
	INPUT_RTHUMB_CLICK = 18,
	// DVD remote (0x00026340)
	INPUT_IR_DISPLAY = 19,
	INPUT_IR_REVERSE = 20,
	INPUT_IR_PLAY = 21,
	INPUT_IR_FORWARD = 22,
	INPUT_IR_SKIP_PREV = 23,
	INPUT_IR_STOP = 24,
	INPUT_IR_PAUSE = 25,
	INPUT_IR_SKIP_NEXT = 26,
	INPUT_IR_TITLE = 27,
	INPUT_IR_INFO = 28,
	INPUT_IR_UP = 29,
	INPUT_IR_DOWN = 30,
	INPUT_IR_LEFT = 31,
	INPUT_IR_RIGHT = 32,
	INPUT_IR_SELECT = 33,
	INPUT_IR_MENU = 34,
	INPUT_IR_BACK = 35,
	INPUT_IR_1 = 36,           // ... INPUT_IR_9 = 44
	INPUT_IR_0 = 45,
	INPUT_POINTER_CLICK = 47,  // TODO: verify (selects the item under the pointer)
};

// Turns the gamepads and the IR remote into one InputAction per call: the
// first port with something pressed wins (and is remembered), and held
// buttons repeat after a delay. Code that takes "SGamepadState **pad" is
// given this object: its first member is the gamepad in use. 0xA4 bytes.
class CGamepadInput
{
public:
	CGamepadInput();                                  // 0x00021F02 (this in edi)
	int GetAction();                                  // 0x0002B080 (this in eax)
	// The IR remote's action, or 0. (this in edi)
	int GetRemoteAction();                            // 0x00026340

	XBGAMEPAD *m_pGamepad;            // +0x00 the one in use
	XBGAMEPAD *m_pFixedGamepad;       // +0x04 if set, always this one (Initialize: the merged pad)
	SIRRemote *m_pRemote;             // +0x08
	SIRRemote *m_pFixedRemote;        // +0x0C (Initialize: CUnleashX::m_defaultIR)
	DWORD m_unknown10;                // +0x10
	DWORD m_unknown14;                // +0x14
	CXBStopWatch m_padTimer;          // +0x18 how long a gamepad button has been held
	float m_fPadDelay;                // +0x38 until the next repeat
	float m_fPadRepeats;              // +0x3C
	CXBStopWatch m_remoteTimer;       // +0x40
	float m_fRemoteDelay;             // +0x60
	DWORD m_unknown64;                // +0x64
	CXBStopWatch m_unknownTimer;      // +0x68
	DWORD m_unknown88;                // +0x88
	float m_fDelays[6];               // +0x8C copies of the delays used (not read)

private:
	// The gamepad (remote) to read: the remembered port if it's still
	// there, else the first one with input, else the first connected.
	XBGAMEPAD *GetActiveGamepad();                    // 0x0002AF08
	SIRRemote *GetActiveRemote();                     // 0x00026248
	// Something is pressed on pad, or on the remote (whose arrows, SELECT
	// and BACK are copied into the gamepad). (pad in eax, this in ecx)
	BOOL HasInput(XBGAMEPAD *pad);                    // 0x0002AFAC
	// (remote in eax, this in edx)
	BOOL MapRemoteToGamepad(SIRRemote *remote);       // 0x000262E6
};

extern CGamepadInput g_gamepadInput;                 // 0x00278280

// Reads the four controllers and merges them into the application's
// combined gamepad and remote state. (app in esi)
void UpdateGamepads(CXBApplicationEx *app);          // 0x000E020F
