// The ISO 9660 reader from XBMC (iso9660.cpp, Team XBMC), linked into
// UnleashX for DVD data discs. Library code: declared, not decompiled. The
// methods are LTCG copies with register arguments (noted).
#pragma once

#include "../common/Platform.h"

// 0xCF0 bytes, vtable 0x0025A048.
class iso9660
{
public:
	iso9660();                                                          // 0x000B2E1F
	virtual ~iso9660();                                                 // 0x000B2494

	// (this in ecx, data in eax) -1 when nothing matches.
	HANDLE FindFirstFile(const char *pattern, WIN32_FIND_DATAA *data);  // 0x000B206F
	// (this in esi, data in edx)
	int FindNextFile(HANDLE find, WIN32_FIND_DATAA *data);              // 0x000B1D3E
	// (this in esi)
	bool FindClose(HANDLE find);                                        // 0x000B1D22
	// The file to read; -1 if it isn't on the disc. (this in eax)
	HANDLE OpenFile(const char *path);                                  // 0x000B227A
	void CloseFile(HANDLE file);                                        // 0x000B1D07 (this in eax)
	// Bytes read, 0 or less at the end. (this in ebx)
	long ReadFile(HANDLE file, BYTE *buffer, long size);                // 0x000B214A
	// (this in ecx)
	LONGLONG Seek(HANDLE file, LONGLONG offset, int whence);            // 0x000B1C0F
	LONGLONG GetFileSize(HANDLE file);                                  // 0x000B1C02 (this in ecx)

	// The disc's size: the volume's block count times its block size,
	// unless set at +0xCE8. (this in ecx)
	ULONGLONG GetDiscSize();                                            // 0x00031564
	// The primary volume descriptor's volume name. (this in eax)
	const char *GetVolumeName();                                        // 0x00031551

	BYTE m_reserved[0xCEC];
};
// The device handle the reader opened (cleared by its destructor).
extern HANDLE g_hIso9660Device;   // 0x0027625C

// The data disc's reader, while one is in the drive (set by the DVD media
// check, 0x000AB9D8). 0x00276380
extern iso9660 *g_pDvdIso9660;
inline iso9660 *GetDvdIso9660() { return g_pDvdIso9660; }   // 0x000AA7E2
