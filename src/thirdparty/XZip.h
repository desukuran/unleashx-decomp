// Lucian Wischik's XZip (zip_utils), the writing half of XUnzip.h,
// statically linked into UnleashX. Third-party library: only the interface
// is reconstructed here.
#pragma once

#include "XUnzip.h"

// Creates a zip (flags ZIP_FILENAME: z is the path).
HZIP CreateZip(void *z, unsigned int len, DWORD flags);                        // 0x00088BB1
// Adds src (a path with ZIP_FILENAME) as dstName. (hz in eax, flags in ecx)
ZRESULT ZipAdd(HZIP hz, const char *dstName, void *src, unsigned int len, DWORD flags);   // 0x0008AFC2
// XUnzip's handles say 1, XZip's 2. (hz in eax) Also TRUE for NULL.
bool IsZipHandleU(HZIP hz);                                                    // 0x0008B06B
ZRESULT CloseZipZ(HZIP hz);                                                    // 0x00089F02 (hz in edi)
// CloseZip for either kind (the zip_utils headers' combined CloseZip,
// inlined at the callers).
inline ZRESULT CloseAnyZip(HZIP hz) { return IsZipHandleU(hz) ? CloseZip(hz) : CloseZipZ(hz); }
