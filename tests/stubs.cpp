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

CXmlDocument *g_pLanguage;
CGamepadInput g_gamepadInput;
CNewsfeed g_newsfeed;
CSoundEffects g_soundEffects;

BOOL MountDrive(char drive) { printf("[stub] MountDrive(%c)\n", drive); return TRUE; }
void UnmountDrive(char drive) { printf("[stub] UnmountDrive(%c)\n", drive); }
void SetXboxSystemTime(SYSTEMTIME st, BOOL) { printf("[stub] SetXboxSystemTime(%d)\n", st.wYear); }
void SetFanSpeed(int speed) { printf("[stub] SetFanSpeed(%d)\n", speed); }
int GetFanSpeed() { return 0; }
void LoadLanguageFile(const char *fileName) { printf("[stub] LoadLanguageFile(%s)\n", fileName); }
int CGamepadInput::GetAction() { return INPUT_NONE; }
void UpdateGamepads(CXBApplicationEx *) {}
HRESULT CNewsfeed::AddFeed(const char *url, const char *title) { printf("[stub] AddFeed(%s, '%s')\n", url, title); return S_OK; }
void CSoundEffects::SetQuiet(BOOL quiet) { m_bQuiet = quiet; }
void SAudioSettings::SetEnable51(BOOL enable) { m_bEnable51 = enable; }
BOOL WriteTextFile(const char *path, const char *, BOOL) { printf("[stub] WriteTextFile(%s)\n", path); return TRUE; }
