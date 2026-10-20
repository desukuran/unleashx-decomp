// The third-party sources include <windows.h>; on the Xbox that's <xtl.h>,
// plus the few Win32 calls XAPI doesn't have that zip_utils uses.
#pragma once
#include <xtl.h>

// zip.cpp seeds its encryption salt with this; there are no windows.
static void *GetDesktopWindow()
{
	return 0;
}

// No file mappings: zipping into memory fails (ZR_NOALLOC). UnleashX only
// zips to files.
#define FILE_MAP_ALL_ACCESS 0xF001F
static HANDLE CreateFileMapping(HANDLE, void *, DWORD, DWORD, DWORD, const char *)
{
	return NULL;
}
static void *MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, DWORD)
{
	return NULL;
}
static BOOL UnmapViewOfFile(const void *)
{
	return FALSE;
}
