// Menu-level gamepad input.
#pragma once

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

// The sample framework's XBGAMEPAD (the fields used).
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

class CGamepadInput
{
public:
	int GetAction();     // 0x0002B080 TODO: decompile
};

extern CGamepadInput g_gamepadInput;                 // 0x00278280

// Merges the four controllers into the application's combined gamepad state.
void UpdateGamepads(CXBApplicationEx *app);          // 0x000E020F TODO: decompile
