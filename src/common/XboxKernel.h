// xboxkrnl.exe exports and XAPI functions that the XDK headers don't declare
// (or that UnleashX uses directly). On non-Xbox hosts these are stubbed in
// PlatformHost.cpp so modules can be built and tested.
#pragma once

#include "Platform.h"

#ifndef _XBOX
typedef unsigned short USHORT;
typedef char *PCHAR;
typedef HANDLE *PHANDLE;
typedef ULONG *PULONG;
typedef unsigned char UCHAR;
typedef unsigned char BOOLEAN;
typedef union _LARGE_INTEGER
{
	struct
	{
		DWORD LowPart;
		LONG HighPart;
	} u;
	long long QuadPart;
} LARGE_INTEGER;

struct MEMORYSTATUS
{
	DWORD dwLength;
	DWORD dwMemoryLoad;
	DWORD dwTotalPhys;
	DWORD dwAvailPhys;
	DWORD dwTotalPageFile;
	DWORD dwAvailPageFile;
	DWORD dwTotalVirtual;
	DWORD dwAvailVirtual;
};

void GlobalMemoryStatus(MEMORYSTATUS *status);
// Xbox soundtracks (the ones ripped by the MS dashboard).
struct XSOUNDTRACK_DATA
{
	UINT uSoundtrackId;
	UINT uSongCount;
	UINT uSoundtrackLength;
	wchar_t szName[32];
};
HANDLE XFindFirstSoundtrack(XSOUNDTRACK_DATA *data);
BOOL XFindNextSoundtrack(HANDLE find, XSOUNDTRACK_DATA *data);
BOOL XGetSoundtrackSongInfo(DWORD soundtrackId, UINT index, DWORD *songId, DWORD *songLength, wchar_t *name,
                            UINT nameLength);
BOOL XFindClose(HANDLE find);
HANDLE XFindFirstNicknameW(BOOL titleNicknamesOnly, wchar_t *nickname, unsigned size);
// Saved games and downloaded content (XDK).
#define MAX_NAMELEN 64
struct XGAME_FIND_DATA
{
	WIN32_FIND_DATAA wfd;                   // +0x000 the save's folder
	char szSaveGameDirectory[MAX_PATH];     // +0x140
	wchar_t szSaveGameName[MAX_NAMELEN];    // +0x244
};
struct XCONTENT_FIND_DATA
{
	WIN32_FIND_DATAA wfd;                   // +0x000
	DWORD dwTitleId;                        // +0x140
	ULONGLONG qwOfferingId;                 // +0x144
	char szContentDirectory[MAX_PATH];      // +0x14C
	wchar_t szDisplayName[MAX_NAMELEN];     // +0x250
};
HANDLE XFindFirstSaveGame(const char *rootPath, XGAME_FIND_DATA *data);   // 0x000374CB
BOOL XFindNextSaveGame(HANDLE find, XGAME_FIND_DATA *data);              // 0x000375D5
HANDLE XFindFirstContent(const char *rootPath, DWORD offerFilter, XCONTENT_FIND_DATA *data);   // 0x00037DCD
BOOL XFindNextContent(HANDLE find, XCONTENT_FIND_DATA *data);            // 0x00037F1D
// The space a folder takes, in 16 KB blocks.
DWORD XGetDisplayBlocks(const char *path);                                // 0x00036B38
// XGetLanguage values (XDK)
#define XC_LANGUAGE_ENGLISH 1
#define XC_LANGUAGE_JAPANESE 2
#define XC_LANGUAGE_GERMAN 3
#define XC_LANGUAGE_FRENCH 4
#define XC_LANGUAGE_SPANISH 5
#define XC_LANGUAGE_ITALIAN 6
#define XC_LANGUAGE_KOREAN 7
#define XC_LANGUAGE_TCHINESE 8
#define XC_LANGUAGE_PORTUGUESE 9
DWORD XGetLanguage();
// XC_AUDIO_FLAGS: the low word is stereo (0), mono (1) or surround (2);
// 0x10000 Dolby Digital, 0x20000 DTS.
DWORD XGetAudioFlags();                          // 0x000394BD
// XC_PARENTAL_CONTROL_GAMES: 0 all games ... 6 early childhood.
DWORD XGetParentalControlSetting();              // 0x0003955C
// XGetGameRegion: 1 North America, 2 Japan, 4 rest of the world.
DWORD GetVideoRegionSetting();                   // 0x00039507 1 NTSC-M, 2 NTSC-J, 4 PAL

// The kernel's XboxHardwareInfo export.
struct XBOX_HARDWARE_INFO
{
	ULONG Flags;
	BYTE GpuRevision;
	BYTE McpRevision;
	BYTE reserved[2];
};
extern XBOX_HARDWARE_INFO *XboxHardwareInfo;     // import thunk 0x00235730
BOOL XSetNicknameW(const wchar_t *nickname, BOOL preserveCase);
BOOL SystemTimeToFileTime(const SYSTEMTIME *systemTime, FILETIME *fileTime);
BOOL LocalFileTimeToFileTime(const FILETIME *localFileTime, FILETIME *fileTime);
void GetSystemTime(SYSTEMTIME *systemTime);
void GetLocalTime(SYSTEMTIME *systemTime);
DWORD XLaunchNewImageA(LPCSTR imagePath, PLAUNCH_DATA launchData);

// threads, events, memory and the file cache
#define STILL_ACTIVE 0x103
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_HIGHEST 2
#define PAGE_READWRITE 0x04
#define MAXULONG_PTR (~(uintptr_t)0)
DWORD SuspendThread(HANDLE thread);
BOOL SwitchToThread();
HANDLE GetCurrentThread();
int GetThreadPriority(HANDLE thread);
BOOL GetExitCodeThread(HANDLE thread, DWORD *exitCode);
BOOL ResetEvent(HANDLE event);
HANDLE CreateEventA(void *attributes, BOOL manualReset, BOOL initialState, LPCSTR name);
BOOL SetEvent(HANDLE event);
// Host builds only: run threads for real (see PlatformHost.cpp).
extern BOOL g_bHostThreads;
void *XPhysicalAlloc(size_t size, uintptr_t highestAddress, uintptr_t alignment, DWORD protect);
void XPhysicalFree(void *address);
DWORD XGetFileCacheSize();
BOOL XSetFileCacheSize(DWORD size);
DWORD XGetProcessQuantumLength();
void XSetProcessQuantumLength(DWORD quantum);
void SetLastError(DWORD error);
BOOL SetEndOfFile(HANDLE file);
// Deletes the save game name under drive (XDK).
BOOL XDeleteSaveGame(const char *drive, const wchar_t *name);   // 0x000373FB
BOOL CreateDirectoryA(LPCSTR path, void *security);

struct OVERLAPPED
{
	uintptr_t Internal;
	uintptr_t InternalHigh;
	DWORD Offset;
	DWORD OffsetHigh;
	HANDLE hEvent;
};
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_FLAG_NO_BUFFERING 0x20000000
#define MEM_COMMIT 0x00001000
#define MEM_RELEASE 0x00008000
#define MEM_NOZERO 0x00800000
#define PAGE_READWRITE 0x04
#define ERROR_HANDLE_EOF 38
#define ERROR_IO_INCOMPLETE 996
#define ERROR_IO_PENDING 997
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_OUTOFMEMORY 14
#define MOVEFILE_COPY_ALLOWED 0x00000002
#ifndef MOVEFILE_REPLACE_EXISTING
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#endif
#define COPY_FILE_FAIL_IF_EXISTS 0x00000001
#define PROGRESS_CONTINUE 0
#define PROGRESS_CANCEL 1
#define PROGRESS_STOP 2
#define PROGRESS_QUIET 3
#define THREAD_PRIORITY_ABOVE_NORMAL 1
BOOL GetOverlappedResult(HANDLE file, OVERLAPPED *overlapped, DWORD *transferred, BOOL wait);
BOOL GetFileSizeEx(HANDLE file, LARGE_INTEGER *size);
DWORD GetFileSize(HANDLE file, DWORD *high);
BOOL SetFilePointerEx(HANDLE file, LARGE_INTEGER distance, LARGE_INTEGER *newPointer, DWORD moveMethod);   // 0x000356DC
BOOL GetFileTime(HANDLE file, FILETIME *creation, FILETIME *lastAccess, FILETIME *lastWrite);   // 0x0003587D
BOOL SetFileTime(HANDLE file, const FILETIME *creation, const FILETIME *lastAccess, const FILETIME *lastWrite);   // 0x000358E3
HANDLE CreateThread(void *security, DWORD stackSize, DWORD (__stdcall *start)(void *), void *param, DWORD flags,
                    DWORD *threadId);
HANDLE CreateMutexA(void *security, BOOL initialOwner, LPCSTR name);
BOOL ReleaseMutex(HANDLE mutex);
DWORD GetCurrentThreadId();
BOOL MoveFileExA(LPCSTR existingFileName, LPCSTR newFileName, DWORD flags);
BOOL MoveFileWithProgressA(LPCSTR existingFileName, LPCSTR newFileName, void *progress, void *data, DWORD flags);
BOOL CopyFileExA(LPCSTR existingFileName, LPCSTR newFileName, void *progress, void *data, BOOL *cancel,
                 DWORD flags);
LONG __stdcall NtAllocateVirtualMemory(void **baseAddress, uintptr_t zeroBits, DWORD *regionSize,
                                       DWORD allocationType, DWORD protect);
LONG __stdcall NtFreeVirtualMemory(void **baseAddress, DWORD *regionSize, DWORD freeType);
int _mbsicmp(const char *a, const char *b);
int _stricmp(const char *a, const char *b);
char *_strdup(const char *s);
LONGLONG _atoi64(const char *s);

struct EXCEPTION_RECORD
{
	DWORD ExceptionCode;
	DWORD ExceptionFlags;
	EXCEPTION_RECORD *ExceptionRecord;
	void *ExceptionAddress;
	DWORD NumberParameters;
	ULONG ExceptionInformation[15];
};
struct EXCEPTION_POINTERS
{
	EXCEPTION_RECORD *ExceptionRecord;
	void *ContextRecord;
};
typedef LONG(__stdcall *LPTOP_LEVEL_EXCEPTION_FILTER)(EXCEPTION_POINTERS *exception);
LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter);
#endif

extern "C" {

typedef LONG NTSTATUS;

typedef struct _IO_STATUS_BLOCK
{
	NTSTATUS Status;
	ULONG Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
#define NT_SUCCESS(status) ((NTSTATUS)(status) >= 0)

typedef struct _ANSI_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
} ANSI_STRING, *PANSI_STRING, STRING, *PSTRING;

typedef struct _OBJECT_ATTRIBUTES
{
	HANDLE RootDirectory;
	PANSI_STRING ObjectName;
	ULONG Attributes;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

#define OBJ_CASE_INSENSITIVE 0x00000040

NTSTATUS __stdcall NtOpenSymbolicLinkObject(PHANDLE LinkHandle, POBJECT_ATTRIBUTES ObjectAttributes);
NTSTATUS __stdcall NtQuerySymbolicLinkObject(HANDLE LinkHandle, PSTRING LinkTarget, PULONG ReturnedLength);
NTSTATUS __stdcall NtClose(HANDLE Handle);
NTSTATUS __stdcall NtCreateFile(HANDLE *FileHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
                                PIO_STATUS_BLOCK IoStatusBlock, LARGE_INTEGER *AllocationSize, ULONG FileAttributes,
                                ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions);
NTSTATUS __stdcall IoCreateSymbolicLink(PSTRING SymbolicLinkName, PSTRING DeviceName);
NTSTATUS __stdcall IoDeleteSymbolicLink(PSTRING SymbolicLinkName);
void __stdcall RtlInitAnsiString(PANSI_STRING Destination, const char *Source);

extern PANSI_STRING XeImageFileName;
extern STRING HalDiskSerialNumber;
typedef struct _XBOX_KRNL_VERSION
{
	USHORT Major;
	USHORT Minor;
	USHORT Build;
	USHORT Qfe;
} XBOX_KRNL_VERSION;
extern XBOX_KRNL_VERSION XboxKrnlVersion;
extern BYTE XboxHDKey[16];
void __stdcall KeQuerySystemTime(LARGE_INTEGER *currentTime);
NTSTATUS __stdcall HalReadSMBusValue(UCHAR address, UCHAR command, BOOLEAN readWord, ULONG *value);
NTSTATUS __stdcall HalWriteSMBusValue(UCHAR address, UCHAR command, BOOLEAN writeWord, ULONG value);
NTSTATUS __stdcall NtSetSystemTime(LARGE_INTEGER *newTime, LARGE_INTEGER *previousTime);
NTSTATUS __stdcall ExQueryNonVolatileSetting(ULONG valueIndex, ULONG *type, void *value, ULONG valueLength,
                                            ULONG *resultLength);
NTSTATUS __stdcall ExSaveNonVolatileSetting(ULONG valueIndex, ULONG type, void *value, ULONG valueLength);
typedef enum _FIRMWARE_REENTRY
{
	ReturnFirmwareHalt,
	ReturnFirmwareReboot,
	ReturnFirmwareQuickReboot,
	ReturnFirmwareHard,
	ReturnFirmwareFatal,
	ReturnFirmwareAll
} FIRMWARE_REENTRY;
void __stdcall HalReturnToFirmware(FIRMWARE_REENTRY routine);

}

// IDE pass-through (not in the XDK headers)
typedef struct _IDEREGS
{
	UCHAR bFeaturesReg;
	UCHAR bSectorCountReg;
	UCHAR bSectorNumberReg;
	UCHAR bCylLowReg;
	UCHAR bCylHighReg;
	UCHAR bDriveHeadReg;
	UCHAR bCommandReg;   // status on return
	UCHAR bHostSendsData;
} IDEREGS;

typedef struct _ATA_PASS_THROUGH
{
	IDEREGS IdeReg;
	ULONG DataBufferSize;
	void *DataBuffer;
} ATA_PASS_THROUGH;

#define IOCTL_IDE_PASS_THROUGH 0x0004D028

#ifndef _XBOX
#ifndef GENERIC_READ
#define GENERIC_READ 0x80000000
#endif
#define SYNCHRONIZE 0x00100000
#ifndef FILE_SHARE_READ
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#endif
#define FILE_OPEN 0x00000001
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020
BOOL DeviceIoControl(HANDLE device, DWORD code, void *in, DWORD inSize, void *out, DWORD outSize, DWORD *returned,
                     void *overlapped);
#endif

// XAPI
BOOL GetDiskFreeSpaceExA(const char *root, ULONGLONG *freeAvailable, ULONGLONG *total, ULONGLONG *totalFree);
BOOL GetVolumeInformationA(const char *root, char *volumeName, DWORD volumeNameSize, DWORD *serial,
                           DWORD *maxComponentLength, DWORD *flags, char *fileSystemName, DWORD fileSystemNameSize);
DWORD XMountUtilityDrive(BOOL formatClean);
