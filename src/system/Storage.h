// Memory units and USB storage (FAT32), plus other pieces Drives.cpp uses.
#pragma once

#include <string>

#include "../common/Platform.h"

struct SDriveInfo;

// Mounts the memory unit (or USB storage device) in info's slot; USB
// partitions go to P..S. 0x000AAC8B (DRIVE_MU branch) TODO: decompile
BOOL MountMemoryUnit(SDriveInfo *info, char drive);
// 0x000AA99A (DRIVE_MU branch) TODO: decompile
BOOL UnmountMemoryUnit(SDriveInfo *info);
void InitMemoryUnitTable();                         // 0x000315A5 TODO: decompile
HRESULT GetUtilityDriveDevice(char *devicePath);    // 0x0009DE6C TODO: decompile

// Serializes file operations between threads. TODO: verify
void EnterFileLock();                               // 0x00018FF7 TODO: decompile
void LeaveFileLock();                               // 0x0001901A TODO: decompile

// Fields read from an XBE's header/certificate.
struct SXbeInfo
{
	DWORD titleId;
	DWORD unknown04;
	std::string titleName;     // TODO: verify layout (0x000A608D)
};
BOOL ReadXbeInfo(const char *xbePath, SXbeInfo *info);             // 0x000A608D TODO: decompile
std::string GetXbeTitleName(const char *xbePath);                  // 0x000A5F49 TODO: decompile
BOOL ExtractXbeTitleImage(const char *xbePath, const char *outFile);   // 0x000A4D10 TODO: decompile
