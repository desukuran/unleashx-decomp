// xboxkrnl.exe exports and XAPI functions that the XDK headers don't declare
// (or that UnleashX uses directly). On non-Xbox hosts these are stubbed in
// PlatformHost.cpp so modules can be built and tested.
#pragma once

#include "Platform.h"

#ifndef _XBOX
typedef unsigned short USHORT;
typedef unsigned long ULONG;
typedef char *PCHAR;
typedef HANDLE *PHANDLE;
typedef ULONG *PULONG;
#endif

extern "C" {

typedef LONG NTSTATUS;
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
NTSTATUS __stdcall IoCreateSymbolicLink(PSTRING SymbolicLinkName, PSTRING DeviceName);
NTSTATUS __stdcall IoDeleteSymbolicLink(PSTRING SymbolicLinkName);
void __stdcall RtlInitAnsiString(PANSI_STRING Destination, const char *Source);

extern PANSI_STRING XeImageFileName;

}

// XAPI
BOOL GetDiskFreeSpaceExA(const char *root, ULONGLONG *freeAvailable, ULONGLONG *total, ULONGLONG *totalFree);
BOOL GetVolumeInformationA(const char *root, char *volumeName, DWORD volumeNameSize, DWORD *serial,
                           DWORD *maxComponentLength, DWORD *flags, char *fileSystemName, DWORD fileSystemNameSize);
DWORD XMountUtilityDrive(BOOL formatClean);
