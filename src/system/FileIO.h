// The virtual file layer: CFile/CDir objects for a path, picked by drive.
// CFileHDD (hard disk; double-buffered overlapped reads and a writer thread
// for writes), CFileISO9660 (DVD data discs), CFileFAT32 (USB drives P..S),
// and the matching CDir classes. Only the interfaces the recovered code calls
// are declared. TODO: decompile
#pragma once

#include "../common/Platform.h"

// vtable layout of CFileHDD (0x0025C45C), CFileISO9660 and CFileFAT32.
class CFile
{
public:
	virtual ~CFile() {}
	virtual HANDLE GetHandle() = 0;                                          // 1
	virtual void Close() = 0;                                                // 2
	// bufferSize 0 = 64 KB. overlapped: read ahead asynchronously.
	virtual BOOL OpenRead(const char *path, DWORD bufferSize, BOOL overlapped) = 0;   // 3
	// Creates the folders on the way. bufferSize != 0: writes go through
	// two buffers of that size and a writer thread.
	virtual BOOL OpenWrite(const char *path, DWORD bufferSize, DWORD unknown1, DWORD unknown2,
	                       DWORD creationDisposition) = 0;                  // 4
	// Waits for the read started by ReadNext(); bytes = its length.
	virtual BOOL WaitRead(BOOL wait, DWORD *bytes) = 0;                      // 5
	virtual DWORD Read(void *buffer, DWORD size) = 0;                        // 6
	// Starts reading the next block (size 0 = the buffer size).
	virtual HRESULT ReadNext(DWORD size) = 0;                                // 7
	// Hands a filled buffer to the writer thread (size 0 = close).
	virtual HRESULT Write(const void *buffer, DWORD size) = 0;               // 8
	// One of the two buffers: first = TRUE for the first one.
	virtual BYTE *GetBuffer(BOOL first, DWORD *size) = 0;                    // 9
	// The block the last WaitRead() completed.
	virtual BYTE *GetData() = 0;                                             // 10
	virtual BOOL IsEOF() = 0;                                                // 11
	virtual void Seek(LONG distance, LONG *distanceHigh, DWORD method) = 0;  // 12
	virtual DWORD GetSize() = 0;                                             // 13
	// The writer thread's error, or 0.
	virtual DWORD GetError() = 0;                                            // 14
};

// vtable layout of CDirHDD (0x0025BF1C) and friends, as far as used.
class CDir
{
public:
	virtual ~CDir() {}
	virtual BOOL FindClose(HANDLE find) = 0;                                 // 1
	virtual HANDLE FindFirst(const char *pattern, WIN32_FIND_DATAA *data) = 0;   // 2
	virtual BOOL FindNext(HANDLE find, WIN32_FIND_DATAA *data) = 0;          // 3
};

// NULL if the drive isn't there (U: and T: always count as hard disk).
CFile *CreateFileObject(const char *path);   // 0x001376AF
CDir *CreateDirObject(const char *path);     // 0x0013D5CF

// Whether path exists as a directory (directory = TRUE) or a file. A drive
// root ("C:\") counts if the drive is mounted. Falls back to asking the
// drive's CDir. TODO: decompile
BOOL PathExists(const char *path, BOOL directory);   // 0x0002FB84
