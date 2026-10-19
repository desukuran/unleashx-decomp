// The FAT32 driver for USB drives (P: to S:) linked into UnleashX
// (0x0006A400-0x0006E300). Library code: only the calls the virtual file
// layer makes are declared, with their register arguments. The names are
// reconstructed. TODO: identify the library
#pragma once

#include "../common/Platform.h"

// A mounted drive.
struct CFat32Volume
{
	BYTE m_reserved[0x314];
	struct CFat32Io *m_pIo;         // +0x314 created by Fat32GetIo
	void *m_pDevice;                // +0x318
};

// The drive's I/O state (sector cache), passed to most calls.
struct CFat32Io;

// An open file. Only the fields the file layer reads.
struct FAT32_FILE
{
	DWORD unknown00;
	DWORD startCluster;             // +0x04 what the file layer uses as a handle
	DWORD unknown08;
	DWORD position;                 // +0x0C
	DWORD unknown10;
	DWORD length;                   // +0x14
	BYTE unknown18[0x42B - 0x18];
	BOOL open;                      // +0x42B
};

// The mounted volumes, by USB port (P: is 0).
extern CFat32Volume *g_fat32Volumes[4];                     // 0x002776F4
// (index in eax) NULL if that drive isn't mounted.
inline CFat32Volume *Fat32GetVolume(int index) { return g_fat32Volumes[index & 3]; }   // 0x0006CCE7
CFat32Volume *Fat32MountVolume(int index);                  // 0x0006CEAB
// (volume in esi)
CFat32Io *Fat32GetIo(CFat32Volume *volume);                 // 0x0006D1DA
// mode is "r", "w" or "w+". NULL on failure.
FAT32_FILE *Fat32Open(CFat32Io *io, const char *path, const char *mode);   // 0x0006C3D1
// Bytes read, 0 or less at the end. (file in eax)
int Fat32Read(FAT32_FILE *file, CFat32Io *io, void *buffer, int size);     // 0x0006BAAE
// (file in ecx, size in eax; 0 = strlen)
int Fat32Write(FAT32_FILE *file, CFat32Io *io, const void *data, int size);   // 0x0006BE2C
// (file in esi, io in edi)
void Fat32Close(FAT32_FILE *file, CFat32Io *io);            // 0x0006BF08
// origin: 0 begin, 1 current, 2 end. (file in esi, io in edi, offset in
// eax, origin in ecx)
int Fat32Seek(FAT32_FILE *file, CFat32Io *io, long offset, int origin);    // 0x0006B789
// 0 when deleted.
int Fat32Remove(CFat32Io *io, const char *path);            // 0x0006BFE9
// Creates a folder (returned open). TODO: what the flags are
FAT32_FILE *Fat32CreateDirectory(CFat32Io *io, const char *path, int unknown1, int unknown2,
                                 int unknown3);              // 0x0006C0E8
// A Win32-style search of the volume; -1 if nothing matches.
HANDLE Fat32FindFirst(CFat32Volume *volume, const char *pattern, WIN32_FIND_DATAA *data);   // 0x0006DF68
// (find in eax)
BOOL Fat32FindNext(CFat32Volume *volume, HANDLE find, WIN32_FIND_DATAA *data);    // 0x0006D29D
// The 32-byte directory entry of the file starting at cluster. (volume in
// eax)
BOOL Fat32GetDirEntry(CFat32Volume *volume, DWORD cluster, BYTE entry[32]);      // 0x0006DC68
// (volume in eax)
BOOL Fat32RemoveDirectory(CFat32Volume *volume, const char *path);               // 0x0006E12E
