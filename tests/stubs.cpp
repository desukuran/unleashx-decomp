// Host test stand-ins for subsystems that haven't been decompiled yet.
#include <cstdio>

#include "../src/audio/SoundEffects.h"
#include "../src/common/FileUtil.h"
#include "../src/input/Gamepad.h"
#include "../src/settings/AppSettings.h"
#include "../src/system/Drives.h"
#include "../src/system/SystemControl.h"
#include "../src/ui/Language.h"
#include "../src/app/XBApp.h"

// Main.cpp and UnleashX.cpp (not linked on the host).
BOOL g_bColdBoot = TRUE;
class CUnleashX *g_pApp;
CSoundEffects g_soundEffects;

void SetXboxSystemTime(SYSTEMTIME st, BOOL) { printf("[stub] SetXboxSystemTime(%d)\n", st.wYear); }
void SetFanSpeed(int speed) { printf("[stub] SetFanSpeed(%d)\n", speed); }
int GetFanSpeed() { return 0; }
// The XBInput device tables (no devices on the host).
XBGAMEPAD g_Gamepads[4];
SIRRemote g_irRemotes[4];
SKeyboardState g_keyboards[4];
void XBInput_GetInput(XBGAMEPAD *) {}
CSoundEffects::CSoundEffects() : m_pSettings(NULL), m_bDisabled(FALSE), m_bQuiet(FALSE) {}
CSoundEffects::~CSoundEffects() {}
void CSoundEffects::SetQuiet(BOOL quiet) { m_bQuiet = quiet; }

BOOL SetLedPattern(int, BOOL) { return FALSE; }

// XBApp.h: CXBStopWatch (XDK sample framework)
#include <chrono>
// Microsecond ticks on the host (QueryPerformanceCounter on the Xbox).
static LONGLONG HostTicks()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}
CXBStopWatch::CXBStopWatch() : m_fSecsPerTick(1e-6f), m_nStartTick(0), m_nElapsedTicks(0), m_bRunning(FALSE) {}
void CXBStopWatch::Start() { m_nElapsedTicks = 0; m_nStartTick = HostTicks(); m_bRunning = TRUE; }
void CXBStopWatch::Stop()
{
	if (m_bRunning)
		m_nElapsedTicks += HostTicks() - m_nStartTick;
	m_bRunning = FALSE;
}
void CXBStopWatch::Reset() { m_nElapsedTicks = 0; m_nStartTick = HostTicks(); }
BOOL CXBStopWatch::IsRunning() const { return m_bRunning; }
float CXBStopWatch::GetElapsedSeconds()
{
	LONGLONG ticks = m_nElapsedTicks + (m_bRunning ? HostTicks() - m_nStartTick : 0);
	return (float)ticks * m_fSecsPerTick;
}
float CXBStopWatch::GetElapsedMilliseconds() { return GetElapsedSeconds() * 1000.0f; }
