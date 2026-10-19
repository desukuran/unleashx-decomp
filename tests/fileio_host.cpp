// Host stand-ins for the virtual file layer (src/system/FileIO.h): plain
// files with the same buffering contract as CFileHDD, so the FTP server's
// RETR/STOR/LIST run in the host tests.
#include <cstring>

#include "../src/common/XboxKernel.h"
#include "../src/system/FileIO.h"

class CHostFile : public CFile
{
public:
	CHostFile() : m_file(NULL), m_bufferSize(0), m_next(0), m_data(NULL), m_pending(0), m_hasPending(FALSE),
	              m_last(0), m_eof(FALSE)
	{
		m_buffers[0] = m_buffers[1] = NULL;
	}
	~CHostFile()
	{
		Close();
		delete[] m_buffers[0];
		delete[] m_buffers[1];
	}

	HANDLE GetHandle() { return m_file; }
	void Close()
	{
		if (m_file != NULL)
			CloseHandle(m_file);
		m_file = NULL;
	}
	BOOL OpenRead(const char *path, DWORD bufferSize, BOOL)
	{
		m_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (m_file == INVALID_HANDLE_VALUE)
		{
			m_file = NULL;
			return FALSE;
		}
		Allocate(bufferSize != 0 ? bufferSize : 0x10000);
		m_eof = GetSize() == 0;
		return TRUE;
	}
	BOOL OpenWrite(const char *path, DWORD bufferSize, DWORD, DWORD, DWORD creationDisposition)
	{
		m_file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, creationDisposition, 0, NULL);
		if (m_file == INVALID_HANDLE_VALUE)
		{
			m_file = NULL;
			return FALSE;
		}
		Allocate(bufferSize);
		return TRUE;
	}
	BOOL WaitRead(BOOL, DWORD *bytes)
	{
		if (m_hasPending)
		{
			m_last = m_pending;
			m_hasPending = FALSE;
			m_eof = m_last < m_bufferSize;
			m_data = m_buffers[m_next];
			m_next ^= 1;
		}
		*bytes = m_last;
		return TRUE;
	}
	DWORD Read(void *buffer, DWORD size)
	{
		DWORD read = 0;
		ReadFile(m_file, buffer, size, &read, NULL);
		return read;
	}
	HRESULT ReadNext(DWORD size)
	{
		if (m_eof)
			return E_FAIL;
		if (size == 0 || size > m_bufferSize)
			size = m_bufferSize;
		ReadFile(m_file, m_buffers[m_next], size, &m_pending, NULL);
		m_hasPending = TRUE;
		return S_OK;
	}
	HRESULT Write(const void *buffer, DWORD size)
	{
		if (size == 0)
		{
			Close();
			return S_OK;
		}
		DWORD written = 0;
		return WriteFile(m_file, buffer, size, &written, NULL) && written == size ? S_OK : E_FAIL;
	}
	BYTE *GetBuffer(BOOL first, DWORD *size)
	{
		if (size != NULL)
			*size = m_bufferSize;
		return first ? m_buffers[0] : m_buffers[1];
	}
	BYTE *GetData() { return m_data; }
	BOOL IsEOF() { return m_eof; }
	void Seek(LONG distance, LONG *distanceHigh, DWORD method) { SetFilePointer(m_file, distance, distanceHigh, method); }
	DWORD GetSize()
	{
		BY_HANDLE_FILE_INFORMATION info;
		return GetFileInformationByHandle(m_file, &info) ? info.nFileSizeLow : 0;
	}
	DWORD GetError() { return 0; }

private:
	void Allocate(DWORD size)
	{
		m_bufferSize = size;
		m_buffers[0] = new BYTE[size];
		m_buffers[1] = new BYTE[size];
	}

	HANDLE m_file;
	BYTE *m_buffers[2];
	DWORD m_bufferSize;
	int m_next;
	BYTE *m_data;
	DWORD m_pending;
	BOOL m_hasPending;
	DWORD m_last;
	BOOL m_eof;
};

class CHostDir : public CDir
{
public:
	BOOL FindClose(HANDLE find) { return CloseHandle(find); }
	HANDLE FindFirst(const char *pattern, WIN32_FIND_DATAA *data) { return FindFirstFileA(pattern, data); }
	BOOL FindNext(HANDLE find, WIN32_FIND_DATAA *data) { return FindNextFileA(find, data); }
};

CFile *CreateFileObject(const char *)
{
	return new CHostFile;
}

CDir *CreateDirObject(const char *)
{
	return new CHostDir;
}

BOOL PathExists(const char *path, BOOL directory)
{
	DWORD attributes = GetFileAttributesA(path);
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return FALSE;
	return ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) == (directory != FALSE);
}
