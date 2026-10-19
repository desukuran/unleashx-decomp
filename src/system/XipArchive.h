// XIP archives: the MS dashboard's packed media (default.xip and
// mainmenu5.xip in C:\xboxdashdata.*). UnleashX takes its wall and logo textures
// from them (see LoadResources). The class name is reconstructed.
#pragma once

#include <cstdio>

#include "../common/Platform.h"

// A file in the archive. 0x10 bytes.
struct SXipFileEntry
{
	DWORD offset;      // +0x00 from the archive's data offset
	DWORD size;        // +0x04
	DWORD type;        // +0x08 XIP_TYPE_*
	DWORD timestamp;   // +0x0C
};

#define XIP_TYPE_DIRECTORY 4   // TODO: verify (Extract skips it)

// An entry with its name. 0x118 bytes.
struct SXipItem
{
	SXipFileEntry entry;   // +0x00
	char name[0x108];      // +0x10
};

// 0x28 bytes. The first 0x10 bytes are the file's header.
class CXipArchive
{
public:
	// Opens and reads the directory. It closes first, so the object must
	// start zeroed (callers clear it). Quirk: on a failure after the file
	// was opened it's closed but m_pFile isn't cleared (nor the freed
	// tables), so Close must not be called then. (this in esi)
	HRESULT Open(const char *path);                   // 0x000BDCBE
	// Frees everything and clears the object. (this in eax)
	void Close();                                     // 0x000BDC0F
	// The entry and name of a file. (this in edx)
	HRESULT GetItem(unsigned index, SXipItem *item);  // 0x000BDC67
	// Writes a file's data to path; a directory is skipped (S_OK).
	// (this in ecx)
	HRESULT Extract(unsigned index, const char *path);   // 0x000BDAC0

	char m_magic[4];              // +0x00 "XIP0"
	DWORD m_dwDataOffset;         // +0x04
	WORD m_wFileCount;            // +0x08
	WORD m_wNameCount;            // +0x0A
	DWORD m_dwDataSize;           // +0x0C
	SXipFileEntry *m_pEntries;    // +0x10
	DWORD *m_pNameTable;          // +0x14 read but not used
	FILE *m_pFile;                // +0x18
	DWORD m_dwNamesOffset;        // +0x1C
	DWORD m_dwNamesSize;          // +0x20
	char *m_pNames;               // +0x24 the names, NUL-separated
};
