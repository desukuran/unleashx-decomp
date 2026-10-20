// Path and file helpers from the MS dashboard's code (0x0007F357-0x0007FAF3),
// linked in with the audio CD and WMA streaming classes. Most of them are
// unused here. See DashUtil.cpp.
#pragma once

#include "../common/Platform.h"

// Copies at most count characters (the NUL included, so a long source
// isn't terminated). (dst in ecx, src in edx)
void DashStrCopyN(char *dst, const char *src, int count);          // 0x0007F367 (thunk 0x0007F357)
// "file:" dropped, '/' to '\', cut at '?'. (url in eax, path in ecx)
void DashUrlToPath(const char *url, char *path);                    // 0x0007F946
// CreateFileA on DashUrlToPath(path).
HANDLE DashCreateFile(const char *path, DWORD access, DWORD share, void *security, DWORD disposition,
                      DWORD attributes, HANDLE templateFile);       // 0x00032894
