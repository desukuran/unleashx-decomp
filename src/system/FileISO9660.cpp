// CFileISO9660 and CDirISO9660: DVD data discs, through XBMC's iso9660
// reader. See FileIO.h.
#include <cstring>

#include "../thirdparty/Iso9660.h"
#include "FileIO.h"

// The device the iso9660 reader opened (see Iso9660.h).
HANDLE g_hIso9660Device;   // 0x0027625C

// ---- CFileISO9660 ----

// 0x00080BB3 (this in eax)
CFileISO9660::CFileISO9660()
{
	m_pIso = NULL;
	m_hFile = INVALID_HANDLE_VALUE;
	m_pBuffer = NULL;
}

// 0x00080B8B
CFileISO9660::~CFileISO9660()
{
	Close();
}

// 0x00080B74 (this in ecx)
BOOL CFileISO9660::GetIso()
{
	if (m_pIso == NULL)
		m_pIso = GetDvdIso9660();
	return m_pIso != NULL;
}

// 0x00080B42
void CFileISO9660::Close()
{
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		// (Only while the disc is still there.)
		if (GetDvdIso9660() != NULL)
			m_pIso->CloseFile(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
	if (m_pBuffer != NULL)
	{
		delete[] m_pBuffer;
		m_pBuffer = NULL;
	}
}

// 0x00080BFC
BOOL CFileISO9660::OpenRead(const char *path, DWORD bufferSize, BOOL /*overlapped*/)
{
	if (!GetIso())
		return FALSE;
	Close();
	m_hFile = m_pIso->OpenFile(path);
	if (m_hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	m_bEOF = FALSE;
	m_llUnknown20 = 0;
	m_dwLastRead = 0;
	m_llSize = m_pIso->GetFileSize(m_hFile);
	if (m_llSize == 0)
	{
		// Quirk: an empty file fails to open.
		m_bEOF = TRUE;
		return FALSE;
	}
	m_dwBufferSize = bufferSize != 0 ? bufferSize : 0x8000;
	m_pBuffer = new BYTE[m_dwBufferSize];
	if (m_pBuffer != NULL)
		return TRUE;
	Close();
	return FALSE;
}

// 0x00080BC6
DWORD CFileISO9660::Read(void *buffer, DWORD size)
{
	if (m_hFile == INVALID_HANDLE_VALUE || buffer == NULL)
		return 0;
	long read = m_pIso->ReadFile(m_hFile, (BYTE *)buffer, (long)size);
	m_dwLastRead = (DWORD)read;
	if (read > 0)
		return (DWORD)read;
	m_bEOF = TRUE;
	return 0;
}

// 0x00080AD9
HRESULT CFileISO9660::ReadNext(DWORD size)
{
	DWORD length = m_dwBufferSize;
	if (size != 0 && size < length)
		length = size;
	return Read(m_pBuffer, length) != 0 ? S_OK : E_FAIL;
}

// 0x00080B06
BOOL CFileISO9660::WaitRead(BOOL /*wait*/, DWORD *bytes)
{
	if (bytes != NULL)
		*bytes = m_dwLastRead;
	return TRUE;
}

// 0x00080B19
void CFileISO9660::Seek(LONG distance, LONG *distanceHigh, DWORD method)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return;
	LONG high = distanceHigh != NULL ? *distanceHigh : 0;
	m_pIso->Seek(m_hFile, ((LONGLONG)high << 32) | (DWORD)distance, (int)method);
}

// 0x000327F1
BYTE *CFileISO9660::GetBuffer(BOOL /*first*/, DWORD *size)
{
	if (size != NULL)
		*size = m_dwBufferSize;
	return m_pBuffer;
}

// ---- CDirISO9660 ----

// 0x00081948 (this in esi)
CDirISO9660::~CDirISO9660()
{
	if (m_pIso != NULL)
	{
		delete m_pIso;
		m_pIso = NULL;
	}
}

// 0x00081971 (this in esi)
BOOL CDirISO9660::CreateIso()
{
	if (m_pIso == NULL)
	{
		m_pIso = new iso9660;
		if (m_pIso == NULL)
			return FALSE;
	}
	return TRUE;
}

// 0x00081D1B
HANDLE CDirISO9660::FindFirst(const char *pattern, WIN32_FIND_DATAA *data)
{
	if (!CreateIso())
		return INVALID_HANDLE_VALUE;
	return m_pIso->FindFirstFile(pattern, data);
}

// 0x00081927
BOOL CDirISO9660::FindNext(HANDLE find, WIN32_FIND_DATAA *data)
{
	return m_pIso->FindNextFile(find, data);
}

// 0x00081938
BOOL CDirISO9660::FindClose(HANDLE find)
{
	return m_pIso->FindClose(find);
}

// 0x000818DF
BOOL CDirISO9660::FileExists(const char *path)
{
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirst(path, &data);
	if (find == INVALID_HANDLE_VALUE)
		return FALSE;
	BOOL file = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	FindClose(find);
	return file;
}

// 0x000818A1
BOOL CDirISO9660::DirectoryExists(const char *path)
{
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirst(path, &data);
	if (find == INVALID_HANDLE_VALUE)
		return FALSE;
	BOOL folder = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	FindClose(find);
	return folder;
}

// (inlined in CopyFile and CopyFileEx)
BOOL CDirISO9660::CopyFromDisc(const char *source, const char *destination, DWORD creation,
                               CopyProgressRoutine progress)
{
	HANDLE out = CreateFileA(destination, GENERIC_WRITE, 0, NULL, creation, 0, NULL);
	if (out == NULL || out == INVALID_HANDLE_VALUE || !CreateIso())
		return FALSE;   // (out stays open)
	iso9660 *iso = m_pIso;
	HANDLE in = iso->OpenFile(source);
	if (in == INVALID_HANDLE_VALUE)
		return FALSE;   // (out stays open)
	BYTE *buffer = new BYTE[0x8000];
	LARGE_INTEGER size;
	size.QuadPart = iso->GetFileSize(in);
	LARGE_INTEGER transferred, streamSize, streamTransferred;
	transferred.QuadPart = 0;
	streamSize.QuadPart = 0;
	streamTransferred.QuadPart = 0;
	ULONGLONG done = 0;
	DWORD chunk = 0;
	BOOL report = TRUE;
	BOOL ok = FALSE;
	do
	{
		long read = iso->ReadFile(in, buffer, 0x8000);
		if (read < 1)
			break;
		if (done + (ULONGLONG)read > (ULONGLONG)size.QuadPart)
			read = (long)((ULONGLONG)size.QuadPart - done);
		if (report && progress != NULL)
		{
			// (The destination comes as the source handle; data isn't passed.)
			DWORD answer = progress(size, transferred, streamSize, streamTransferred, chunk, chunk == 0, out, in,
			                        NULL);
			if (answer == PROGRESS_CANCEL)
			{
				ok = FALSE;
				break;
			}
			if (answer == PROGRESS_QUIET)
				report = FALSE;
		}
		DWORD written;
		ok = WriteFile(out, buffer, (DWORD)read, &written, NULL);
		if (!ok)
		{
			SetLastError(GetLastError());
			break;
		}
		transferred.QuadPart += written;
		streamSize.QuadPart = read;
		streamTransferred.QuadPart += written;
		chunk++;
		done += read;
	} while (done < (ULONGLONG)size.QuadPart);
	CloseHandle(out);
	iso->CloseFile(in);
	delete[] buffer;
	SetFileAttributesA(destination, FILE_ATTRIBUTE_NORMAL);
	return ok;
}

// 0x00081B9D
BOOL CDirISO9660::CopyFile(const char *source, const char *destination, BOOL failIfExists)
{
	return CopyFromDisc(source, destination, failIfExists ? CREATE_NEW : CREATE_ALWAYS, NULL);
}

// 0x0008199C
BOOL CDirISO9660::CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress,
                             void * /*data*/, BOOL * /*cancel*/, DWORD flags)
{
	return CopyFromDisc(source, destination, flags == COPY_FILE_FAIL_IF_EXISTS ? CREATE_NEW : CREATE_ALWAYS,
	                    progress);
}
