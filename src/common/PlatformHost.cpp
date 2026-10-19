// Host (non-Xbox) stand-ins for the few Win32/XAPI calls portable modules use.
#ifndef _XBOX
#include "Platform.h"

#include <cstdio>

BOOL SetFileAttributesA(LPCSTR, DWORD)
{
	return TRUE;
}

void OutputDebugStringA(LPCSTR text)
{
	fputs(text, stderr);
}
#endif
