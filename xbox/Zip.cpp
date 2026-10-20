// The zip libraries for the Xbox build: Lucian Wischik's unzip and Valve's
// XZip (zip creation), compiled from reference/thirdparty/zip_utils (see
// Makefile.xbox), behind the interface UnleashX was linked against (XUnzip
// and XZip with the ZIP_HANDLE/ZIP_FILENAME/ZIP_MEMORY flags,
// src/thirdparty/XZip.h).
#include "../src/thirdparty/XZip.h"

// unzip.cpp
HZIP OpenZipInternal(void *z, unsigned int len, DWORD flags, const char *password);
ZRESULT UnzipItemInternal(HZIP hz, int index, void *dst, unsigned int len, DWORD flags);
ZRESULT CloseZipU(HZIP hz);
// XZip.cpp
HZIP CreateZipZ(void *z, unsigned int len, DWORD flags);

HZIP OpenZip(void *z, unsigned int len, DWORD flags)
{
	return OpenZipInternal(z, len, flags, NULL);
}

ZRESULT UnzipItem(HZIP hz, int index, void *dst, unsigned int len, DWORD flags)
{
	return UnzipItemInternal(hz, index, dst, len, flags);
}

ZRESULT CloseZip(HZIP hz)
{
	return CloseZipU(hz);
}

HZIP CreateZip(void *z, unsigned int len, DWORD flags)
{
	return CreateZipZ(z, len, flags);
}
