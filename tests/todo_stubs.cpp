// Host stand-ins for functions declared "TODO: decompile". Remove an entry
// here when the real implementation lands.
#include <cstdio>

#include "../src/common/FileUtil.h"
#include "../src/system/Drives.h"
#include "../src/system/Storage.h"
#include "../src/system/SystemInfo.h"
#include "../src/thirdparty/XKUtils.h"
#include "../src/audio/MusicManager.h"
#include "../src/net/Newsfeed.h"
#include "../src/common/StrUtil.h"
#include <cstring>
#include <cwchar>

BOOL MountMemoryUnit(SDriveInfo *, char) { return FALSE; }
BOOL UnmountMemoryUnit(SDriveInfo *) { return TRUE; }
void InitMemoryUnitTable() {}
HRESULT GetUtilityDriveDevice(char *) { return E_FAIL; }
void EnterFileLock() {}
void LeaveFileLock() {}
BOOL ReadXbeInfo(const char *, SXbeInfo *) { return FALSE; }
BOOL ExtractXbeTitleImage(const char *, const char *) { return FALSE; }
HRESULT FormatDrive(char) { return E_FAIL; }
BOOL CreateDirectoryTree(const char *) { return FALSE; }
BOOL WriteTextFile(const char *path, const char *, BOOL) { printf("[stub] WriteTextFile(%s)\n", path); return TRUE; }
std::string GetPathDirectory(const char *) { return std::string(); }
void MakeFatxPath(const char *path, std::string &out) { out = path; }
std::string GetPersistentDataPath(const wchar_t *, unsigned int) { return std::string(); }
std::string GetXbeTitleName(const char *) { return std::string(); }

// SystemInfo.h
wchar_t GetThousandsSeparator() { return L','; }
wchar_t GetDecimalSeparator() { return L'.'; }
void FormatNumberW(wchar_t *out, ULONGLONG n) { swprintf(out, 100, L"%llu", (unsigned long long)n); }
std::string FormatDouble(int decimals, double v) { std::string s; StrFormat(s, "%.*f", decimals, v); return s; }
std::string FormatIPAddress(DWORD addr) { std::string s; StrFormat(s, "%u.%u.%u.%u", addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, addr >> 24); return s; }
std::string GetKernelVersionString() { return "1.0.5838.1"; }
std::string GetFlashChipName(BOOL) { return "Unknown"; }
void GetXboxVersion(char *out) { strcpy(out, "V1.0"); }
float ReadTemperature(BOOL, BOOL) { return 40.0f; }
void FormatFileTime(const FILETIME *, wchar_t *date, wchar_t *time, BOOL, BOOL)
{
	if (date) wcscpy(date, L"2026-10-19");
	if (time) wcscpy(time, L"12:00:00");
}
void GetIdeModel(BOOL hdd, char *out, unsigned) { strcpy(out, hdd ? "ST310014A   " : "PHILIPS XBOX DVD"); }
void GetIdeSerial(BOOL, char *out, unsigned) { out[0] = 0; }
void GetIdeFirmware(BOOL, char *out, unsigned) { strcpy(out, "3.10"); }
WORD GetHddSecurityStatus() { return HDD_SECURITY_SUPPORTED | HDD_SECURITY_ENABLED; }
BYTE GetHddTemperature() { return 0; }
CMD5::CMD5(const void *, int) { m_bValid = false; }
BOOL CMD5::HashMemory(const void *, int) { return FALSE; }

// XKUtils.h
XKEEPROM g_eeprom;
void XKEEPROM::GetMACAddressString(char *mac, DWORD *length) { strcpy(mac, "00-50-F2-00-00-01"); *length = 6; }
void XKEEPROM::GetSerialNumberString(char *serial, DWORD *length) { strcpy(serial, "123456789012"); *length = 12; }
void XKEEPROM::GetHDKeyString(char *key, DWORD *length) { strcpy(key, "00000000000000000000000000000000"); *length = 16; }

// MusicManager.h
CMusicManager *g_pMusicManager = NULL;
HRESULT CMusicManager::GetSongInfo(wchar_t *, wchar_t *, DWORD *) { return E_FAIL; }
SSongFormat *CMusicManager::GetSongFormat() { return NULL; }
float CMusicManager::GetSongPosition() { return 0.0f; }

// Newsfeed.h
std::string CNewsfeed::GetUrl(int) { return std::string(); }
std::string CNewsfeed::GetContent(const char *) { return std::string(); }
