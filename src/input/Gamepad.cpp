// CGamepadInput: one InputAction per frame from the gamepads and the IR
// remote, with auto-repeat.
#include "Gamepad.h"

#include <cmath>
#include <cstring>

#include "../app/XBApp.h"

// The ports last used (-1 = none yet).
CGamepadInput g_gamepadInput;     // 0x00278280
static int s_nGamepadPort = -1;   // 0x0026CC00
static int s_nRemotePort = -1;    // 0x0026C99C
// Quick repeats so far; every sixth one waits again.
static DWORD s_dwRepeats;         // 0x00278D4C

// An InputAction forced through XBGAMEPAD::nAction.
static const int s_forcedActions[14] = {
	INPUT_START, INPUT_BACK, INPUT_A, INPUT_B, INPUT_X, INPUT_Y, INPUT_BLACK, INPUT_WHITE,
	INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT, INPUT_LTRIGGER, INPUT_RTRIGGER,
};

// 0x00021F02
CGamepadInput::CGamepadInput()
{
	m_fPadRepeats = 0.0f;
	m_fDelays[0] = 0.085f;
	m_fDelays[1] = 0.333f;
	m_fDelays[2] = 0.433f;
	m_fDelays[3] = 0.28f;
	m_fDelays[4] = 0.15f;
	m_fDelays[5] = 0.03f;
	m_pFixedGamepad = NULL;
	m_pGamepad = NULL;
	m_pRemote = NULL;
	m_pFixedRemote = NULL;
	m_unknown10 = 0;
}

// 0x0002AF08
XBGAMEPAD *CGamepadInput::GetActiveGamepad()
{
	if (m_pFixedGamepad != NULL)
		return m_pFixedGamepad;
	if (s_nGamepadPort != -1 && g_Gamepads[s_nGamepadPort].hDevice != NULL)
		return &g_Gamepads[s_nGamepadPort];

	int firstConnected = -1;
	for (int port = 0; port < 4; port++)
	{
		XBGAMEPAD *pad = &g_Gamepads[port];
		if (pad->hDevice == NULL)
			continue;
		if (firstConnected == -1)
			firstConnected = port;
		if (HasInput(pad))
		{
			s_nGamepadPort = port;
			return pad;
		}
	}
	return firstConnected == -1 ? NULL : &g_Gamepads[firstConnected];
}

// 0x00026248
SIRRemote *CGamepadInput::GetActiveRemote()
{
	if (m_pFixedRemote != NULL)
		return m_pFixedRemote;
	if (s_nRemotePort > -1 && g_irRemotes[s_nRemotePort].hDevice != NULL)
		return &g_irRemotes[s_nRemotePort];

	int firstConnected = -1;
	for (int port = 0; port < 4; port++)
	{
		SIRRemote *remote = &g_irRemotes[port];
		if (remote->hDevice == NULL)
			continue;
		if (firstConnected == -1)
			firstConnected = port;
		if (MapRemoteToGamepad(remote))
		{
			s_nRemotePort = port;
			return remote;
		}
	}
	return firstConnected == -1 ? NULL : &g_irRemotes[firstConnected];
}

// 0x000262E6
// The remote's arrows, SELECT and BACK act on the gamepad in use too.
// Quirk: left and right count as input but aren't copied.
BOOL CGamepadInput::MapRemoteToGamepad(SIRRemote *remote)
{
	if (remote == NULL)
	{
		remote = m_pRemote;
		if (remote == NULL)
			return FALSE;
	}
	XBGAMEPAD *pad = m_pGamepad;
	if (pad == NULL || remote->wKeyCode == 0)
		return FALSE;
	switch (remote->wKeyCode)
	{
	case 0x0B:   // SELECT: A
		pad->bPressedAnalogButtons[0] = TRUE;
		pad->bAnalogButtons[0] = 0x2D;
		return TRUE;
	case 0xA6:   // up
		pad->wButtons |= 0x01;
		return TRUE;
	case 0xA7:   // down
		pad->wButtons |= 0x02;
		return TRUE;
	case 0xA8:
	case 0xA9:
		return TRUE;
	case 0xD8:   // BACK
		pad->wButtons |= 0x20;
		return TRUE;
	default:
		return FALSE;
	}
}

// 0x00026340
int CGamepadInput::GetRemoteAction()
{
	m_pRemote = GetActiveRemote();
	if (m_pRemote == NULL)
		return 0;

	if (MapRemoteToGamepad(m_pRemote))
	{
		// The arrows repeat (after 0.433s, then every 0.28s).
		if (m_remoteTimer.IsRunning())
		{
			if (m_fRemoteDelay > m_remoteTimer.GetElapsedSeconds())
				return 0;
			m_fRemoteDelay = 0.28f;
			m_remoteTimer.Start();
			goto arrows;
		}
		m_fRemoteDelay = 0.433f;
		m_remoteTimer.Start();
	}
	else
	{
		m_fRemoteDelay = 0.433f;
		m_remoteTimer.Stop();
	}

	switch (m_pRemote->wKeyCode)
	{
	case 0x0B: return INPUT_IR_SELECT;
	case 0xC3: return INPUT_IR_INFO;
	case 0xC6: return INPUT_IR_1 + 8;   // 9
	case 0xC7: return INPUT_IR_1 + 7;
	case 0xC8: return INPUT_IR_1 + 6;
	case 0xC9: return INPUT_IR_1 + 5;
	case 0xCA: return INPUT_IR_1 + 4;
	case 0xCB: return INPUT_IR_1 + 3;
	case 0xCC: return INPUT_IR_1 + 2;
	case 0xCD: return INPUT_IR_1 + 1;
	case 0xCE: return INPUT_IR_1;
	case 0xCF: return INPUT_IR_0;
	case 0xD5: return INPUT_IR_DISPLAY;
	case 0xD8: return INPUT_IR_BACK;
	case 0xDD: return INPUT_IR_SKIP_PREV;
	case 0xDF: return INPUT_IR_SKIP_NEXT;
	case 0xE0: return INPUT_IR_STOP;
	case 0xE2: return INPUT_IR_REVERSE;
	case 0xE3: return INPUT_IR_FORWARD;
	case 0xE5: return INPUT_IR_TITLE;
	case 0xE6: return INPUT_IR_PAUSE;
	case 0xEA: return INPUT_IR_PLAY;
	case 0xF7: return INPUT_IR_MENU;
	}

arrows:
	switch (m_pRemote->wKeyCode)
	{
	case 0xA6: return INPUT_IR_UP;
	case 0xA7: return INPUT_IR_DOWN;
	case 0xA8: return INPUT_IR_RIGHT;
	case 0xA9: return INPUT_IR_LEFT;
	default: return 0;
	}
}

// 0x0002AFAC
BOOL CGamepadInput::HasInput(XBGAMEPAD *pad)
{
	if (pad->nAction != 0 || pad->wButtons != 0)
		return TRUE;
	for (int i = 0; i < 8; i++)
	{
		if (pad->bAnalogButtons[i] > 30)
			return TRUE;
	}
	if (fabs(pad->fX1) > 0.35f || fabs(pad->fY1) > 0.35f || fabs(pad->fX2) > 0.35f || fabs(pad->fY2) > 0.35f)
		return TRUE;

	// The remote, as if on this gamepad.
	m_pRemote = GetActiveRemote();
	if (!MapRemoteToGamepad(m_pRemote))
		return FALSE;
	switch (GetRemoteAction())
	{
	case INPUT_IR_UP:
		m_pGamepad->wButtons |= 0x01;
		break;
	case INPUT_IR_DOWN:
		m_pGamepad->wButtons |= 0x02;
		break;
	case INPUT_IR_LEFT:
		m_pGamepad->wButtons |= 0x04;
		break;
	case INPUT_IR_RIGHT:
		m_pGamepad->wButtons |= 0x08;
		break;
	case INPUT_IR_SELECT:
		m_pGamepad->bAnalogButtons[0] = 0x1F;
		break;
	case INPUT_IR_BACK:
		m_pGamepad->wButtons |= 0x20;
		break;
	}
	return TRUE;
}

// 0x0002B080
// A button that stays down repeats: after 0.333s, then every 0.085s; every
// sixth repeat waits the long delay again. The face buttons, START and
// BACK only report on the first press and after 3 repeats.
int CGamepadInput::GetAction()
{
	BOOL repeating = FALSE;
	m_pGamepad = GetActiveGamepad();
	if (m_pGamepad == NULL)
		return GetRemoteAction();

	BOOL forced = m_pGamepad->nAction != 0;
	if (HasInput(m_pGamepad))
	{
		if (m_padTimer.IsRunning())
		{
			if (m_fPadDelay > m_padTimer.GetElapsedSeconds())
				return 0;
			m_fPadRepeats = m_fPadRepeats + 1.0f;
			s_dwRepeats++;
			m_fPadDelay = 0.085f;
			if (s_dwRepeats > 5)
			{
				s_dwRepeats = 0;
			}
			else
			{
				m_padTimer.Start();
				repeating = TRUE;
			}
		}
		else
		{
			m_fPadDelay = 0.333f;
			m_padTimer.Start();
			m_fPadRepeats = 0.0f;
		}
		// Quirk: stops the remote's timer, not the gamepad's.
		m_remoteTimer.Stop();
	}
	else
	{
		m_fPadDelay = 0.333f;
		m_padTimer.Stop();
		m_fPadRepeats = 0.0f;
		if (!forced)
			return GetRemoteAction();
	}

	XBGAMEPAD *pad = m_pGamepad;
	if (pad->nAction != 0)
	{
		m_fPadRepeats = 0.0f;
		if (pad->nAction >= 1 && pad->nAction <= 14)
			return s_forcedActions[pad->nAction - 1];
	}

	if (!repeating || m_fPadRepeats > 3.0f)
	{
		m_fPadRepeats = 0.0f;
		if (pad->wButtons & 0x10)
			return INPUT_START;
		if (pad->wButtons & 0x20)
			return INPUT_BACK;
		if (pad->bAnalogButtons[0] > 30)
			return INPUT_A;
		if (pad->bAnalogButtons[1] > 30)
			return INPUT_B;
		if (pad->bAnalogButtons[2] > 30)
			return INPUT_X;
		if (pad->bAnalogButtons[3] > 30)
			return INPUT_Y;
		if (pad->bAnalogButtons[4] > 30)
			return INPUT_BLACK;
		if (pad->bAnalogButtons[5] > 30)
			return INPUT_WHITE;
	}
	if (pad->wButtons & 0x01)
		return INPUT_UP;
	if (pad->wButtons & 0x02)
		return INPUT_DOWN;
	if (pad->wButtons & 0x04)
		return INPUT_LEFT;
	if (pad->wButtons & 0x08)
		return INPUT_RIGHT;
	// The triggers repeat faster the harder they're pressed.
	if (pad->bAnalogButtons[6] > 30)
	{
		if (m_fPadDelay >= 0.085f)
			m_fPadDelay = (1.0f - (float)pad->bAnalogButtons[6] * (0.6f / 255.0f)) * 0.085f;
		return INPUT_LTRIGGER;
	}
	if (pad->bAnalogButtons[7] > 30)
	{
		if (m_fPadDelay >= 0.085f)
			m_fPadDelay = (1.0f - (float)pad->bAnalogButtons[7] * (0.6f / 255.0f)) * 0.085f;
		return INPUT_RTRIGGER;
	}
	if (pad->wButtons & 0x40)
		return INPUT_LTHUMB_CLICK;
	if (pad->wButtons & 0x80)
		return INPUT_RTHUMB_CLICK;
	if (pad->fY1 > 0.35f || -0.35f > pad->fY1 || -0.35f > pad->fX1 || pad->fX1 > 0.35f)
	{
		if (m_fPadDelay >= 0.085f)
			m_fPadDelay = 0.0255f;
		return INPUT_LTHUMB;
	}
	if (pad->fY2 > 0.35f || -0.35f > pad->fY2 || -0.35f > pad->fX2 || pad->fX2 > 0.35f)
	{
		if (m_fPadDelay >= 0.085f)
			m_fPadDelay = 0.034f;
		return INPUT_RTHUMB;
	}
	return GetRemoteAction();
}

// Thumbstick values inside this are ignored when the pads are merged (the
// XDK samples' 24% dead zone).
#define THUMB_DEADZONE 7864

static void AddThumb(short *total, short value)
{
	if (value > THUMB_DEADZONE || value < -THUMB_DEADZONE)
		*total = (short)(*total + value);
}

// 0x000E020F (app in esi)
// Reads the controllers and merges them: buttons OR'ed, sticks added
// (outside the dead zone; quirk: the sums wrap as shorts and the -1..1
// values aren't clamped), the last pad's action; the remote is the last
// port with a receiver.
void UpdateGamepads(CXBApplicationEx *app)
{
	XBInput_GetInput(app->m_pGamepads);
	memset(&app->m_defaultGamepad, 0, sizeof(app->m_defaultGamepad));
	memset(&app->m_defaultIR, 0, sizeof(app->m_defaultIR));
	short thumbLX = 0, thumbLY = 0, thumbRX = 0, thumbRY = 0;
	XBGAMEPAD &merged = app->m_defaultGamepad;
	for (DWORD port = 0; port < 4; port++)
	{
		const XBGAMEPAD &pad = app->m_pGamepads[port];
		if (pad.hDevice != NULL)
		{
			AddThumb(&thumbLX, pad.sThumbLX);
			AddThumb(&thumbLY, pad.sThumbLY);
			AddThumb(&thumbRX, pad.sThumbRX);
			AddThumb(&thumbRY, pad.sThumbRY);
			merged.fX1 += pad.fX1;
			merged.fY1 += pad.fY1;
			merged.fX2 += pad.fX2;
			merged.fY2 += pad.fY2;
			merged.wButtons |= pad.wButtons;
			merged.wPressedButtons |= pad.wPressedButtons;
			merged.wLastButtons |= pad.wLastButtons;
			if (pad.nAction != 0)
				merged.nAction = pad.nAction;
			for (int i = 0; i < 8; i++)
			{
				merged.bAnalogButtons[i] |= pad.bAnalogButtons[i];
				merged.bPressedAnalogButtons[i] |= pad.bPressedAnalogButtons[i];
				merged.bLastAnalogButtons[i] |= pad.bLastAnalogButtons[i];
			}
		}
		const SIRRemote &remote = app->m_pIRRemotes[port];
		if (remote.hDevice != NULL)
		{
			app->m_defaultIR.unknown00 = remote.unknown00;
			app->m_defaultIR.wKeyCode = remote.wKeyCode;
			app->m_defaultIR.unknown08 = remote.unknown08;
			app->m_defaultIR.dwPort = port;
		}
	}
	merged.sThumbLX = thumbLX;
	merged.sThumbLY = thumbLY;
	merged.sThumbRX = thumbRX;
	merged.sThumbRY = thumbRY;
}
