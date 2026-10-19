// Drive letters. UnleashX maps its own letters with kernel symbolic links:
//
//   C E F G X Y Z   hard disk partitions 2,1,6,7,3,4,5   (type DRIVE_HDD)
//   D               DVD drive                           (type DRIVE_DVD)
//   H..O            memory units (port*2 + slot)        (type DRIVE_MU)
//   P..S            USB storage partitions (FAT32)      (type DRIVE_USB)
//   T U             TDATA / UDATA folders on E:, U: is per title
//   V               second CD-ROM device
//
// Quirk: S and T share one SDriveInfo slot.
#pragma once

#include <string>

#include "../common/Platform.h"

enum DriveType
{
	DRIVE_HDD = 0,
	DRIVE_DVD = 1,
	DRIVE_MU = 2,
	DRIVE_USB = 3,
};

#pragma pack(push, 1)
// size 0x97 (packed)
struct SDriveInfo
{
	char letter;                  // +0x00 0 = slot unused
	char linkName[10];            // +0x01 "\??\X:"
	char devicePath[30];          // +0x0B "\Device\Harddisk0\Partition1", MU name...
	int type;                     // +0x29 DriveType
	ULONGLONG totalBytes;         // +0x2D
	ULONGLONG freeBytes;          // +0x35
	ULONGLONG usedBytes;          // +0x3D
	BOOL mounted;                 // +0x45
	BOOL hasMedia;                // +0x49 has space info / media present
	BYTE unknown4D[0x14];         // +0x4D
	DWORD volumeSerial;           // +0x61
	DWORD maxComponentLength;     // +0x65
	char fileSystem[10];          // +0x69 "FATX", ...
	int muIndex;                  // +0x73 memory unit index (letter - 'H')
	DWORD port;                   // +0x77
	DWORD slot;                   // +0x7B
	BOOL muOrUsb;                 // +0x7F TODO: verify
	DWORD dvdUnknown83;           // +0x83 TODO
	BYTE unknown87[4];            // +0x87
	char label[11];               // +0x8B copy of devicePath's start (MU/USB name)
	BYTE unknown96;               // +0x96
};
#pragma pack(pop)

// One slot per letter C..W (see GetDriveInfo for the mapping).
#define DRIVE_INFO_COUNT 0x17
extern SDriveInfo g_drives[DRIVE_INFO_COUNT];             // 0x00276388

SDriveInfo *GetDriveInfo(char drive);                     // 0x000AA69E
inline BOOL IsDriveInfoMounted(SDriveInfo *info)
{
	return info->mounted;
}

BOOL MountDrive(char drive);                              // 0x000AAC8B
void UnmountDrive(char drive);                            // 0x000AA99A
void MountAllDrives();                                    // 0x000AB172
BOOL RemountDrive(SDriveInfo *info);                      // 0x000AB915
BOOL RemountDriveLetter(char drive);                      // 0x000ABFFE
// Maps folder (e.g. "E:\Games\Halo") to drive as if it were a partition.
BOOL MapFolderToDrive(SDriveInfo *info, char drive, const char *folder);   // 0x000AAA8A
// Maps U: to E:\UDATA\<title id> for the XBE and writes its TitleImage.xbx
// and TitleMeta.xbx there if missing.
void MountTitleDataDrive(const char *xbePath);             // 0x000AB2FF
void RefreshMemoryUnits();                                // 0x000AB587
void UpdateDriveSpace(SDriveInfo *info);                  // 0x000AB6D4 TODO: decompile the USB/MU branches
// Memory units once a second, every drive's space every 30 seconds or when
// the FTP server received a file. force: also when the next update is more
// than half a second away.
void RefreshDrives(BOOL force);                           // 0x000AB7F1
HRESULT FormatDrive(char drive);                          // 0x000A956B TODO: decompile

// total/free/used bytes of a drive, each divided by unit.
void GetDiskSpace(char drive, ULONGLONG *total, ULONGLONG *free, ULONGLONG *used, int unit);   // 0x0009D6A3

// F: and G: are optional (EnableDriveF/EnableDriveG).
extern BOOL g_bDriveFMapped;       // 0x0026BBF4
extern BOOL g_bDriveGMapped;       // 0x0027637C
inline BOOL IsDriveFMapped()       // 0x000AA690
{
	return g_bDriveFMapped;
}
inline BOOL IsDriveGMapped()       // 0x000AA68A
{
	return g_bDriveGMapped;
}
void SetDriveFMapped(BOOL mapped); // 0x000AB8F3
void SetDriveGMapped(BOOL mapped); // 0x000AB8D1

// DVD drive state (see Dvd.cpp).
enum DiscType
{
	DISC_UNKNOWN = 0,
	DISC_GAME = 1,
	DISC_VIDEO = 2,
	DISC_DATA = 3,
	DISC_AUDIO = 4,
	DISC_DATA2 = 5,
	DISC_NONE = 6,
	DISC_CHECKING = 7,
	DISC_TRAY_MOVING = 8,
};
extern int g_nDiscType;                    // 0x0026BBEC
extern wchar_t g_szDiscTitle[0x80];        // 0x00276278
int GetDiscType();                         // 0x000AA903
BOOL IsDiscPresent();                      // 0x000AA909
const wchar_t *GetDiscTitleIfPresent();    // 0x000AA91E
const wchar_t *GetDiscTypeName();          // 0x000AA7E8
const wchar_t *GetTrayStateName();         // 0x000AA85B
const wchar_t *GetDiscTitle();             // 0x000AB285
BOOL GetMemoryUnitsChanged();              // 0x000AA696
