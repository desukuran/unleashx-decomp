// Platform glue. On the Xbox (XDK 5933) everything comes from <xtl.h>; on other
// hosts a few stand-ins let portable modules be compiled and tested. Only what
// the recovered code uses is declared here.
#pragma once

#ifdef _XBOX
#include <xtl.h>
#else
#include <arpa/inet.h>  // inet_addr, INADDR_NONE (XNet on the Xbox)
#include <cstddef>
#include <cstdint>
#include <cwchar>

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef uint32_t ULONG;
typedef unsigned int UINT;
typedef uint64_t ULONGLONG;
typedef int64_t LONGLONG;
typedef int32_t LONG;
typedef int32_t HRESULT;
typedef void *HANDLE;
typedef const char *LPCSTR;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define E_FAIL ((HRESULT)0x80004005)
#define E_INVALIDARG ((HRESULT)0x80070057)

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

#define CP_ACP 0
#define CP_UTF8 65001

struct FILETIME
{
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
};

struct SYSTEMTIME
{
	WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
};

struct WIN32_FILE_ATTRIBUTE_DATA
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
};
enum GET_FILEEX_INFO_LEVELS { GetFileExInfoStandard };

struct LAUNCH_DATA
{
	BYTE Data[3072];
};
typedef LAUNCH_DATA *PLAUNCH_DATA;

#define XC_VIDEO_FLAGS_WIDESCREEN 0x00000001
#define XC_VIDEO_FLAGS_HDTV_720p 0x00000002
#define XC_VIDEO_FLAGS_HDTV_1080i 0x00000004
#define XC_VIDEO_FLAGS_HDTV_480p 0x00000008
#define XC_VIDEO_FLAGS_LETTERBOX 0x00000010
#define XC_VIDEO_FLAGS_PAL_60Hz 0x00000040

#define XC_VIDEO_STANDARD_NTSC_M 1
#define XC_VIDEO_STANDARD_NTSC_J 2
#define XC_VIDEO_STANDARD_PAL_I 3

#define XC_AV_PACK_SCART 0
#define XC_AV_PACK_HDTV 1
#define XC_AV_PACK_VGA 2
#define XC_AV_PACK_RFU 3
#define XC_AV_PACK_SVIDEO 4
#define XC_AV_PACK_STANDARD 6

BOOL SetFileAttributesA(LPCSTR fileName, DWORD attributes);
DWORD GetFileAttributesA(LPCSTR fileName);
BOOL GetFileAttributesExA(LPCSTR fileName, GET_FILEEX_INFO_LEVELS level, void *info);
BOOL FileTimeToSystemTime(const FILETIME *fileTime, SYSTEMTIME *systemTime);
BOOL FileTimeToLocalFileTime(const FILETIME *fileTime, FILETIME *localFileTime);
char *_fcvt(double value, int count, int *decimalPos, int *negative);
void OutputDebugStringA(LPCSTR text);
void Sleep(DWORD ms);
int MultiByteToWideChar(unsigned codePage, DWORD flags, const char *src, int srcLen, wchar_t *dst, int dstLen);
#define INFINITE 0xFFFFFFFF
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0
#endif
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#ifndef __stdcall
#define __stdcall
#endif

struct WIN32_FIND_DATAA
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	char cFileName[260];
	char cAlternateFileName[14];
};

HANDLE FindFirstFileA(LPCSTR pattern, WIN32_FIND_DATAA *data);
BOOL FindNextFileA(HANDLE find, WIN32_FIND_DATAA *data);
void *XLoadSection(LPCSTR name);
BOOL XFreeSection(LPCSTR name);
HANDLE XGetSectionHandle(LPCSTR name);
DWORD XGetSectionSize(HANDLE section);
// A save game folder (pathBuffer gets its path). Returns ERROR_SUCCESS (0).
DWORD XCreateSaveGame(LPCSTR rootPath, const wchar_t *name, DWORD creationDisposition, DWORD createFlags,
                      char *pathBuffer, UINT bufferSize);
#define XCREATESAVEGAME_NOCOPY 1
BOOL SetThreadPriority(HANDLE thread, int priority);
uintptr_t _beginthreadex(void *security, unsigned stackSize, unsigned (__stdcall *start)(void *), void *arg,
                         unsigned flags, unsigned *threadId);
int stricmp(const char *a, const char *b);
int _wcsicmp(const wchar_t *a, const wchar_t *b);
int _wcsnicmp(const wchar_t *a, const wchar_t *b, size_t count);
char *_strdup(const char *s);
int _wtoi(const wchar_t *s);
long _wtol(const wchar_t *s);
double _wtof(const wchar_t *s);
int WideCharToMultiByte(UINT codePage, DWORD flags, const wchar_t *wide, int wideLength, char *multiByte,
                        int multiByteLength, const char *defaultChar, BOOL *usedDefaultChar);   // 0x000360AE
DWORD WaitForSingleObject(HANDLE handle, DWORD ms);
BOOL CloseHandle(HANDLE handle);
DWORD GetTickCount();
BOOL DeleteFileA(LPCSTR fileName);
BOOL CopyFileA(LPCSTR existingFileName, LPCSTR newFileName, BOOL failIfExists);
BOOL MoveFileA(LPCSTR existingFileName, LPCSTR newFileName);
#ifndef GENERIC_READ
#define GENERIC_READ 0x80000000
#endif
#define GENERIC_WRITE 0x40000000
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define ERROR_DIR_NOT_EMPTY 145
#ifndef FILE_SHARE_READ
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#endif
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
HANDLE CreateFileA(LPCSTR fileName, DWORD access, DWORD shareMode, void *security, DWORD creationDisposition,
                   DWORD flags, HANDLE templateFile);
BOOL ReadFile(HANDLE file, void *buffer, DWORD size, DWORD *read, void *overlapped);
DWORD SetFilePointer(HANDLE file, LONG distance, LONG *distanceHigh, DWORD moveMethod);
BOOL RemoveDirectoryA(LPCSTR path);
DWORD GetLastError();
BOOL WriteFile(HANDLE file, const void *buffer, DWORD size, DWORD *written, void *overlapped);
struct BY_HANDLE_FILE_INFORMATION
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD dwVolumeSerialNumber;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD nNumberOfLinks;
	DWORD nFileIndexHigh;
	DWORD nFileIndexLow;
};
BOOL GetFileInformationByHandle(HANDLE file, BY_HANDLE_FILE_INFORMATION *info);
#define CREATE_SUSPENDED 0x00000004
DWORD ResumeThread(HANDLE thread);
void _endthreadex(unsigned int code);
DWORD XGetLaunchInfo(DWORD *launchDataType, PLAUNCH_DATA launchData);
DWORD XGetVideoFlags();
DWORD XGetVideoStandard();
DWORD XGetAVPack();

// fopen() with "X:\path" mapped to the host filesystem (see PlatformHost.cpp).
#include <cstdio>
FILE *HostFopen(const char *path, const char *mode);
#ifndef UNLEASHX_PLATFORM_HOST_IMPL
#define fopen HostFopen
#endif
#endif

// Where the BIOS flash is mapped (the top 16 MB of the address space). NULL
// on hosts, where there's nothing to read.
#ifdef _XBOX
#define BIOS_FLASH_BASE ((const void *)0xFF000000)
#else
#define BIOS_FLASH_BASE ((const void *)0)
#endif
