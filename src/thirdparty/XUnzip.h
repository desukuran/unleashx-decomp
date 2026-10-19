// Lucian Wischik's XUnzip (zip_utils), statically linked into UnleashX.
// Third-party library: only the interface is reconstructed here.
#pragma once

#include "../common/Platform.h"

typedef struct HZIP__ *HZIP;
typedef DWORD ZRESULT;

#define ZIP_HANDLE 1
#define ZIP_FILENAME 2
#define ZIP_MEMORY 3

#define ZR_OK 0

struct ZIPENTRY   // 0x12C bytes
{
	int index;              // GetZipItem(-1): number of entries
	char name[260];
	DWORD attr;
	FILETIME atime, ctime, mtime;
	long comp_size;
	long unc_size;
};

HZIP OpenZip(void *z, unsigned int len, DWORD flags);                          // 0x0008E753
ZRESULT GetZipItem(HZIP hz, int index, ZIPENTRY *ze);                          // 0x0008E725
// Finds name (ic: ignoring case); *index is -1 when it isn't there.
ZRESULT FindZipItem(HZIP hz, const char *name, bool ic, int *index, ZIPENTRY *ze);   // 0x0008EBD8
ZRESULT UnzipItem(HZIP hz, int index, void *dst, unsigned int len, DWORD flags);   // 0x0008EAE0
ZRESULT CloseZip(HZIP hz);                                                     // 0x0008E5F2
