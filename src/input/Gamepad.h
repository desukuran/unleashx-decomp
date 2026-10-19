// Menu-level gamepad input.
#pragma once

class CXBApplicationEx;

enum InputAction
{
	INPUT_NONE = 0,
	INPUT_START = 2,     // START button
	INPUT_BACK = 4,      // BACK button
	// TODO: the rest (0x0002B080 returns 1..14)
};

class CGamepadInput
{
public:
	int GetAction();     // 0x0002B080 TODO: decompile
};

extern CGamepadInput g_gamepadInput;                 // 0x00278280

// Merges the four controllers into the application's combined gamepad state.
void UpdateGamepads(CXBApplicationEx *app);          // 0x000E020F TODO: decompile
