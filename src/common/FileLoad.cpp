// XBUtil_LoadFile, the XDK samples' whole-file loader (XBUtil.cpp; Xbox
// only: unbuffered, overlapped reads). Library code, but kept as source
// since the WMA player and CXMVPlayer::OpenMovieFromMemory use it. See
// FileUtil.h.
#include "FileUtil.h"

#include <cstdlib>
#include <cstring>

#include "XboxKernel.h"

#ifndef FILE_FLAG_NO_BUFFERING
#define FILE_FLAG_NO_BUFFERING 0x20000000
#define FILE_FLAG_OVERLAPPED 0x40000000
#endif
#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING 997
#endif

// 0x00092B01
HRESULT XBUtil_LoadFile(const char *path, void **data, DWORD *size)
{
	if (path == NULL || data == NULL)
		return E_INVALIDARG;
	*data = NULL;
	if (size != NULL)
		*size = 0;
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	                          FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return E_FAIL;
	DWORD fileSize = GetFileSize(file, NULL);
	HRESULT hr;
	if (fileSize == (DWORD)-1)
	{
		hr = E_FAIL;
	}
	else
	{
		// Unbuffered reads go in whole sectors.
		DWORD bufferSize = (fileSize + 0x7FF) & ~0x7FF;
		void *buffer = malloc(bufferSize);
		if (buffer == NULL)
		{
			hr = E_OUTOFMEMORY;
		}
		else
		{
			OVERLAPPED overlapped;
			memset(&overlapped, 0, sizeof(overlapped));
			DWORD read;
			if ((ReadFile(file, buffer, bufferSize, NULL, &overlapped) || GetLastError() == ERROR_IO_PENDING) &&
			    GetOverlappedResult(file, &overlapped, &read, TRUE))
			{
				*data = buffer;
				if (size != NULL)
					*size = read;
				CloseHandle(file);
				return S_OK;
			}
			free(buffer);
			CloseHandle(file);
			return (HRESULT)GetLastError();
		}
	}
	CloseHandle(file);
	return hr;
}
