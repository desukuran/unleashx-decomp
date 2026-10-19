// Host (non-Xbox) stand-ins for the Win32/XAPI calls portable modules use, so
// they can be compiled and tested on a PC. Not part of the dashboard.
#ifndef _XBOX
#define UNLEASHX_PLATFORM_HOST_IMPL
#include "Platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Maps "X:\dir\file" to a host path: drive letters become directories under
// $UNLEASHX_ROOT (default "."), backslashes become slashes.
static std::string HostPath(LPCSTR path)
{
	std::string p(path);
	const char *root = getenv("UNLEASHX_ROOT");
	std::string out = root != NULL ? root : ".";
	if (p.size() >= 2 && p[1] == ':')
	{
		out += "/";
		out += (char)toupper(p[0]);
		p = p.substr(2);
	}
	else
	{
		out.clear();
	}
	for (size_t i = 0; i < p.size(); i++)
		if (p[i] == '\\')
			p[i] = '/';
	return out + p;
}

FILE *HostFopen(const char *path, const char *mode)
{
	return fopen(HostPath(path).c_str(), mode);
}

BOOL SetFileAttributesA(LPCSTR, DWORD)
{
	return TRUE;
}

DWORD GetFileAttributesA(LPCSTR fileName)
{
	struct stat st;
	if (stat(HostPath(fileName).c_str(), &st) != 0)
		return INVALID_FILE_ATTRIBUTES;
	return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

BOOL GetFileAttributesExA(LPCSTR fileName, GET_FILEEX_INFO_LEVELS, void *info)
{
	struct stat st;
	if (stat(HostPath(fileName).c_str(), &st) != 0)
		return FALSE;
	WIN32_FILE_ATTRIBUTE_DATA *data = (WIN32_FILE_ATTRIBUTE_DATA *)info;
	memset(data, 0, sizeof(*data));
	// FILETIME: 100ns units since 1601.
	unsigned long long t = ((unsigned long long)st.st_mtime + 11644473600ull) * 10000000ull;
	data->ftLastWriteTime.dwLowDateTime = (DWORD)t;
	data->ftLastWriteTime.dwHighDateTime = (DWORD)(t >> 32);
	return TRUE;
}

BOOL FileTimeToSystemTime(const FILETIME *ft, SYSTEMTIME *st)
{
	unsigned long long t = ((unsigned long long)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	time_t secs = (time_t)(t / 10000000ull - 11644473600ull);
	struct tm tm;
	gmtime_r(&secs, &tm);
	st->wYear = (WORD)(tm.tm_year + 1900);
	st->wMonth = (WORD)(tm.tm_mon + 1);
	st->wDayOfWeek = (WORD)tm.tm_wday;
	st->wDay = (WORD)tm.tm_mday;
	st->wHour = (WORD)tm.tm_hour;
	st->wMinute = (WORD)tm.tm_min;
	st->wSecond = (WORD)tm.tm_sec;
	st->wMilliseconds = 0;
	return TRUE;
}

BOOL DeleteFileA(LPCSTR fileName)
{
	return unlink(HostPath(fileName).c_str()) == 0;
}

BOOL CopyFileA(LPCSTR, LPCSTR, BOOL)
{
	return FALSE;
}

void OutputDebugStringA(LPCSTR text)
{
	fputs(text, stderr);
}

void Sleep(DWORD)
{
}

DWORD WaitForSingleObject(HANDLE, DWORD)
{
	return 0;
}

BOOL CloseHandle(HANDLE)
{
	return TRUE;
}

DWORD GetTickCount()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int _wcsicmp(const wchar_t *a, const wchar_t *b)
{
	return wcscasecmp(a, b);
}

// No threads on the host: the start routine isn't run.
uintptr_t _beginthreadex(void *, unsigned, unsigned(__stdcall *)(void *), void *, unsigned, unsigned *)
{
	return 0;
}

BOOL SetThreadPriority(HANDLE, int)
{
	return TRUE;
}

void _endthreadex(unsigned int)
{
}

int MultiByteToWideChar(unsigned, DWORD, const char *src, int srcLen, wchar_t *dst, int dstLen)
{
	int n = 0;
	for (; n < srcLen && n < dstLen; n++)
		dst[n] = (unsigned char)src[n];
	return n;
}

DWORD XGetLaunchInfo(DWORD *type, PLAUNCH_DATA data)
{
	*type = 0;
	memset(data, 0, sizeof(*data));
	return 0x80004005;
}

DWORD XGetVideoFlags()
{
	return 0;
}

DWORD XGetVideoStandard()
{
	return XC_VIDEO_STANDARD_NTSC_M;
}

DWORD XGetAVPack()
{
	return 0;
}
#endif

#ifndef _XBOX
#include "XboxKernel.h"

extern "C" {
PANSI_STRING XeImageFileName = NULL;
NTSTATUS __stdcall NtOpenSymbolicLinkObject(PHANDLE handle, POBJECT_ATTRIBUTES)
{
	*handle = NULL;
	return (NTSTATUS)0xC0000034;   // STATUS_OBJECT_NAME_NOT_FOUND
}
NTSTATUS __stdcall NtQuerySymbolicLinkObject(HANDLE, PSTRING, PULONG)
{
	return (NTSTATUS)0xC0000001;
}
STRING HalDiskSerialNumber = {9, 10, (char *)"HOSTDISK1"};
BYTE XboxHDKey[16];

void __stdcall KeQuerySystemTime(LARGE_INTEGER *currentTime)
{
	// 100 ns units since 1601
	currentTime->QuadPart = ((long long)time(NULL) + 11644473600LL) * 10000000LL;
}

NTSTATUS __stdcall HalReadSMBusValue(UCHAR, UCHAR, BOOLEAN, ULONG *value)
{
	*value = 0;
	return 0;
}

NTSTATUS __stdcall NtClose(HANDLE)
{
	return 0;
}
NTSTATUS __stdcall IoCreateSymbolicLink(PSTRING, PSTRING)
{
	return 0;
}
NTSTATUS __stdcall IoDeleteSymbolicLink(PSTRING)
{
	return 0;
}
void __stdcall RtlInitAnsiString(PANSI_STRING dst, const char *src)
{
	dst->Buffer = (PCHAR)src;
	dst->Length = (USHORT)(src != NULL ? strlen(src) : 0);
	dst->MaximumLength = (USHORT)(dst->Length + 1);
}
}

void GlobalMemoryStatus(MEMORYSTATUS *status)
{
	memset(status, 0, sizeof(*status));
	status->dwLength = sizeof(*status);
	status->dwTotalPhys = 64 << 20;
	status->dwAvailPhys = 40 << 20;
}

HANDLE XFindFirstNicknameW(BOOL, wchar_t *nickname, unsigned)
{
	nickname[0] = 0;
	return INVALID_HANDLE_VALUE;
}

DWORD XGetLanguage()
{
	return 1;   // XC_LANGUAGE_ENGLISH
}

BOOL GetDiskFreeSpaceExA(const char *, ULONGLONG *freeAvailable, ULONGLONG *total, ULONGLONG *totalFree)
{
	*freeAvailable = *total = *totalFree = 0;
	return FALSE;
}

BOOL GetVolumeInformationA(const char *, char *, DWORD, DWORD *, DWORD *, DWORD *, char *, DWORD)
{
	return FALSE;
}

DWORD XMountUtilityDrive(BOOL)
{
	return 0;
}
#endif
