// CXBApplicationEx::Run: the XDK sample framework's frame loop, as UnleashX
// changed it (the rest of the framework is library code). Each frame:
// read the controllers, the remote, the debug keyboard and the USB mouse;
// merge the four gamepads into one; map keys and the mouse wheel onto the
// gamepad; the quit combination; the timers; then FrameMove and Render.
// Xbox only.
#include "XBApp.h"

#include <cstring>
#include <cwchar>

#include "../common/XboxKernel.h"
#include "../input/XBInput.h"

// The pointer state the menus read (Menu.h).
BOOL g_bPointerEvent;          // 0x0027213C
int g_nPointerPort;            // 0x00272140 the mouse with the last data
BOOL g_bPointerActive;         // 0x00272144
BOOL g_bPointerClicked;        // 0x00272148
BOOL g_bPointerSecondButton;   // 0x0027214C
BOOL g_bPointerThirdButton;    // 0x00272150 (never read)
BOOL g_bPointerWheelUp;        // 0x00272154
BOOL g_bPointerWheelDown;      // 0x00272158
int g_nPointerX;               // 0x0027215C
int g_nPointerY;               // 0x00272160
BOOL g_bPointerMoved;          // 0x00272164
BOOL g_bPointerDoubleClick;    // 0x00272168
DWORD g_dwPointerMoveTime;     // 0x0027216C

// Thumbstick values inside this are ignored when the pads are merged.
#define THUMB_DEADZONE 7864

// The debug keyboard's virtual keys mapped onto the gamepad.
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define KEYSTROKE_CTRL 0x01

static DWORD s_dwFrames;       // 0x00278DCC
static float s_fLastFPSTime;   // 0x00278DD0

static void AddThumb(int *total, short value)
{
	if (value > THUMB_DEADZONE || value < -THUMB_DEADZONE)
		*total += value;
}

static void PressButton(XBGAMEPAD &pad, WORD button)
{
	pad.wPressedButtons |= button;
	pad.wButtons |= button;
}

// 0x0009A514
// (Never returns.)
int CXBApplicationEx::Run()
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	float secondsPerTick = 1.0f / (float)frequency.QuadPart;
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	LONGLONG last = now.QuadPart;
	// The pointer starts in the middle of the screen.
	int pointerX = (int)(m_dwScreenWidth >> 1);
	int pointerY = (int)(m_dwScreenHeight >> 1);
	LONGLONG appTime = 0;
	DWORD mouseData = 0;
	BOOL moving = FALSE;
	memset(&g_bPointerEvent, 0, 13 * sizeof(DWORD));   // (all the pointer globals)

	for (;;)
	{
		XBInput_GetInput(m_pGamepads);
		XBInput_GetIR_RemoteInput(m_pIRRemotes, m_fTime);
		BOOL keystroke = XBInput_GetKeyboardInput(NULL) >= 0;
		if (!keystroke)
			mouseData = XBInput_GetMouseInput();
		g_bPointerActive = XBInput_IsMouseConnected();
		g_bPointerWheelUp = FALSE;
		g_bPointerWheelDown = FALSE;
		g_bPointerSecondButton = FALSE;
		g_bPointerThirdButton = FALSE;
		g_bPointerClicked = FALSE;
		g_bPointerDoubleClick = FALSE;
		g_bPointerMoved = FALSE;
		g_bPointerEvent = FALSE;
		g_nPointerPort = 0;
		g_nPointerY = 0;
		g_nPointerX = 0;
		memset(&m_defaultGamepad, 0, sizeof(m_defaultGamepad));
		memset(&m_defaultIR, 0, sizeof(m_defaultIR));

		// The four gamepads merged (as UpdateGamepads does; here the remote
		// is only taken from ports without a gamepad).
		int thumbLX = 0, thumbLY = 0, thumbRX = 0, thumbRY = 0;
		XBGAMEPAD &merged = m_defaultGamepad;
		for (int port = 0; port < 4; port++)
		{
			const XBGAMEPAD &pad = m_pGamepads[port];
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
			else
			{
				const SIRRemote &remote = m_pIRRemotes[port];
				if (remote.hDevice != NULL)
				{
					m_defaultIR.unknown00 = remote.unknown00;
					m_defaultIR.wKeyCode = remote.wKeyCode;
					m_defaultIR.unknown08 = remote.unknown08;
					m_defaultIR.dwPort = port;
				}
			}
			if (mouseData & (1 << port))
				g_nPointerPort = port;
		}

		// A remote key code in the printable range types as a character.
		if (m_pKeystroke->ascii == 0 && m_defaultIR.wKeyCode > 0x1F && m_defaultIR.wKeyCode < 0x80)
			m_pKeystroke->ascii = (char)m_defaultIR.wKeyCode;

		if (keystroke)
		{
			// Enter is A (Ctrl: START), Esc is B (Ctrl: BACK), the arrows the
			// d-pad.
			switch (m_pKeystroke->virtualKey)
			{
			case VK_RETURN:
				if (m_pKeystroke->flags & KEYSTROKE_CTRL)
				{
					PressButton(merged, XINPUT_GAMEPAD_START);
				}
				else
				{
					merged.bPressedAnalogButtons[0] = TRUE;
					merged.bAnalogButtons[0] = 0xFF;
				}
				break;
			case VK_ESCAPE:
				if (m_pKeystroke->flags & KEYSTROKE_CTRL)
				{
					PressButton(merged, XINPUT_GAMEPAD_BACK);
				}
				else
				{
					merged.bPressedAnalogButtons[1] = TRUE;
					merged.bAnalogButtons[1] = 0xFF;
				}
				break;
			case VK_LEFT:
				PressButton(merged, XINPUT_GAMEPAD_DPAD_LEFT);
				break;
			case VK_UP:
				PressButton(merged, XINPUT_GAMEPAD_DPAD_UP);
				break;
			case VK_RIGHT:
				PressButton(merged, XINPUT_GAMEPAD_DPAD_RIGHT);
				break;
			case VK_DOWN:
				PressButton(merged, XINPUT_GAMEPAD_DPAD_DOWN);
				break;
			}
		}
		else if (mouseData != 0)
		{
			g_bPointerEvent = TRUE;
			g_dwPointerMoveTime = GetTickCount();
			const SMouseState &mouse = m_pMice[g_nPointerPort];
			g_bPointerDoubleClick = mouse.bButtons != 0 && mouse.bClickButtons == mouse.bButtons;
			g_bPointerWheelDown = mouse.cWheel < 0;
			g_bPointerWheelUp = mouse.cWheel > 0;
			// Kept on the screen (0..width, 0..height).
			pointerX += mouse.cMickeysX;
			if (pointerX < 0)
				pointerX = 0;
			else if ((DWORD)pointerX > m_dwScreenWidth)
				pointerX = (int)m_dwScreenWidth;
			pointerY += mouse.cMickeysY;
			if (pointerY < 0)
				pointerY = 0;
			else if ((DWORD)pointerY > m_dwScreenHeight)
				pointerY = (int)m_dwScreenHeight;
			g_bPointerClicked = mouse.bButtons & 1;
			g_bPointerSecondButton = mouse.bButtons & 2;
			g_bPointerThirdButton = mouse.bButtons & 4;
			moving = mouse.cMickeysX != 0 || mouse.cMickeysY != 0;
			// The wheel scrolls like the d-pad.
			if (g_bPointerWheelDown)
				PressButton(merged, XINPUT_GAMEPAD_DPAD_DOWN);
			else if (g_bPointerWheelUp)
				PressButton(merged, XINPUT_GAMEPAD_DPAD_UP);
		}
		// "Moved" is reported the first frame without new data after a move.
		if (!g_bPointerEvent && moving)
		{
			moving = FALSE;
			g_bPointerMoved = TRUE;
		}
		g_nPointerX = pointerX;
		g_nPointerY = pointerY;
		merged.sThumbLX = (short)thumbLX;
		merged.sThumbLY = (short)thumbLY;
		merged.sThumbRX = (short)thumbRX;
		merged.sThumbRY = (short)thumbRY;

		// Both triggers with BACK and BLACK (or BACK and START): back to the
		// dashboard.
		if (merged.bAnalogButtons[6] > 30 && merged.bAnalogButtons[7] > 30 && (merged.wButtons & XINPUT_GAMEPAD_BACK) &&
		    (merged.bAnalogButtons[4] > 30 || (merged.wButtons & XINPUT_GAMEPAD_START)))
		{
			LAUNCH_DATA data;
			memset(&data, 0, sizeof(data));
			XLaunchNewImageA(NULL, &data);
		}
		if (merged.wPressedButtons & XINPUT_GAMEPAD_START)
			m_bPaused = !m_bPaused;

		// The clocks: the time, the frame's, and the app's (stopped while
		// paused).
		QueryPerformanceCounter(&now);
		LONGLONG elapsed = now.QuadPart - last;
		last = now.QuadPart;
		LONGLONG appElapsed = m_bPaused ? 0 : elapsed;
		appTime += appElapsed;
		float time = (float)now.QuadPart * secondsPerTick;
		s_dwFrames++;
		m_fTime = time;
		float sinceFPS = time - s_fLastFPSTime;
		m_fElapsedTime = (float)elapsed * secondsPerTick;
		m_fAppTime = (float)appTime * secondsPerTick;
		m_fElapsedAppTime = (float)appElapsed * secondsPerTick;
		if (sinceFPS > 1.0f)
		{
			m_fFPS = (float)s_dwFrames / sinceFPS;
			s_fLastFPSTime = time;
			s_dwFrames = 0;
			swprintf(m_strFrameRate, 20, L"%0.02f fps", (double)m_fFPS);
		}
		FrameMove();
		Render();
	}
}
