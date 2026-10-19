// Host test stand-ins for subsystems that haven't been decompiled yet.
#include <cstdio>

#include "../src/audio/SoundEffects.h"
#include "../src/common/FileUtil.h"
#include "../src/input/Gamepad.h"
#include "../src/net/Newsfeed.h"
#include "../src/settings/AppSettings.h"
#include "../src/system/Drives.h"
#include "../src/system/SystemControl.h"
#include "../src/ui/Language.h"
#include "../src/app/XBApp.h"

CGamepadInput g_gamepadInput;
CNewsfeed g_newsfeed;
CSoundEffects g_soundEffects;

void SetXboxSystemTime(SYSTEMTIME st, BOOL) { printf("[stub] SetXboxSystemTime(%d)\n", st.wYear); }
void SetFanSpeed(int speed) { printf("[stub] SetFanSpeed(%d)\n", speed); }
int GetFanSpeed() { return 0; }
int CGamepadInput::GetAction() { return INPUT_NONE; }
void UpdateGamepads(CXBApplicationEx *) {}
HRESULT CNewsfeed::AddFeed(const char *url, const char *title) { printf("[stub] AddFeed(%s, '%s')\n", url, title); return S_OK; }
void CSoundEffects::SetQuiet(BOOL quiet) { m_bQuiet = quiet; }
void SAudioSettings::SetEnable51(BOOL enable) { m_bEnable51 = enable; }

BOOL SetLedPattern(int, BOOL) { return FALSE; }

// XBApp.h: CXBStopWatch (XDK sample framework)
#include <chrono>
static double HostSeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
CXBStopWatch::CXBStopWatch() { Reset(); }
void CXBStopWatch::Start() { *(double *)m_state = HostSeconds(); }
void CXBStopWatch::Stop() {}
void CXBStopWatch::Reset() { m_fSecsPerTick = 0; *(double *)m_state = HostSeconds(); }
float CXBStopWatch::GetElapsedSeconds() { return (float)(HostSeconds() - *(double *)m_state); }
float CXBStopWatch::GetElapsedMilliseconds() { return GetElapsedSeconds() * 1000.0f; }
