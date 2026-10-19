// CFileFAT32 and CDirFAT32: USB drives (P: to S:), through the FAT32
// driver. See FileIO.h.
#include <cstring>
#include <string>

#include "../common/StrUtil.h"
#include "../thirdparty/Fat32.h"
#include "FileIO.h"

// ---- CFileFAT32 ----

// 0x0008165A (this in eax, drive in ecx)
CFileFAT32::CFileFAT32(int drive)
{
	m_unknown10 = 0xFFFFFFFF;
	m_nDrive = drive;
	m_pFile = NULL;
	m_pVolume = NULL;
	m_pBuffer = NULL;
	m_dwBufferSize = 0x20000;
	memset(m_unknown20, 0, sizeof(m_unknown20));
	m_unknown30 = TRUE;
	m_dwLastRead = 0;
	m_bWriteFailed = FALSE;
}

// 0x0008178C (this in esi)
CFileFAT32::~CFileFAT32()
{
	Close();
	m_pVolume = NULL;
}

// 0x00081639
HANDLE CFileFAT32::GetHandle()
{
	if (m_pFile != NULL)
		return (HANDLE)(uintptr_t)m_pFile->startCluster;
	return INVALID_HANDLE_VALUE;
}

// 0x0008173F (this in ebx)
void CFileFAT32::Close()
{
	if (m_pFile != NULL)
	{
		m_pFile->open = FALSE;   // (before closing it)
		m_pVolume = Fat32GetVolume(m_nDrive);
		if (m_pVolume != NULL)
			Fat32Close(m_pFile, Fat32GetIo(m_pVolume));
		m_pFile = NULL;
	}
	if (m_pBuffer != NULL)
	{
		delete[] m_pBuffer;
		m_pBuffer = NULL;
	}
}

// 0x00081827
BOOL CFileFAT32::OpenRead(const char *path, DWORD bufferSize, BOOL /*overlapped*/)
{
	if (m_pVolume == NULL)
	{
		m_pVolume = Fat32MountVolume(m_nDrive);
		if (m_pVolume == NULL)
			return FALSE;
	}
	Close();
	m_dwBufferSize = bufferSize != 0 ? bufferSize : 0x20000;
	m_pBuffer = new BYTE[m_dwBufferSize];
	if (m_pBuffer == NULL && bufferSize != 0)
	{
		Close();
		return FALSE;
	}
	m_pFile = Fat32Open(Fat32GetIo(m_pVolume), path, "r");
	return m_pFile != NULL;
}

// 0x000817A4
BOOL CFileFAT32::OpenWrite(const char *path, DWORD bufferSize, DWORD /*unknown1*/, DWORD /*unknown2*/,
                           DWORD creationDisposition)
{
	if (m_pVolume == NULL)
	{
		m_pVolume = Fat32MountVolume(m_nDrive);
		if (m_pVolume == NULL)
			return FALSE;
	}
	Close();
	m_dwBufferSize = bufferSize != 0 ? bufferSize : 0x20000;
	m_pBuffer = new BYTE[m_dwBufferSize];
	if (m_pBuffer == NULL)
	{
		Close();
		return FALSE;
	}
	m_pFile = Fat32Open(Fat32GetIo(m_pVolume), path, creationDisposition == CREATE_ALWAYS ? "w" : "w+");
	return m_pFile != NULL;
}

// 0x000816BC
DWORD CFileFAT32::Read(void *buffer, DWORD size)
{
	if (m_pFile == NULL || buffer == NULL)
		return 0;
	int read = Fat32Read(m_pFile, Fat32GetIo(m_pVolume), buffer, (int)size);
	m_dwLastRead = (DWORD)read;
	if (read > 0)
		return (DWORD)read;
	m_unknown30 = TRUE;
	return 0;
}

// 0x000815E4
HRESULT CFileFAT32::ReadNext(DWORD size)
{
	// (size isn't held to the buffer's.)
	return Read(m_pBuffer, size) != 0 ? S_OK : E_FAIL;
}

// 0x00081603
BOOL CFileFAT32::WaitRead(BOOL /*wait*/, DWORD *bytes)
{
	if (m_pFile == NULL)
		return FALSE;
	if (bytes != NULL)
		*bytes = m_dwLastRead;
	return TRUE;
}

// 0x000816FD
HRESULT CFileFAT32::Write(const void *buffer, DWORD size)
{
	if (m_pFile != NULL)
	{
		m_bWriteFailed = FALSE;
		// (size 0 means "up to the NUL" to the driver, not "close".)
		if (Fat32Write(m_pFile, Fat32GetIo(m_pVolume), buffer, (int)size) > 0)
			return S_OK;
	}
	m_bWriteFailed = TRUE;
	return E_FAIL;
}

// 0x00081693
void CFileFAT32::Seek(LONG distance, LONG * /*distanceHigh*/, DWORD method)
{
	if (m_pFile != NULL)
		Fat32Seek(m_pFile, Fat32GetIo(m_pVolume), distance, (int)method);
}

// 0x00081620
BOOL CFileFAT32::IsEOF()
{
	if (m_pFile != NULL)
		return m_pFile->position == m_pFile->length;
	return TRUE;
}

// 0x00081648
DWORD CFileFAT32::GetSize()
{
	return m_pFile != NULL ? m_pFile->length : 0;
}

// 0x00032755
BYTE *CFileFAT32::GetBuffer(BOOL /*first*/, DWORD *size)
{
	if (size != NULL)
		*size = m_dwBufferSize;
	return m_pBuffer;
}

// ---- CDirFAT32 ----

// 0x00087414 (this in esi, drive in eax)
CDirFAT32::CDirFAT32(int drive)
{
	m_nDrive = drive;
	m_pVolume = Fat32MountVolume(drive);
}

// The volume, looked up again (it may have been unplugged). (inlined)
CFat32Volume *CDirFAT32::GetVolume()
{
	m_pVolume = Fat32GetVolume(m_nDrive);
	return m_pVolume;
}

// 0x000873EF
HANDLE CDirFAT32::FindFirst(const char *pattern, WIN32_FIND_DATAA *data)
{
	if (GetVolume() == NULL)
		return INVALID_HANDLE_VALUE;
	return Fat32FindFirst(m_pVolume, pattern, data);
}

// 0x00087397
BOOL CDirFAT32::FindNext(HANDLE find, WIN32_FIND_DATAA *data)
{
	if (m_pVolume == NULL)
		return FALSE;
	return Fat32FindNext(m_pVolume, find, data);
}

// 0x000873B3
DWORD CDirFAT32::GetFileSize(HANDLE file, DWORD * /*high*/)
{
	// The "handle" is the file's start cluster (CFileFAT32::GetHandle).
	BYTE entry[32];
	if (GetVolume() == NULL || file == NULL || file == INVALID_HANDLE_VALUE ||
	    !Fat32GetDirEntry(m_pVolume, (DWORD)(uintptr_t)file, entry))
		return 0;
	DWORD size;
	memcpy(&size, entry + 0x1C, sizeof(size));
	return size;
}

// 0x00087915
BOOL CDirFAT32::RemoveDirectory(const char *path)
{
	if (GetVolume() == NULL)
		return FALSE;
	return Fat32RemoveDirectory(m_pVolume, path);
}

// 0x00087365
BOOL CDirFAT32::CopyFile(const char *source, const char *destination, BOOL failIfExists)
{
	return CopyFileEx(source, destination, NULL, NULL, NULL, failIfExists != 0);
}

// 0x0008769D
BOOL CDirFAT32::CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress,
                           void * /*data*/, BOOL * /*cancel*/, DWORD /*flags*/)
{
	if (GetVolume() == NULL)
		return FALSE;
	BYTE *buffer = new BYTE[0x20000];
	if (buffer == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}
	FAT32_FILE *in = Fat32Open(Fat32GetIo(m_pVolume), source, "r");
	if (in == NULL)
	{
		SetLastError(2);   // ERROR_FILE_NOT_FOUND (buffer leaked)
		return FALSE;
	}
	IFile *out = CreateFileObject(destination);
	if (out == NULL)
	{
		Fat32Close(in, Fat32GetIo(m_pVolume));
		delete[] buffer;
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}
	if (!out->OpenWrite(destination, 0, in->length, 0, CREATE_ALWAYS))
	{
		Fat32Close(in, Fat32GetIo(m_pVolume));
		delete[] buffer;
		SetLastError(GetLastError());   // (out is leaked)
		return FALSE;
	}

	DWORD total = in->length;
	LARGE_INTEGER size, transferred, streamSize, streamTransferred;
	size.QuadPart = total;
	transferred.QuadPart = 0;
	streamSize.QuadPart = 0;
	streamTransferred.QuadPart = 0;
	DWORD done = 0;
	DWORD chunk = 0;
	BOOL report = progress != NULL;
	BOOL ok = FALSE;
	for (;;)
	{
		int read = Fat32Read(in, Fat32GetIo(m_pVolume), buffer, 0x20000);
		if (read < 1)
			break;
		if (done + (DWORD)read > total)
			read = (int)(total - done);
		if (report && progress != NULL)
		{
			// (The handles come swapped; data isn't passed.)
			DWORD answer = progress(size, transferred, streamSize, streamTransferred, chunk, chunk == 0,
			                        out->GetHandle(), (HANDLE)(uintptr_t)in->startCluster, NULL);
			if (answer == PROGRESS_CANCEL)
			{
				ok = FALSE;
				break;
			}
			if (answer == PROGRESS_QUIET)
				report = FALSE;
		}
		ok = SUCCEEDED(out->Write(buffer, (DWORD)read));
		if (!ok)
		{
			SetLastError(GetLastError());
			break;
		}
		transferred.QuadPart += read;
		streamSize.QuadPart = read;
		streamTransferred.QuadPart += read;
		chunk++;
		done += (DWORD)read;
		if (done >= total)
			break;
	}
	out->Close();
	Fat32Close(in, Fat32GetIo(m_pVolume));
	delete[] buffer;
	delete out;
	return ok;
}

// 0x000874A6
BOOL CDirFAT32::CreateDirectory(const char *path, void * /*security*/)
{
	if (GetVolume() == NULL)
		return FALSE;
	std::string all = _strdup(path);   // (never freed)
	if (all.size() <= 3)
		return FALSE;
	StrReplaceAll(all, "/", "\\");
	if (DirectoryExists(all.c_str()))
		return TRUE;
	// Each folder on the way, from the drive root.
	std::string part = StrLeft(all, 3);
	int pos = 3;
	while (all.compare(part) != 0)
	{
		if (!DirectoryExists(part.c_str()))
		{
			FAT32_FILE *folder = Fat32CreateDirectory(Fat32GetIo(m_pVolume), part.c_str(), 0, 1, 0);
			if (folder == NULL)
				return FALSE;
			Fat32Close(folder, Fat32GetIo(m_pVolume));
		}
		pos = StrFind(all, "\\", pos + 1);
		if (pos == -1)
			break;
		part = StrLeft(all, pos);
	}
	if (!DirectoryExists(all.c_str()))
	{
		FAT32_FILE *folder = Fat32CreateDirectory(Fat32GetIo(m_pVolume), all.c_str(), 0, 1, 0);
		if (folder != NULL)
			Fat32Close(folder, Fat32GetIo(m_pVolume));
	}
	// Quirk: says it failed even when it made the folders.
	return FALSE;
}

// 0x00087478
BOOL CDirFAT32::DeleteFile(const char *path)
{
	if (GetVolume() == NULL)
		return FALSE;
	return Fat32Remove(Fat32GetIo(m_pVolume), path) == 0;
}

// 0x0008734F
BOOL CDirFAT32::MoveFile(const char *source, const char *destination)
{
	return MoveFileWithProgress(source, destination, NULL, NULL, MOVEFILE_COPY_ALLOWED);
}

// 0x00087337
BOOL CDirFAT32::MoveFileEx(const char *source, const char *destination, DWORD flags)
{
	return MoveFileWithProgress(source, destination, NULL, NULL, flags);
}

// 0x000872FE
BOOL CDirFAT32::MoveFileWithProgress(const char *source, const char *destination, CopyProgressRoutine progress,
                                     void *data, DWORD flags)
{
	// A copy, then the delete.
	if (m_pVolume == NULL || !CopyFileEx(source, destination, progress, data, NULL, flags))
		return FALSE;
	return DeleteFile(source);
}

// 0x0008742A
BOOL CDirFAT32::FileExists(const char *path)
{
	if (GetVolume() == NULL)
		return FALSE;
	FAT32_FILE *file = Fat32Open(Fat32GetIo(m_pVolume), path, "r");
	if (file == NULL)
		return FALSE;
	Fat32Close(file, Fat32GetIo(m_pVolume));
	return TRUE;
}

// 0x000872EE
BOOL CDirFAT32::DirectoryExists(const char *path)
{
	// Quirk: asks FileExists (opening a folder as a file works here).
	if (m_pVolume == NULL)
		return FALSE;
	return FileExists(path);
}
