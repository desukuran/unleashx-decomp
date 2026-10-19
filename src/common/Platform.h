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
typedef int32_t LONG;
typedef int32_t HRESULT;
typedef void *HANDLE;
typedef const char *LPCSTR;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define S_OK ((HRESULT)0)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define E_FAIL ((HRESULT)0x80004005)

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

BOOL SetFileAttributesA(LPCSTR fileName, DWORD attributes);
DWORD GetFileAttributesA(LPCSTR fileName);
BOOL GetFileAttributesExA(LPCSTR fileName, GET_FILEEX_INFO_LEVELS level, void *info);
BOOL FileTimeToSystemTime(const FILETIME *fileTime, SYSTEMTIME *systemTime);
void OutputDebugStringA(LPCSTR text);
void Sleep(DWORD ms);
int MultiByteToWideChar(unsigned codePage, DWORD flags, const char *src, int srcLen, wchar_t *dst, int dstLen);
#define INFINITE 0xFFFFFFFF
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
BOOL SetThreadPriority(HANDLE thread, int priority);
uintptr_t _beginthreadex(void *security, unsigned stackSize, unsigned (__stdcall *start)(void *), void *arg,
                         unsigned flags, unsigned *threadId);
int stricmp(const char *a, const char *b);
DWORD WaitForSingleObject(HANDLE handle, DWORD ms);
BOOL CloseHandle(HANDLE handle);
DWORD GetTickCount();
BOOL DeleteFileA(LPCSTR fileName);
BOOL CopyFileA(LPCSTR existingFileName, LPCSTR newFileName, BOOL failIfExists);
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
