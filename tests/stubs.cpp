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
static double HostSeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
CXBStopWatch::CXBStopWatch() { Reset(); }
// m_state: the start time, then a running flag.
void CXBStopWatch::Start() { *(double *)m_state = HostSeconds(); m_state[8] = 1; }
void CXBStopWatch::Stop() { m_state[8] = 0; }
void CXBStopWatch::Reset() { m_fSecsPerTick = 0; *(double *)m_state = HostSeconds(); m_state[8] = 0; }
BOOL CXBStopWatch::IsRunning() const { return m_state[8] != 0; }
float CXBStopWatch::GetElapsedSeconds() { return (float)(HostSeconds() - *(double *)m_state); }
float CXBStopWatch::GetElapsedMilliseconds() { return GetElapsedSeconds() * 1000.0f; }
