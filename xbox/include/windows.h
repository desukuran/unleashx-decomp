// The third-party sources include <windows.h>; on the Xbox that's <xtl.h>,
// plus the few Win32 calls XAPI doesn't have.
#pragma once
#include <xtl.h>

// Everything the zip code opens is a seekable file.
#define FILE_TYPE_DISK 1
static DWORD GetFileType(HANDLE)
{
	return FILE_TYPE_DISK;
}

static BOOL FileTimeToDosDateTime(const FILETIME *fileTime, WORD *dosDate, WORD *dosTime)
{
	SYSTEMTIME st;
	if (!FileTimeToSystemTime(fileTime, &st))
		return FALSE;
	*dosDate = (WORD)(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);
	*dosTime = (WORD)((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
	return TRUE;
}

// No file mappings: zipping into memory fails (ZR_NOALLOC); UnleashX zips
// to files.
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
