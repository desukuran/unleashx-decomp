// Memory units and USB storage (FAT32), plus other pieces Drives.cpp uses.
#pragma once

#include <string>

#include "../common/Platform.h"

struct SDriveInfo;

// The memory unit branches of MountDrive/UnmountDrive (inlined there in
// the original; Storage.cpp). Mounting tries the slot as a memory unit,
// then as USB mass storage (its FAT32 partitions go to P..S); FALSE for
// the latter.
BOOL MountMemoryUnit(SDriveInfo *info, char drive);   // 0x000AAEAA (in MountDrive)
BOOL UnmountMemoryUnit(SDriveInfo *info);             // 0x000AAA31 (in UnmountDrive)
// g_memoryUnits sized for the 8 slots, once (inlined in MountAllDrives).
void InitMemoryUnitTable();
// The kernel device the drive's link points to. (drive in eax) Quirk:
// MountAllDrives passes the letter itself as the pointer.
HRESULT GetUtilityDriveDevice(const char *drive, char *devicePath);   // 0x0009DE6C
// Formats a memory unit (H..O) as FAT.
HRESULT FormatMemoryUnit(char drive);                 // 0x0009DA9B
// Formats hard disk partition n by linking Z: to it and formatting the
// utility drive; then Z: is remounted. A status or Win32 error, 0 on
// success.
DWORD FormatPartition(int partition);                 // 0x000A94F0
// Links link (e.g. "\??\Z:") to device, deleting an old link first if
// replace. (XbeLaunch.cpp)
LONG MapDriveLink(const char *link, const char *device, BOOL replace);   // 0x000A39D9

// The guard of the drive table: one global instance whose constructor
// sets up the critical section UpdateDriveSpace takes, and whose
// destructor frees the DVD's ISO 9660 reader. (Storage.cpp)
class CStorageInfo
{
public:
	CStorageInfo();               // 0x000AA977 (this in esi)
	virtual ~CStorageInfo();      // 0x000AA953 (deleting 0x00031589)
};
extern CStorageInfo g_storageInfo;   // 0x002788E0

// Fields read from an XBE's header/certificate. 0x78 bytes; constructor
// 0x00018FF7 and destructor 0x0001901A (this in esi), both implicit.
struct SXbeInfo
{
	DWORD titleId;             // +0x00 TODO: verify
	std::string titleName;     // +0x04
	std::string unused20;      // +0x20 never filled
	std::string unused3C;      // +0x3C never filled
	std::string unused58;      // +0x58 never filled
	DWORD gameRegion;          // +0x74 certificate game region flags
};
BOOL ReadXbeInfo(const char *xbePath, SXbeInfo *info);             // 0x000A608D (Xbe.cpp)
// Lets a copied game XBE run from the hard disk: sets the certificate's
// allowed media to everything ("FF010040") and applies four byte patches.
// TRUE if any patch applied. (path in edi; XbePatch.cpp)
BOOL PatchXbe(const char *xbePath);                                 // 0x000A2245
// The hex bytes written at offset: 0, or -1 bad hex, -2 can't open, -3
// short write. (hex in eax)
int WriteXbePatchBytes(const char *hex, const char *xbePath, long offset);   // 0x0009F58D
// Where the hex bytes first appear from start on; -1 not found, -2 can't
// open. (hex in eax)
long CheckXbePatchBytes(const char *hex, const char *xbePath, long start);   // 0x0009F62A

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
// TRUE if it was written or already the same. (path in eax; XbePatch.cpp)
BOOL SetXbeTitleName(const char *xbePath, const wchar_t *title);    // 0x0009DB0C
std::string GetXbeTitleName(const char *xbePath);                  // 0x000A5F49 (Xbe.cpp)
// The kernel path of an XBE's folder (E:\Games\x.xbe ->
// \Device\Harddisk0\Partition1\Games), what D: is mapped to when it
// launches. (Xbe.cpp)
std::string GetXbeDevicePath(const char *xbePath);                 // 0x000A5C05
// Saves the XBE's title image ($$XTIMAGE section) as outFile: an XPR
// texture is written as an .xpr, a DDS is copied when outFile doesn't exist
// yet. (src/render/XbeImage.cpp)
BOOL ExtractXbeTitleImage(const char *xbePath, const char *outFile);   // 0x000A4D10
