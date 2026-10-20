// UnleashX build shim: the part of XBMC's XFILE::CFile that UnrarXLib uses,
// on the Win32 file calls. (Not XBMC code.)
#pragma once
#include <xtl.h>
#include <string>

namespace XFILE
{

class CFile
{
public:
	CFile() : m_hFile(INVALID_HANDLE_VALUE) {}
	~CFile() { Close(); }

	bool Open(const char *path)
	{
		Close();
		m_hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		return m_hFile != INVALID_HANDLE_VALUE;
	}
	bool OpenForWrite(const char *path, bool overwrite = false)
	{
		Close();
		m_hFile = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
		                      overwrite ? CREATE_ALWAYS : OPEN_ALWAYS, 0, NULL);
		return m_hFile != INVALID_HANDLE_VALUE;
	}
	void Close()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
			CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
	unsigned int Read(void *buffer, __int64 size)
	{
		DWORD read = 0;
		if (m_hFile == INVALID_HANDLE_VALUE || !ReadFile(m_hFile, buffer, (DWORD)size, &read, NULL))
			return 0;
		return read;
	}
	int Write(const void *buffer, __int64 size)
	{
		DWORD written = 0;
		if (m_hFile == INVALID_HANDLE_VALUE || !WriteFile(m_hFile, buffer, (DWORD)size, &written, NULL))
			return -1;
		return (int)written;
	}
	__int64 Seek(__int64 offset, int whence)
	{
		LARGE_INTEGER distance, position;
		distance.QuadPart = offset;
		if (m_hFile == INVALID_HANDLE_VALUE || !SetFilePointerEx(m_hFile, distance, &position, whence))
			return -1;
		return position.QuadPart;
	}
	__int64 GetPosition() { return Seek(0, FILE_CURRENT); }
	__int64 GetLength()
	{
		LARGE_INTEGER size;
		if (m_hFile == INVALID_HANDLE_VALUE || !GetFileSizeEx(m_hFile, &size))
			return 0;
		return size.QuadPart;
	}
	void Flush()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
			FlushFileBuffers(m_hFile);
	}
	static bool Delete(const std::string &path) { return DeleteFileA(path.c_str()) != FALSE; }
	static bool Exists(const std::string &path) { return GetFileAttributesA(path.c_str()) != (DWORD)-1; }

private:
	HANDLE m_hFile;
};

}   // namespace XFILE
