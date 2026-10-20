// UnleashX's zip interface (src/thirdparty/XZip.h: XUnzip/XZip with the
// ZIP_HANDLE/ZIP_FILENAME/ZIP_MEMORY flags) on Lucian Wischik's zip_utils
// (thirdparty/zip_utils), which keep that flag interface internally.
// GetZipItem, FindZipItem, IsZipHandleU and CloseZipZ come straight from
// unzip.cpp and zip.cpp.
#include "../src/thirdparty/XZip.h"

// unzip.cpp
HZIP OpenZipInternal(void *z, unsigned int len, DWORD flags, const char *password);
ZRESULT UnzipItemInternal(HZIP hz, int index, void *dst, unsigned int len, DWORD flags);
ZRESULT CloseZipU(HZIP hz);
// zip.cpp
HZIP CreateZipInternal(void *z, unsigned int len, DWORD flags, const char *password);
ZRESULT ZipAddInternal(HZIP hz, const char *dstzn, void *src, unsigned int len, DWORD flags);

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
	return CreateZipInternal(z, len, flags, NULL);
}

ZRESULT ZipAdd(HZIP hz, const char *dstName, void *src, unsigned int len, DWORD flags)
{
	return ZipAddInternal(hz, dstName, src, len, flags);
}
