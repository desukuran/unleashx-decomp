// The XDK sample framework's input (XBInput) as UnleashX links it: gamepads,
// the DVD kit's IR remote, the debug keyboard and the debug mouse. Xbox
// only. See input/XBInput.h.
#include "../input/XBInput.h"

// xkbd.h has no extern "C" of its own.
#define DEBUG_KEYBOARD
extern "C" {
#include <xkbd.h>
}

// The IR remote's device type isn't in the XDK 5933 headers.
extern "C" XPP_DEVICE_TYPE XDEVICE_TYPE_IR_REMOTE_TABLE;
#define XDEVICE_TYPE_IR_REMOTE (&XDEVICE_TYPE_IR_REMOTE_TABLE)

XBGAMEPAD g_Gamepads[4];                   // 0x00271B30
SIRRemote g_irRemotes[4];                  // 0x00271ED8
SKeyboardState g_keyboards[4];             // 0x002720EC
SMouseState g_mice[4];                     // 0x00272170

static XINPUT_STATE s_gamepadStates[4];    // 0x00271AD8 (0x16 bytes each)
// Auto-poll, every 8 ms both ways.
static XINPUT_POLLING_PARAMETERS s_gamepadPolling = {TRUE, TRUE, 0, 8, 8, 0};   // 0x0026BC70

// The thumbsticks: -1..1 with a 0.24 dead zone.
static float ThumbToFloat(short value)
{
	float f = ((float)value + 0.5f) * (1.0f / 32767.5f);
	float sign = f < 0.0f ? -1.0f : 1.0f;
	float scaled = (fabsf(f) - 0.24f) * (1.0f / 0.76f);
	if (scaled < 0.0f)
		scaled = 0.0f;
	return scaled * sign;
}

// The trigger threshold for the analog buttons.
#define ANALOG_PRESSED(value) ((value) > 30)

static void OpenGamepad(XBGAMEPAD *pad, DWORD port, XINPUT_STATE *state)
{
	pad->hDevice = XInputOpen(XDEVICE_TYPE_GAMEPAD, port, XDEVICE_NO_SLOT, &s_gamepadPolling);
	if (pad->hDevice == NULL)
		return;
	XInputGetCapabilities(pad->hDevice, (XINPUT_CAPABILITIES *)pad->caps);
	XInputGetState(pad->hDevice, state);
	pad->wLastButtons = state->Gamepad.wButtons;
	for (int i = 0; i < 8; i++)
		pad->bLastAnalogButtons[i] = ANALOG_PRESSED(state->Gamepad.bAnalogButtons[i]);
}

// 0x00098A46
HRESULT XBInput_CreateGamepads(XBGAMEPAD **gamepads)
{
	DWORD devices = XGetDevices(XDEVICE_TYPE_GAMEPAD);
	for (DWORD port = 0; port < 4; port++)
	{
		memset(&s_gamepadStates[port], 0, sizeof(s_gamepadStates[port]));
		memset(&g_Gamepads[port], 0, sizeof(g_Gamepads[port]));
		if (devices & (1 << port))
			OpenGamepad(&g_Gamepads[port], port, &s_gamepadStates[port]);
	}
	if (gamepads != NULL)
		*gamepads = g_Gamepads;
	return S_OK;
}

// 0x00098943
void XBInput_RefreshDeviceList(XBGAMEPAD *gamepads)
{
	if (gamepads == NULL)
		gamepads = g_Gamepads;
	DWORD insertions, removals;
	XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals);
	for (DWORD port = 0; port < 4; port++)
	{
		XBGAMEPAD *pad = &gamepads[port];
		pad->bRemoved = (removals & (1 << port)) != 0;
		if (pad->bRemoved)
		{
			if (pad->hDevice != NULL)
				XInputClose(pad->hDevice);
			pad->hDevice = NULL;
			pad->sThumbLX = 0;
			pad->sThumbLY = 0;
		}
		pad->bInserted = (insertions & (1 << port)) != 0;
		if (pad->bInserted)
		{
			// Quirk: the state is read through the global table's handle.
			pad->hDevice = XInputOpen(XDEVICE_TYPE_GAMEPAD, port, XDEVICE_NO_SLOT, &s_gamepadPolling);
			if (pad->hDevice != NULL)
			{
				XInputGetCapabilities(pad->hDevice, (XINPUT_CAPABILITIES *)pad->caps);
				XInputGetState(g_Gamepads[port].hDevice, &s_gamepadStates[port]);
				g_Gamepads[port].wLastButtons = s_gamepadStates[port].Gamepad.wButtons;
				for (int i = 0; i < 8; i++)
					g_Gamepads[port].bLastAnalogButtons[i] = ANALOG_PRESSED(s_gamepadStates[port].Gamepad.bAnalogButtons[i]);
			}
		}
	}
}

// 0x00098B11
void XBInput_GetInput(XBGAMEPAD *gamepads)
{
	if (gamepads == NULL)
		gamepads = g_Gamepads;
	XBInput_RefreshDeviceList(gamepads);
	for (DWORD port = 0; port < 4; port++)
	{
		XBGAMEPAD *pad = &gamepads[port];
		if (pad->hDevice == NULL)
			continue;
		XInputGetState(pad->hDevice, &s_gamepadStates[port]);
		memcpy(pad, &s_gamepadStates[port].Gamepad, sizeof(XINPUT_GAMEPAD));
		pad->fX1 = ThumbToFloat(pad->sThumbLX);
		pad->fY1 = ThumbToFloat(pad->sThumbLY);
		pad->fX2 = ThumbToFloat(pad->sThumbRX);
		pad->fY2 = ThumbToFloat(pad->sThumbRY);

		WORD last = pad->wLastButtons;
		pad->wLastButtons = pad->wButtons;
		pad->wPressedButtons = (last ^ pad->wButtons) & pad->wButtons;
		BOOL anyPressed = pad->wPressedButtons != 0;
		for (int i = 0; i < 8; i++)
		{
			BOOL down = ANALOG_PRESSED(pad->bAnalogButtons[i]);
			pad->bPressedAnalogButtons[i] = down ? !pad->bLastAnalogButtons[i] : FALSE;
			// Quirk: a held analog button counts as activity too.
			anyPressed |= down;
			pad->bLastAnalogButtons[i] = down;
		}

		// The cooked event: the first new press in this order.
		pad->nAction = 0;
		if (!anyPressed)
			continue;
		WORD pressed = pad->wPressedButtons;
		if (pressed & XINPUT_GAMEPAD_START)
			pad->nAction = 1;
		else if (pressed & XINPUT_GAMEPAD_BACK)
			pad->nAction = 2;
		else if (pad->bPressedAnalogButtons[0])
			pad->nAction = 3;
		else if (pad->bPressedAnalogButtons[1])
			pad->nAction = 4;
		else if (pad->bPressedAnalogButtons[2])
			pad->nAction = 5;
		else if (pad->bPressedAnalogButtons[3])
			pad->nAction = 6;
		else if (pad->bPressedAnalogButtons[4])
			pad->nAction = 7;
		else if (pad->bPressedAnalogButtons[5])
			pad->nAction = 8;
		else if (pressed & XINPUT_GAMEPAD_DPAD_UP)
			pad->nAction = 9;
		else if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
			pad->nAction = 10;
		else if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
			pad->nAction = 11;
		else if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT)
			pad->nAction = 12;
		else if (pad->bPressedAnalogButtons[6])
			pad->nAction = 13;
		else if (pad->bPressedAnalogButtons[7])
			pad->nAction = 14;
	}
}

// ---- the IR remote ----

static BOOL s_bIRCreated;                  // 0x002776A4
static float s_fIRDelay;                   // 0x00271EB0 0.14 s, 0.1 s while a key repeats
static float s_fIRLastTime;                // 0x00271ED4
static XINPUT_STATE s_irStates[4];         // 0x00271EB4 (8 bytes used each)
static DWORD s_irLastPacket[4];            // 0x0027767C
static DWORD s_dwIRLastTick;               // 0x00277678 (written, never read)

// 0x0009888E
HRESULT XBInput_CreateIR_Remotes(SIRRemote **remotes)
{
	if (s_bIRCreated)
		return S_OK;
	DWORD devices = XGetDevices(XDEVICE_TYPE_IR_REMOTE);
	for (DWORD port = 0; port < 4; port++)
	{
		memset(&s_irStates[port], 0, 8);
		memset(&g_irRemotes[port], 0, sizeof(g_irRemotes[port]));
		if (devices & (1 << port))
		{
			XINPUT_POLLING_PARAMETERS polling = {TRUE, TRUE, 0, 0xFF, 8, 0};
			g_irRemotes[port].hDevice = XInputOpen(XDEVICE_TYPE_IR_REMOTE, port, XDEVICE_NO_SLOT, &polling);
			g_irRemotes[port].dwPort = port;
		}
	}
	s_bIRCreated = TRUE;
	if (remotes != NULL)
		*remotes = g_irRemotes;
	s_fIRDelay = 0.14f;
	return S_OK;
}

// 0x000986B9: time is the app time in seconds. Only the first receiver
// found is read.
void XBInput_GetIR_RemoteInput(SIRRemote *remotes, float time)
{
	if (remotes == NULL)
		remotes = g_irRemotes;
	for (int i = 0; i < 4; i++)
	{
		if (remotes[i].hDevice != NULL)
		{
			remotes[i].wKeyCode = 0;
			break;
		}
	}
	if (time < s_fIRLastTime + 0.14)
		return;
	s_fIRLastTime = time;

	XINPUT_POLLING_PARAMETERS polling = {TRUE, TRUE, 0, 0xFF, 8, 0};
	DWORD insertions, removals;
	XGetDeviceChanges(XDEVICE_TYPE_IR_REMOTE, &insertions, &removals);
	for (DWORD port = 0; port < 4; port++)
	{
		SIRRemote *remote = &remotes[port];
		remote->bRemoved = (removals & (1 << port)) != 0;
		if (remote->bRemoved)
		{
			if (remote->hDevice != NULL)
				XInputClose(remote->hDevice);
			remote->hDevice = NULL;
		}
		remote->bInserted = (insertions & (1 << port)) != 0;
		if (remote->bInserted)
			remote->hDevice = XInputOpen(XDEVICE_TYPE_IR_REMOTE, port, XDEVICE_NO_SLOT, &polling);
		if (remote->hDevice == NULL)
			continue;

		if (XInputGetState(remote->hDevice, &s_irStates[port]) == ERROR_SUCCESS)
		{
			if (s_irLastPacket[port] != s_irStates[port].dwPacketNumber)
			{
				s_irLastPacket[port] = s_irStates[port].dwPacketNumber;
				GetTickCount();
				WORD last = remote->wLastKey;
				const BYTE *data = (const BYTE *)&s_irStates[port].Gamepad;
				remote->unknown00 = data[0];
				remote->unknown01 = data[1];
				remote->bKeyFlags = data[2];
				// The same key again: a repeat, which comes after 0.1 s.
				s_fIRDelay = 0.14f;
				if (remote->wPreviousKey == last && last == remote->unknown00 && remote->unknown00 != 0)
					s_fIRDelay = 0.1f;
				BYTE key = remote->unknown00;
				remote->wPreviousKey = last;
				remote->wLastKey = key;
				// The key still held without the repeat timing: not reported.
				if (remote->unknown08 == key && s_fIRDelay != 0.1f)
				{
					remote->wKeyCode = 0;
					remote->unknown00 = 0;
				}
				else
				{
					remote->wKeyCode = key;
				}
				remote->unknown08 = remote->unknown00;
			}
		}
		else
		{
			XInputClose(remote->hDevice);
			remote->hDevice = XInputOpen(XDEVICE_TYPE_IR_REMOTE, port, XDEVICE_NO_SLOT, &polling);
		}
		break;
	}
	s_dwIRLastTick = GetTickCount();
}

// ---- the debug keyboard ----

static SKeystroke s_keystroke;             // 0x002720E8, just before g_keyboards
static BOOL s_bKeyboardCreated;            // 0x002776AC
static DWORD s_dwKeyboardLastTick;         // 0x002776A8

// 0x000985A4
SKeystroke *GetKeystroke()
{
	return &s_keystroke;
}

// 0x000985AA
HRESULT XBInput_InitDebugKeyboard(SKeyboardState **keyboards)
{
	if (s_bKeyboardCreated)
		return S_OK;
	XINPUT_DEBUG_KEYQUEUE_PARAMETERS queue;
	queue.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT;
	queue.dwQueueSize = 25;
	queue.dwRepeatDelay = 500;
	queue.dwRepeatInterval = 50;
	if (XInputDebugInitKeyboardQueue(&queue) != ERROR_SUCCESS)
		return E_FAIL;
	s_bKeyboardCreated = TRUE;
	DWORD devices = XGetDevices(XDEVICE_TYPE_DEBUG_KEYBOARD);
	for (DWORD port = 0; port < 4; port++)
	{
		memset(&g_keyboards[port], 0, sizeof(g_keyboards[port]));
		if (devices & (1 << port))
		{
			XINPUT_POLLING_PARAMETERS polling = {TRUE, TRUE, 0, 0x20, 0x20, 0};
			g_keyboards[port].hDevice = XInputOpen(XDEVICE_TYPE_DEBUG_KEYBOARD, port, XDEVICE_NO_SLOT, &polling);
			g_keyboards[port].dwPort = port;
		}
	}
	if (keyboards != NULL)
		*keyboards = (SKeyboardState *)&s_keystroke;   // (the keystroke comes first)
	return S_OK;
}

// 0x00098478: one keystroke from the first keyboard, at most every 30 ms.
// E_FAIL when there's none.
HRESULT XBInput_GetKeyboardInput(SKeystroke *keystroke)
{
	if (keystroke == NULL)
		keystroke = &s_keystroke;
	keystroke->ascii = 0;
	keystroke->virtualKey = 0;
	DWORD now = GetTickCount();
	if (now - s_dwKeyboardLastTick < 30)
		return E_FAIL;
	s_dwKeyboardLastTick = now;

	DWORD insertions, removals;
	if (XGetDeviceChanges(XDEVICE_TYPE_DEBUG_KEYBOARD, &insertions, &removals))
	{
		for (DWORD port = 0; port < 4; port++)
		{
			SKeyboardState *keyboard = &g_keyboards[port];
			keyboard->bRemoved = (removals & (1 << port)) != 0;
			if (keyboard->bRemoved)
			{
				XInputClose(keyboard->hDevice);
				keyboard->hDevice = NULL;
				g_keyboards[0].hDevice = NULL;
			}
			keyboard->bInserted = (insertions & (1 << port)) != 0;
			if (keyboard->bInserted)
			{
				XINPUT_POLLING_PARAMETERS polling = {TRUE, TRUE, 0, 0x20, 0x20, 0};
				keyboard->hDevice = XInputOpen(XDEVICE_TYPE_DEBUG_KEYBOARD, port, XDEVICE_NO_SLOT, &polling);
			}
		}
	}
	for (DWORD port = 0; port < 4; port++)
	{
		if (g_keyboards[port].hDevice == NULL)
			continue;
		if (XInputDebugGetKeystroke((XINPUT_DEBUG_KEYSTROKE *)keystroke) != ERROR_SUCCESS)
			return E_FAIL;
		// Quirk: the port and handle of the keyboard found go into the
		// first entry.
		g_keyboards[0].dwPort = port;
		g_keyboards[0].hDevice = g_keyboards[port].hDevice;
		return S_OK;
	}
	return E_FAIL;
}

// ---- the debug mouse ----

static BOOL s_bMouseCreated;               // 0x00278DD4
static DWORD s_dwMouseDevices;             // 0x002776B0
static DWORD s_mouseLastPacket[4];         // 0x002776B4
static BOOL s_bMouseConnected;             // 0x002776C4
static BYTE s_mouseLastButtons[4];         // 0x002776C8

// 0x0009748A
HRESULT XBInput_InitDebugMouse(SMouseState **mice)
{
	if (s_bMouseCreated)
		return S_OK;
	s_dwMouseDevices = XGetDevices(XDEVICE_TYPE_DEBUG_MOUSE);
	memset(g_mice, 0, sizeof(g_mice));
	for (DWORD port = 0; port < 4; port++)
	{
		memset(&g_mice[port], 0, sizeof(g_mice[port]));
		if (s_dwMouseDevices & (1 << port))
		{
			g_mice[port].hDevice = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, port, XDEVICE_NO_SLOT, NULL);
			g_mice[port].dwPort = port;
		}
		else
		{
			g_mice[port].hDevice = NULL;
		}
	}
	if (mice != NULL)
		*mice = g_mice;
	s_bMouseCreated = TRUE;
	return S_OK;
}

// 0x000972F0
BOOL XBInput_IsMouseConnected()
{
	return s_bMouseConnected;
}

// 0x000972F6
DWORD XBInput_GetMouseInput()
{
	DWORD insertions, removals;
	if (!XGetDeviceChanges(XDEVICE_TYPE_DEBUG_MOUSE, &insertions, &removals))
	{
		// No change: open the ones seen at startup that aren't open yet.
		for (DWORD port = 0; port < 4; port++)
			if (g_mice[port].hDevice == NULL && (s_dwMouseDevices & (1 << port)))
				g_mice[port].hDevice = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, port, XDEVICE_NO_SLOT, NULL);
	}
	else
	{
		for (DWORD port = 0; port < 4; port++)
		{
			SMouseState *mouse = &g_mice[port];
			mouse->bRemoved = (removals & (1 << port)) != 0;
			if (mouse->bRemoved && mouse->hDevice != NULL)
			{
				XInputClose(mouse->hDevice);
				mouse->hDevice = NULL;
				g_mice[0].hDevice = NULL;
			}
			mouse->bInserted = (insertions & (1 << port)) != 0;
			if (mouse->bInserted && mouse->hDevice == NULL)
				mouse->hDevice = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, port, XDEVICE_NO_SLOT, NULL);
		}
	}

	DWORD changed = 0;
	s_bMouseConnected = FALSE;
	DWORD now = GetTickCount();
	for (int port = 0; port < 4; port++)
	{
		SMouseState *mouse = &g_mice[port];
		if (mouse->hDevice == NULL)
			continue;
		BYTE before = mouse->bButtons;
		s_bMouseConnected = TRUE;
		if (XInputGetState(mouse->hDevice, (XINPUT_STATE *)mouse) != ERROR_SUCCESS)
			continue;
		if (s_mouseLastPacket[port] == mouse->dwPacketNumber)
			continue;
		BYTE buttons = mouse->bButtons;
		// A press within 180 ms of the last report: a double click.
		if (buttons == 0 || now - mouse->dwLastTime > 179)
		{
			mouse->bClickButtons = 0;
		}
		else
		{
			if (before == 0 && mouse->dwPacketNumber - s_mouseLastPacket[port] < 2)
				before = s_mouseLastButtons[port];
			mouse->bClickButtons = before;
		}
		mouse->dwLastTime = now;
		s_mouseLastPacket[port] = mouse->dwPacketNumber;
		changed |= 1 << port;
		if (buttons != 0 || mouse->cWheel != 0)
			s_mouseLastButtons[port] = buttons;
		return changed;
	}
	return changed;
}
