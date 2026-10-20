// CFileHDD and CDirHDD: hard disk files and the plain Win32 calls. See
// FileIO.h.
#include <cstdio>
#include <cstring>
#include <string>

#include "../common/FileUtil.h"
#include "FileIO.h"

// ---- CFileHDD ----

// 0x000813B2 (this in eax)
CFileHDD::CFileHDD()
{
	m_pBuffers[0] = NULL;
	m_pBuffers[1] = NULL;
	m_nBuffer = 0;
	m_dwBufferSize = 0;
	m_hRead = NULL;
	m_bTruncate = FALSE;
	m_unknown60 = NULL;
	m_pWriteData = NULL;
	m_hWrite = NULL;
	m_hWriter = NULL;
	m_bStopWriter = FALSE;
	m_dwWriteSize = 0;
	m_nWritten = 0;
	m_nWrittenSeen = 0;
	m_dwWriteError = 0;
	m_hMutex = NULL;
	m_bWroteBuffered = FALSE;
	m_bOverlapped = TRUE;
	m_ullWriteSize = 0;
	Reset(NULL);
}

// 0x0008134A
CFileHDD::~CFileHDD()
{
	Reset(NULL);
	CloseHandle(m_hWrite);   // (sic, Reset already closed it)
	if (m_unknown60 != NULL)
	{
		operator delete(m_unknown60);
		m_unknown60 = NULL;
	}
	for (int i = 0; i < 2; i++)
	{
		if (m_pBuffers[i] != NULL)
		{
			NtFreeVirtualMemory((void **)&m_pBuffers[i], &m_dwBufferSize, MEM_RELEASE);
			m_pBuffers[i] = NULL;
		}
	}
}

// 0x00080FB9 (this in esi)
void CFileHDD::Reset(HANDLE handle)
{
	if (m_hRead != NULL)
	{
		if (m_dwReadPending != 0 && !m_bEOF)
			WaitRead(TRUE, NULL);
		if (m_bTruncate)
		{
			LONG high = (LONG)(m_ullPosition >> 32);
			Seek((LONG)m_ullPosition, high != 0 ? &high : NULL, FILE_BEGIN);
			SetEndOfFile(m_hRead);
		}
		CloseHandle(m_hRead);
		m_hRead = NULL;
		if (m_overlapped.hEvent != NULL)
		{
			CloseHandle(m_overlapped.hEvent);
			m_overlapped.hEvent = NULL;
		}
	}

	// Stop the writer (it finishes the block it has).
	m_bStopWriter = TRUE;
	if (m_hWriter != NULL && m_hWriter != INVALID_HANDLE_VALUE)
		ResumeThread(m_hWriter);
	if (m_hMutex != NULL && m_hMutex != INVALID_HANDLE_VALUE)
	{
		ReleaseMutex(m_hMutex);
		if (m_hWriter != NULL && m_hWriter != INVALID_HANDLE_VALUE)
			WaitForSingleObject(m_hWriter, INFINITE);
		WaitForSingleObject(m_hMutex, INFINITE);
		CloseHandle(m_hWriter);
		CloseHandle(m_hMutex);
		m_hMutex = NULL;
		m_hWriter = NULL;
	}
	if (m_hWriter != NULL && m_hWriter != INVALID_HANDLE_VALUE)
	{
		m_bStopWriter = TRUE;
		WaitForSingleObject(m_hWriter, INFINITE);
		CloseHandle(m_hWriter);
		m_hWriter = NULL;
	}

	if (m_hWrite != NULL && m_hWrite != INVALID_HANDLE_VALUE)
	{
		// OpenWrite stretched the file; cut it back to what was written.
		if (!m_bWroteBuffered)
		{
			LONG high = (LONG)(m_ullWriteSize >> 32);
			SetFilePointer(m_hWrite, (LONG)m_ullWriteSize, &high, FILE_BEGIN);
		}
		m_ullWriteSize = 0;
		SetEndOfFile(m_hWrite);
		CloseHandle(m_hWrite);
		m_hWrite = NULL;
		m_bWroteBuffered = FALSE;
	}

	// (The original clears +0x08..+0x47 in one go.)
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_unknown1C[0] = m_unknown1C[1] = 0;
	m_dwRead = 0;
	m_unknown34 = 0;
	m_unknown40[0] = m_unknown40[1] = 0;
	m_bEOF = FALSE;
	m_dwReadPending = 0;
	m_hRead = handle != INVALID_HANDLE_VALUE ? handle : NULL;
	m_ullPosition = 0;
	for (int i = 0; i < 2; i++)
	{
		if (m_pBuffers[i] != NULL)
		{
			NtFreeVirtualMemory((void **)&m_pBuffers[i], &m_dwBufferSize, MEM_RELEASE);
			m_pBuffers[i] = NULL;
		}
	}
	m_nBuffer = 0;
	m_pData = NULL;
}

// 0x0008125A
BOOL CFileHDD::OpenRead(const char *path, DWORD bufferSize, BOOL overlapped)
{
	m_bOverlapped = overlapped;
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	                          overlapped ? FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING : FILE_FLAG_SEQUENTIAL_SCAN,
	                          NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	Reset(file);
	m_dwBufferSize = bufferSize != 0 ? bufferSize : 0x10000;
	if (m_dwBufferSize & 0x1FF)
	{
		// Quirk: meant to round down to whole sectors, but also takes off
		// up to 0xC0000 more.
		DWORD rounded = m_dwBufferSize - (m_dwBufferSize & 0x1FF);
		m_dwBufferSize = rounded - (rounded & 0x7FF) * 0x200;
	}
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileInformationByHandle(file, &info) && info.nFileSizeHigh == 0 && info.nFileSizeLow == 0)
	{
		m_pData = NULL;
		m_bEOF = TRUE;
	}
	NtAllocateVirtualMemory((void **)&m_pBuffers[0], 0, &m_dwBufferSize, MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE);
	NtAllocateVirtualMemory((void **)&m_pBuffers[1], 0, &m_dwBufferSize, MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE);
	if (m_pBuffers[0] != NULL && m_pBuffers[1] != NULL)
	{
		m_pData = m_pBuffers[0];
		m_nBuffer = 0;
		m_bTruncate = FALSE;
		return TRUE;
	}
	Reset(NULL);
	return FALSE;
}

// 0x00081419
BOOL CFileHDD::OpenWrite(const char *path, DWORD bufferSize, DWORD /*unknown1*/, DWORD /*unknown2*/,
                         DWORD creationDisposition)
{
	m_bStopWriter = TRUE;
	Reset(NULL);
	CreateDirectoryTree(GetPathDirectory(path).c_str());
	m_hWrite = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, creationDisposition,
	                       FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (m_hWrite == INVALID_HANDLE_VALUE)
		return FALSE;

	m_dwBufferSize = 0;
	m_dwWriteSize = bufferSize;
	if (bufferSize != 0)
	{
		NtAllocateVirtualMemory((void **)&m_pBuffers[0], 0, &m_dwWriteSize, MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE);
		NtAllocateVirtualMemory((void **)&m_pBuffers[1], 0, &m_dwWriteSize, MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE);
		if (m_pBuffers[0] == NULL || m_pBuffers[1] == NULL)
		{
			Reset(NULL);
			return FALSE;
		}
	}
	m_dwBufferSize = m_dwWriteSize;
	m_bStopWriter = FALSE;
	m_nBuffer = 0;
	m_nWritten = 0;
	m_nWrittenSeen = 0;
	m_dwWriteError = 0;
	m_hMutex = NULL;
	if (m_dwWriteSize != 0)
	{
		// Started by the first Write().
		m_hWriter = (HANDLE)_beginthreadex(NULL, 0, WriterThread, this, CREATE_SUSPENDED, NULL);
		if (m_hWriter == INVALID_HANDLE_VALUE)
		{
			Reset(NULL);
			return FALSE;
		}
		SetThreadPriority(m_hWriter, THREAD_PRIORITY_ABOVE_NORMAL);
	}

	m_ullWriteSize = 0;
	GetFileSizeEx(m_hWrite, (LARGE_INTEGER *)&m_ullWriteSize);
	SetLastError(0);
	if (creationDisposition == OPEN_ALWAYS)
		SetFilePointer(m_hWrite, 0, NULL, FILE_END);
	if (creationDisposition == CREATE_ALWAYS)
	{
		// Stretch the new file (to 4 GB + 4 MB) so it's allocated in one go;
		// closing cuts it back. (FATX refuses it, so it's a no-op there.)
		LONG high = 1;
		SetFilePointer(m_hWrite, 0x400000, &high, FILE_BEGIN);
		if (GetLastError() == 0)
			SetEndOfFile(m_hWrite);
		SetFilePointer(m_hWrite, 0, NULL, FILE_BEGIN);
	}
	m_pData = m_pBuffers[m_nBuffer];
	m_pWriteData = m_pBuffers[m_nBuffer];
	if (m_pBuffers[0] != NULL)
		memset(m_pBuffers[0], 0, m_dwWriteSize);
	if (m_pBuffers[1] != NULL)
		memset(m_pBuffers[1], 0, m_dwWriteSize);
	m_bWroteBuffered = FALSE;
	return TRUE;
}

// 0x00080C80
BOOL CFileHDD::WaitRead(BOOL wait, DWORD *bytes)
{
	if (m_dwReadPending == 0 || m_hRead == NULL)
	{
		if (bytes != NULL)
			*bytes = m_dwRead;
		return TRUE;
	}
	if (!GetOverlappedResult(m_hRead, &m_overlapped, &m_dwRead, wait))
	{
		DWORD error = GetLastError();
		if (error == ERROR_HANDLE_EOF)
		{
			m_dwReadPending = 0;
			m_bEOF = TRUE;
		}
		else
		{
			if (error == ERROR_IO_INCOMPLETE || error == ERROR_IO_PENDING)
				return FALSE;
			m_dwReadPending = 0;
		}
		return TRUE;
	}
	m_ullPosition += m_dwRead;
	if (bytes != NULL)
		*bytes = m_dwRead;
	m_dwReadPending = 0;
	m_bEOF = m_dwRead < m_dwBufferSize;
	m_pData = m_pBuffers[m_nBuffer];
	m_nBuffer ^= 1;
	return TRUE;
}

// 0x00080E1D
HRESULT CFileHDD::ReadNext(DWORD size)
{
	if (m_bEOF)
		return E_FAIL;
	OVERLAPPED *overlapped;
	if (!m_bOverlapped)
	{
		if (size == 0 || size >= m_dwBufferSize)
			size = m_dwBufferSize;
		overlapped = NULL;
	}
	else
	{
		size = m_dwBufferSize;
		m_overlapped.Offset = (DWORD)m_ullPosition;
		m_overlapped.OffsetHigh = (DWORD)(m_ullPosition >> 32);
		overlapped = &m_overlapped;
	}
	if (!ReadFile(m_hRead, m_pBuffers[m_nBuffer], size, &m_dwRead, overlapped))
	{
		DWORD error = GetLastError();
		if (error == ERROR_HANDLE_EOF)
		{
			m_pBuffers[m_nBuffer][0] = 0;
			m_pData = m_pBuffers[m_nBuffer];
			m_dwReadPending = 0;
			m_bEOF = TRUE;
		}
		else
		{
			if (error != ERROR_IO_PENDING)
			{
				m_dwReadPending = 0;
				return E_FAIL;
			}
			m_dwReadPending = m_dwBufferSize;
			// The first block is waited for.
			if (m_overlapped.Offset == 0)
				WaitRead(TRUE, NULL);
		}
	}
	else
	{
		m_ullPosition += m_dwRead;
		m_dwReadPending = 0;
		m_bEOF = m_dwRead == 0;
		m_pData = m_pBuffers[m_nBuffer];
		m_nBuffer ^= 1;
	}
	return S_OK;
}

// 0x00080EFE
DWORD CFileHDD::Read(void *buffer, DWORD size)
{
	if (m_bEOF || buffer == NULL)
		return 0;
	if (!ReadFile(m_hRead, buffer, size, &m_dwRead, NULL))
		return 0;
	m_ullPosition += m_dwRead;
	m_dwReadPending = 0;
	m_bEOF = m_dwRead == 0;
	return m_dwRead;
}

// 0x00080F62
void CFileHDD::Seek(LONG distance, LONG *distanceHigh, DWORD method)
{
	if (m_hRead != NULL && WaitRead(TRUE, NULL))
	{
		DWORD low = SetFilePointer(m_hRead, distance, distanceHigh, method);
		if (low != INVALID_SET_FILE_POINTER)
		{
			// (The high half only changes when it was asked for.)
			m_ullPosition = (m_ullPosition & 0xFFFFFFFF00000000ULL) | low;
			if (distanceHigh != NULL)
				m_ullPosition = ((ULONGLONG)(DWORD)*distanceHigh << 32) | low;
		}
	}
	if (m_hWrite != NULL)
		SetFilePointer(m_hWrite, distance, distanceHigh, method);
}

// 0x00080D1F
DWORD CFileHDD::GetSize()
{
	LARGE_INTEGER size;
	size.QuadPart = 0;
	GetFileSizeEx(m_hWrite != NULL ? m_hWrite : m_hRead, &size);
	return size.u.LowPart;
}

// 0x000327A3
BYTE *CFileHDD::GetBuffer(BOOL first, DWORD *size)
{
	if (size != NULL)
		*size = m_dwBufferSize;
	return first ? m_pBuffers[0] : m_pBuffers[1];
}

// 0x00081142
HRESULT CFileHDD::Write(const void *buffer, DWORD size)
{
	if (m_dwWriteError != 0)
	{
		SetLastError(m_dwWriteError);
		return E_FAIL;
	}
	if (size == 0)
	{
		Reset(NULL);
		return m_dwWriteError != 0 ? E_FAIL : S_OK;
	}
	if (m_dwBufferSize == 0)
	{
		DWORD written;
		if (!WriteFile(m_hWrite, buffer, size, &written, NULL))
		{
			m_dwWriteError = GetLastError();
			return E_FAIL;
		}
		m_ullWriteSize += written;
		return S_OK;
	}

	// Hand the block to the writer thread, and go on as soon as it has
	// taken it (the caller fills the other buffer meanwhile).
	if (m_hMutex == NULL)
	{
		char name[12];
		sprintf(name, "MIO%08x", (unsigned)GetCurrentThreadId());
		m_hMutex = CreateMutexA(NULL, FALSE, name);
		if (m_hMutex == NULL || m_hMutex == INVALID_HANDLE_VALUE)
		{
			m_hMutex = NULL;
			return S_OK;   // (sic)
		}
		ResumeThread(m_hWriter);
	}
	WaitForSingleObject(m_hMutex, INFINITE);
	m_pWriteData = (const BYTE *)buffer;
	m_dwWriteSize = size;
	ReleaseMutex(m_hMutex);
	m_nBuffer ^= 1;
	m_bWroteBuffered = TRUE;
	while (m_nWrittenSeen == m_nWritten)
		Sleep(1);
	m_nWrittenSeen = m_nWritten;
	m_pData = m_pBuffers[m_nBuffer];
	return S_OK;
}

// 0x00080D45
unsigned __stdcall CFileHDD::WriterThread(void *param)
{
	CFileHDD *file = (CFileHDD *)param;
	if (file == NULL)
		return 0;
	DWORD result = 0;
	file->m_bStopWriter = FALSE;
	file->m_nWritten = 0;
	file->m_dwWriteError = 0;
	file->m_nWrittenSeen = 0;
	for (;;)
	{
		WaitForSingleObject(file->m_hMutex, INFINITE);
		if (file->m_bStopWriter || file->m_hMutex == NULL)
			break;
		if (file->m_dwWriteSize == 0)
		{
			ReleaseMutex(file->m_hMutex);
			Sleep(5);
			continue;
		}
		file->m_nWritten++;   // lets Write() return
		Sleep(1);
		DWORD written = 0;
		if (file->m_dwWriteSize == 0 ||
		    !WriteFile(file->m_hWrite, file->m_pWriteData, file->m_dwWriteSize, &written, NULL) || written == 0)
		{
			result = GetLastError();
			file->m_dwWriteError = result;
			break;
		}
		// (Clears the caller's buffer for the next block.)
		memset((void *)file->m_pWriteData, 0, file->m_dwWriteSize);
		file->m_dwWriteSize = 0;
		if (!ReleaseMutex(file->m_hMutex))
			return GetLastError();
	}
	ReleaseMutex(file->m_hMutex);
	return result;
}

// ---- CDirHDD ----

// 0x00081DEE
BOOL CDirHDD::CopyFile(const char *source, const char *destination, BOOL failIfExists)
{
	if (::CopyFileA(source, destination, failIfExists))
		return TRUE;
	return CopyFileEx(source, destination, NULL, NULL, NULL, failIfExists != 0);
}

// 0x00081E9A
BOOL CDirHDD::CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress, void *data,
                         BOOL *cancel, DWORD flags)
{
	if (::CopyFileExA(source, destination, XDK_CAST(CopyProgressRoutine, (void *)progress), data, cancel, flags))
		return TRUE;

	// Win32 refused (another drive type): copy it by hand, 256 KB at a time.
	HANDLE in = CreateFileA(source, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
	                        NULL);
	if (in == INVALID_HANDLE_VALUE)
		return FALSE;
	BYTE *buffer = new BYTE[0x40000];
	if (buffer == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);   // (the file stays open)
		return FALSE;
	}
	IFile *out = CreateFileObject(destination);
	if (out == NULL)
	{
		FindClose(in);   // (FindClose is CloseHandle here)
		delete[] buffer;
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}
	LARGE_INTEGER size;
	GetFileSizeEx(in, &size);
	if (!out->OpenWrite(destination, 0, size.u.LowPart, 0, CREATE_ALWAYS))
	{
		FindClose(in);
		delete[] buffer;
		SetLastError(GetLastError());   // (out is leaked)
		return FALSE;
	}

	BOOL ok = FALSE;   // (an empty file "fails")
	BOOL report = progress != NULL;
	ULONGLONG total = (ULONGLONG)size.QuadPart;
	ULONGLONG done = 0;
	LARGE_INTEGER transferred, streamSize, streamTransferred;
	transferred.QuadPart = 0;
	streamSize.QuadPart = 0;
	streamTransferred.QuadPart = 0;
	DWORD chunk = 0;
	for (;;)
	{
		DWORD read;
		ReadFile(in, buffer, 0x40000, &read, NULL);
		if (read == 0)
			break;
		if (done + read > total)
			read = (DWORD)(total - done);
		if (report && progress != NULL)
		{
			ok = FALSE;
			// Quirk: the handles come swapped, and data isn't passed.
			DWORD answer = progress(size, transferred, streamSize, streamTransferred, chunk, chunk == 0,
			                        out->GetHandle(), in, NULL);
			if (answer == PROGRESS_CANCEL)
				break;
			if (answer == PROGRESS_QUIET)
				report = FALSE;
		}
		ok = SUCCEEDED(out->Write(buffer, read));
		if (!ok)
		{
			SetLastError(GetLastError());
			break;
		}
		transferred.QuadPart += read;
		streamSize.QuadPart = read;
		streamTransferred.QuadPart += read;
		chunk++;
		done += read;
		if (done >= total)
			break;
	}
	out->Close();
	FindClose(in);
	delete[] buffer;
	delete out;
	return ok;
}

// 0x00081DC1
BOOL CDirHDD::MoveFile(const char *source, const char *destination)
{
	if (::MoveFileA(source, destination))
		return TRUE;
	return MoveFileWithProgress(source, destination, NULL, NULL, MOVEFILE_COPY_ALLOWED);
}

// 0x00081D90
BOOL CDirHDD::MoveFileEx(const char *source, const char *destination, DWORD flags)
{
	if (::MoveFileExA(source, destination, flags))
		return TRUE;
	return MoveFileWithProgress(source, destination, NULL, NULL, flags);
}

// 0x00081D40
BOOL CDirHDD::MoveFileWithProgress(const char *source, const char *destination, CopyProgressRoutine progress,
                                   void *data, DWORD flags)
{
	BOOL ok = ::MoveFileWithProgressA(source, destination, XDK_CAST(CopyProgressRoutine, (void *)progress), data, flags);
	if (!ok && CopyFileEx(source, destination, progress, data, NULL, flags))
		ok = DeleteFile(source);
	return ok;
}

// 0x00081E60
BOOL CDirHDD::FileExists(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// 0x00081E26
BOOL CDirHDD::DirectoryExists(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
