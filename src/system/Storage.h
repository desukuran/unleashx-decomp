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

// Fields read from an XBE's header/certificate. 0x78 bytes.
struct SXbeInfo
{
	DWORD titleId;             // +0x00 TODO: verify
	std::string titleName;     // +0x04
	BYTE unknown20[0x54];      // +0x20
	DWORD gameRegion;          // +0x74 certificate game region flags
};
BOOL ReadXbeInfo(const char *xbePath, SXbeInfo *info);             // 0x000A608D (Xbe.cpp)

// An XBE's image header and certificate as read by ReadXbeHeader(). 0x35C
// bytes; only the fields used are named.
struct SXbeHeaderInfo
{
	BYTE header[0x110];              // +0x000 image header
	DWORD sizeOfImageHeader;         // +0x110 the certificate follows the image header
	BYTE header114[0x64];            // +0x114
	struct
	{
		DWORD size;                  // +0x00
		DWORD timestamp;             // +0x04
		DWORD titleId;               // +0x08
		WORD titleName[40];          // +0x0C UTF-16
		DWORD alternateTitleIds[16]; // +0x5C
		DWORD allowedMedia;          // +0x9C
		DWORD gameRegion;            // +0xA0
		DWORD gameRatings;           // +0xA4
		BYTE rest[0x1D0 - 0xA8];
	} certificate;                   // +0x178
	BYTE unused348[0x14];            // +0x348 cleared, not read
};
BOOL ReadXbeHeader(const char *xbePath, SXbeHeaderInfo *info);     // 0x0009E08A (Xbe.cpp)
// Writes a new title name (40 characters) into the XBE's certificate.
// TRUE if it was written or already the same. TODO: decompile
BOOL SetXbeTitleName(const char *xbePath, const wchar_t *title);    // 0x0009DB0C
std::string GetXbeTitleName(const char *xbePath);                  // 0x000A5F49 (Xbe.cpp)
BOOL ExtractXbeTitleImage(const char *xbePath, const char *outFile);   // 0x000A4D10 TODO: decompile
