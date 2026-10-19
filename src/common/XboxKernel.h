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
HANDLE XFindFirstNicknameW(BOOL titleNicknamesOnly, wchar_t *nickname, unsigned size);
DWORD XGetLanguage();
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
BOOL CreateDirectoryA(LPCSTR path, void *security);
int _mbsicmp(const char *a, const char *b);
int _stricmp(const char *a, const char *b);
char *_strdup(const char *s);

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
