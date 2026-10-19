// xboxkrnl.exe exports that the XDK headers don't declare. Xbox build only.
#pragma once

#ifdef _XBOX
#include <xtl.h>

extern "C" {

typedef LONG NTSTATUS;

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

NTSTATUS WINAPI NtOpenSymbolicLinkObject(PHANDLE LinkHandle, POBJECT_ATTRIBUTES ObjectAttributes);
NTSTATUS WINAPI NtQuerySymbolicLinkObject(HANDLE LinkHandle, PSTRING LinkTarget, PULONG ReturnedLength);
NTSTATUS WINAPI NtClose(HANDLE Handle);

extern PANSI_STRING XeImageFileName;

}
#endif
