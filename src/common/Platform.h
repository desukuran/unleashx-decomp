// Platform glue. On the Xbox (XDK 5933) everything comes from <xtl.h>; on other
// hosts a few stand-ins let portable modules be compiled and syntax-checked.
#pragma once

#ifdef _XBOX
#include <xtl.h>
#else
#include <cstdint>

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef void *HANDLE;
typedef const char *LPCSTR;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define FILE_ATTRIBUTE_NORMAL 0x00000080

BOOL SetFileAttributesA(LPCSTR fileName, DWORD attributes);
void OutputDebugStringA(LPCSTR text);
#endif
