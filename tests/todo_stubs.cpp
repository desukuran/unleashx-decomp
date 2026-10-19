// Host stand-ins for functions declared "TODO: decompile". Remove an entry
// here when the real implementation lands.
#include <cstdio>

#include "../src/common/FileUtil.h"
#include "../src/system/Drives.h"
#include "../src/system/Storage.h"
#include "../src/system/FileOps.h"
#include "../src/system/SystemInfo.h"
#include "../src/thirdparty/XKUtils.h"
#include "../src/audio/MusicManager.h"
#include "../src/common/StrUtil.h"
#include "../src/net/FtpClient.h"
#include <cstring>
#include <cwchar>

BOOL MountMemoryUnit(SDriveInfo *, char) { return FALSE; }
BOOL UnmountMemoryUnit(SDriveInfo *) { return TRUE; }
void InitMemoryUnitTable() {}
HRESULT GetUtilityDriveDevice(char *) { return E_FAIL; }
HRESULT FormatDrive(char) { return E_FAIL; }

// SystemInfo.h (Hardware.cpp is Xbox only)
std::string GetFlashChipName(BOOL) { return "Unknown"; }
void GetXboxVersion(char *out) { strcpy(out, "V1.0"); }
float ReadTemperature(BOOL, BOOL) { return 40.0f; }
void GetIdeModel(BOOL hdd, char *out, unsigned) { strcpy(out, hdd ? "ST310014A   " : "PHILIPS XBOX DVD"); }
void GetIdeSerial(BOOL, char *out, unsigned) { out[0] = 0; }
void GetIdeFirmware(BOOL, char *out, unsigned) { strcpy(out, "3.10"); }
WORD GetHddSecurityStatus() { return HDD_SECURITY_SUPPORTED | HDD_SECURITY_ENABLED; }
BYTE GetHddTemperature() { return 0; }
void GetSmartAttribute(unsigned, DWORD *, DWORD *, DWORD *) {}

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

// FileOps.h
void ZipFiles(const char *source, const char *zipFile, int) { printf("[stub] ZipFiles(%s, %s)\n", source, zipFile); }

// Drives.h
void RefreshMemoryUnits() {}
void UpdateDriveSpace(SDriveInfo *) {}

