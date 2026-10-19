// Host stand-ins for functions declared "TODO: decompile". Remove an entry
// here when the real implementation lands.
#include <cstdio>

#include "../src/common/FileUtil.h"
#include "../src/system/Drives.h"
#include "../src/system/Storage.h"

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
