// The virtual file layer: file and directory objects for a path, picked by
// its drive (CreateFileObject, CreateDirObject).
//   hard disk, memory units...  CFileHDD / CDirHDD          (FileHDD.cpp)
//   D: with a data disc          CFileISO9660 / CDirISO9660  (FileISO9660.cpp, XBMC's iso9660)
//   P: to S: (USB drives)        CFileFAT32 / CDirFAT32      (FileFAT32.cpp, the FAT32 driver)
//   FTP sites (file manager)     CDirFTP                     (src/net/FtpClient.cpp)
// The directory objects carry the Win32 file calls (find, copy with
// progress, move, delete) so that the file manager and the file operations
// work the same on every drive.
#pragma once

#include "../common/Platform.h"
#include "../common/XboxKernel.h"

class iso9660;
struct CFat32Volume;
struct FAT32_FILE;

// CopyFileEx's progress callback (Win32's LPPROGRESS_ROUTINE): returns
// PROGRESS_CONTINUE (0), PROGRESS_CANCEL (1), PROGRESS_STOP (2) or
// PROGRESS_QUIET (3, no more calls).
typedef DWORD (__stdcall *CopyProgressRoutine)(LARGE_INTEGER totalSize, LARGE_INTEGER totalTransferred,
                                               LARGE_INTEGER streamSize, LARGE_INTEGER streamTransferred,
                                               DWORD streamNumber, DWORD reason, HANDLE source,
                                               HANDLE destination, void *data);

// A file opened for reading or writing. Reads can be double-buffered (one
// block read ahead while the last is used), writes can go through a writer
// thread. vtable 0x0025C3D4; the defaults below are the base class's.
class IFile
{
public:
	virtual ~IFile() {}                                                       // 0 (0x00032738)
	virtual HANDLE GetHandle() { return INVALID_HANDLE_VALUE; }               // 1 (0x00024ABD)
	virtual void Close() {}                                                   // 2 (0x00013D20)
	// bufferSize 0 = the class's default. overlapped: read ahead
	// asynchronously.
	virtual BOOL OpenRead(const char * /*path*/, DWORD /*bufferSize*/, BOOL /*overlapped*/) { return FALSE; }   // 3 (0x00032704)
	// Creates the folders on the way. bufferSize != 0: writes go through
	// two buffers of that size and a writer thread.
	virtual BOOL OpenWrite(const char * /*path*/, DWORD /*bufferSize*/, DWORD /*unknown1*/, DWORD /*unknown2*/,
	                       DWORD /*creationDisposition*/) { return FALSE; }      // 4 (0x00032709)
	// Waits for the read started by ReadNext(); bytes = its length.
	virtual BOOL WaitRead(BOOL /*wait*/, DWORD * /*bytes*/) { return TRUE; }          // 5 (0x0003272A)
	// A plain read into buffer.
	virtual DWORD Read(void * /*buffer*/, DWORD /*size*/) { return 0; }               // 6 (0x000326FF)
	// Starts reading the next block (size 0 = the buffer size).
	virtual HRESULT ReadNext(DWORD /*size*/) { return E_FAIL; }                  // 7 (0x00012990)
	// Writes a block (through the writer thread when buffered); size 0
	// closes the file.
	virtual HRESULT Write(const void * /*buffer*/, DWORD /*size*/) { return E_FAIL; } // 8 (0x00032730)
	// One of the two buffers: first = TRUE for the first one.
	virtual BYTE *GetBuffer(BOOL /*first*/, DWORD * /*size*/) { return NULL; }        // 9 (0x000326FF)
	// The block the last WaitRead() completed.
	virtual BYTE *GetData() { return NULL; }                                 // 10 (0x00068291)
	virtual BOOL IsEOF() { return TRUE; }                                    // 11 (0x000111E7)
	virtual void Seek(LONG /*distance*/, LONG * /*distanceHigh*/, DWORD /*method*/) {}    // 12 (0x0002A7A0)
	virtual DWORD GetSize() { return 0; }                                    // 13 (0x00068291)
	// The writer thread's error, or 0. (The base says 1.)
	virtual DWORD GetError() { return 1; }                                   // 14 (0x000111E7)
};

// A hard disk file. vtable 0x0025C45C, 0xA0 bytes.
class CFileHDD : public IFile
{
public:
	CFileHDD();                                                // 0x000813B2 (this in eax)
	virtual ~CFileHDD();                                       // 0x0008134A (deleting 0x000327CD)
	virtual HANDLE GetHandle() { return m_hRead != NULL ? m_hRead : m_hWrite; }   // 0x00032788
	virtual void Close() { Reset(INVALID_HANDLE_VALUE); }      // 0x00032793
	virtual BOOL OpenRead(const char *path, DWORD bufferSize, BOOL overlapped);   // 0x0008125A
	virtual BOOL OpenWrite(const char *path, DWORD bufferSize, DWORD unknown1, DWORD unknown2,
	                       DWORD creationDisposition);         // 0x00081419
	virtual BOOL WaitRead(BOOL wait, DWORD *bytes);            // 0x00080C80
	virtual DWORD Read(void *buffer, DWORD size);              // 0x00080EFE
	virtual HRESULT ReadNext(DWORD size);                      // 0x00080E1D
	virtual HRESULT Write(const void *buffer, DWORD size);     // 0x00081142
	virtual BYTE *GetBuffer(BOOL first, DWORD *size);          // 0x000327A3
	virtual BYTE *GetData() { return m_pData; }                // 0x0003279F
	virtual BOOL IsEOF() { return m_bEOF; }                    // 0x000327C2
	virtual void Seek(LONG distance, LONG *distanceHigh, DWORD method);   // 0x00080F62
	virtual DWORD GetSize();                                   // 0x00080D1F
	virtual DWORD GetError() { return m_dwWriteError; }        // 0x000327C6

	int m_unknown04;                   // +0x04
	OVERLAPPED m_overlapped;           // +0x08
	DWORD m_unknown1C[2];              // +0x1C
	DWORD m_dwReadPending;             // +0x24 an overlapped read is in flight
	DWORD m_dwRead;                    // +0x28 its length
	HANDLE m_hRead;                    // +0x2C
	BOOL m_bEOF;                       // +0x30
	DWORD m_unknown34;                 // +0x34
	ULONGLONG m_ullPosition;           // +0x38 where the next read starts
	DWORD m_unknown40[2];              // +0x40
	BYTE *m_pBuffers[2];               // +0x48 NtAllocateVirtualMemory'd
	BYTE *m_pData;                     // +0x50 the block last read
	int m_nBuffer;                     // +0x54 the buffer the next read goes into
	DWORD m_dwBufferSize;              // +0x58
	BOOL m_bTruncate;                  // +0x5C cut the file at the position on close (never set)
	void *m_unknown60;                 // +0x60 freed by the destructor (never set)
	HANDLE m_hMutex;                   // +0x64 hands blocks to the writer
	HANDLE m_hWrite;                   // +0x68
	HANDLE m_hWriter;                  // +0x6C the writer thread
	const BYTE *m_pWriteData;          // +0x70 the block for the writer
	DWORD m_dwWriteSize;               // +0x74 its size (also the buffer size while opening)
	int m_nWritten;                    // +0x78 blocks the writer finished
	int m_nWrittenSeen;                // +0x7C ... and that Write() saw
	BOOL m_bStopWriter;                // +0x80
	BOOL m_bWroteBuffered;             // +0x84
	BOOL m_bOverlapped;                // +0x88
	DWORD m_unknown8C;                 // +0x8C
	ULONGLONG m_ullWriteSize;          // +0x90 the file's size, then what was written
	DWORD m_unknown98;                 // +0x98
	DWORD m_dwWriteError;              // +0x9C

private:
	// Closes everything and frees the buffers; then takes handle as the
	// file to read (-1 for none). (this in esi)
	void Reset(HANDLE handle);                                 // 0x00080FB9
	static unsigned __stdcall WriterThread(void *param);       // 0x00080D45
};

// A file on a DVD data disc (read only). vtable 0x0025C4A4, 0x30 bytes.
class CFileISO9660 : public IFile
{
public:
	CFileISO9660();                                            // 0x00080BB3 (this in eax)
	virtual ~CFileISO9660();                                   // 0x00080B8B (deleting 0x00032808)
	virtual HANDLE GetHandle() { return m_hFile; }             // 0x000327E9
	virtual void Close();                                      // 0x00080B42
	virtual BOOL OpenRead(const char *path, DWORD bufferSize, BOOL overlapped);   // 0x00080BFC
	virtual BOOL WaitRead(BOOL wait, DWORD *bytes);            // 0x00080B06
	virtual DWORD Read(void *buffer, DWORD size);              // 0x00080BC6
	virtual HRESULT ReadNext(DWORD size);                      // 0x00080AD9
	virtual BYTE *GetBuffer(BOOL first, DWORD *size);          // 0x000327F1
	virtual BYTE *GetData() { return m_pBuffer; }              // 0x000327ED
	virtual BOOL IsEOF() { return m_bEOF; }                    // 0x00032804
	virtual void Seek(LONG distance, LONG *distanceHigh, DWORD method);   // 0x00080B19
	virtual DWORD GetSize() { return (DWORD)m_llSize; }        // 0x00023469

	iso9660 *m_pIso;                   // +0x04 the drive's (g_pDvdIso9660)
	HANDLE m_hFile;                    // +0x08 -1 = closed
	BYTE *m_pBuffer;                   // +0x0C
	DWORD m_dwBufferSize;              // +0x10 32 KB by default
	DWORD m_unknown14;                 // +0x14
	LONGLONG m_llSize;                 // +0x18
	LONGLONG m_llUnknown20;            // +0x20
	BOOL m_bEOF;                       // +0x28
	DWORD m_dwLastRead;                // +0x2C

private:
	BOOL GetIso();                                             // 0x00080B74 (this in ecx)
};

// A file on a USB drive. vtable 0x0025C414, 0x40 bytes.
class CFileFAT32 : public IFile
{
public:
	explicit CFileFAT32(int drive);                            // 0x0008165A (this in eax, drive in ecx)
	virtual ~CFileFAT32();                                     // 0x0008178C (this in esi; deleting 0x0003276C)
	virtual HANDLE GetHandle();                                // 0x00081639
	virtual void Close();                                      // 0x0008173F
	virtual BOOL OpenRead(const char *path, DWORD bufferSize, BOOL overlapped);   // 0x00081827
	virtual BOOL OpenWrite(const char *path, DWORD bufferSize, DWORD unknown1, DWORD unknown2,
	                       DWORD creationDisposition);         // 0x000817A4
	virtual BOOL WaitRead(BOOL wait, DWORD *bytes);            // 0x00081603
	virtual DWORD Read(void *buffer, DWORD size);              // 0x000816BC
	virtual HRESULT ReadNext(DWORD size);                      // 0x000815E4
	virtual HRESULT Write(const void *buffer, DWORD size);     // 0x000816FD
	virtual BYTE *GetBuffer(BOOL first, DWORD *size);          // 0x00032755
	virtual BYTE *GetData() { return m_pBuffer; }              // 0x00081656
	virtual BOOL IsEOF();                                      // 0x00081620
	virtual void Seek(LONG distance, LONG *distanceHigh, DWORD method);   // 0x00081693
	virtual DWORD GetSize();                                   // 0x00081648
	virtual DWORD GetError() { return m_bWriteFailed; }        // 0x00032768

	CFat32Volume *m_pVolume;           // +0x04
	FAT32_FILE *m_pFile;               // +0x08
	int m_nDrive;                      // +0x0C 0 = P:
	DWORD m_unknown10;                 // +0x10 -1
	BYTE *m_pBuffer;                   // +0x14
	DWORD m_dwBufferSize;              // +0x18 128 KB by default
	DWORD m_unknown1C;                 // +0x1C
	DWORD m_unknown20[4];              // +0x20
	BOOL m_unknown30;                  // +0x30 1; set at the end of a read
	DWORD m_dwLastRead;                // +0x34
	BOOL m_bWriteFailed;               // +0x38
	DWORD m_unknown3C;                 // +0x3C
};

// The Win32 file calls for one kind of drive. vtable 0x0025BED4; the
// defaults below are the base class's (most fail with error 1).
class CDirectory
{
public:
	virtual ~CDirectory() {}                                                  // 0 (0x0003219B)
	virtual BOOL FindClose(HANDLE /*find*/) { SetLastError(1); return FALSE; }    // 1 (0x0003215F)
	virtual HANDLE FindFirst(const char * /*pattern*/, WIN32_FIND_DATAA * /*data*/)
	{ SetLastError(1); return INVALID_HANDLE_VALUE; }                        // 2 (0x00032152)
	virtual BOOL FindNext(HANDLE /*find*/, WIN32_FIND_DATAA * /*data*/) { SetLastError(1); return FALSE; }   // 3 (0x00032183)
	virtual DWORD GetFileSize(HANDLE /*file*/, DWORD * /*high*/) { return 0; }         // 4 (0x000326FF)
	virtual BOOL RemoveDirectory(const char * /*path*/) { SetLastError(1); return FALSE; }   // 5 (0x0003215F)
	virtual BOOL CopyFile(const char * /*source*/, const char * /*destination*/, BOOL /*failIfExists*/)
	{ SetLastError(1); return FALSE; }                                       // 6 (0x0003216B)
	virtual BOOL CopyFileEx(const char * /*source*/, const char * /*destination*/, CopyProgressRoutine /*progress*/,
	                        void * /*data*/, BOOL * /*cancel*/, DWORD /*flags*/) { SetLastError(1); return FALSE; }   // 7 (0x00032177)
	virtual BOOL CreateDirectory(const char * /*path*/, void * /*security*/) { SetLastError(1); return FALSE; }   // 8 (0x00032183)
	virtual BOOL DeleteFile(const char * /*path*/) { SetLastError(1); return FALSE; }   // 9 (0x0003215F)
	virtual BOOL MoveFile(const char * /*source*/, const char * /*destination*/) { SetLastError(1); return FALSE; }   // 10 (0x00032183)
	virtual BOOL MoveFileEx(const char * /*source*/, const char * /*destination*/, DWORD /*flags*/)
	{ SetLastError(1); return FALSE; }                                       // 11 (0x0003216B)
	virtual BOOL MoveFileWithProgress(const char * /*source*/, const char * /*destination*/, CopyProgressRoutine /*progress*/,
	                                  void * /*data*/, DWORD /*flags*/) { SetLastError(1); return FALSE; }   // 12 (0x0003218F)
	virtual BOOL FileExists(const char * /*path*/) { return FALSE; }              // 13 (0x000326FA)
	virtual BOOL DirectoryExists(const char * /*path*/) { return FALSE; }         // 14 (0x000326FA)
	// DRIVE_HDD (0), DRIVE_DVD (1), 2 for USB, 3 for FTP; 4 here.
	virtual int GetDriveType() { return 4; }                                  // 15 (0x000129D9)
};

// The plain Win32 calls. vtable 0x0025BF1C, 4 bytes.
class CDirHDD : public CDirectory
{
public:
	virtual BOOL FindClose(HANDLE find) { return CloseHandle(find); }                              // 0x000321B8
	virtual HANDLE FindFirst(const char *pattern, WIN32_FIND_DATAA *data) { return FindFirstFileA(pattern, data); }   // 0x000321BD
	virtual BOOL FindNext(HANDLE find, WIN32_FIND_DATAA *data) { return FindNextFileA(find, data); }   // 0x000321C2
	virtual DWORD GetFileSize(HANDLE file, DWORD *high) { return ::GetFileSize(file, high); }     // 0x000321C7
	virtual BOOL RemoveDirectory(const char *path) { return RemoveDirectoryA(path); }             // 0x000321CC
	virtual BOOL CopyFile(const char *source, const char *destination, BOOL failIfExists);        // 0x00081DEE
	virtual BOOL CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress,
	                        void *data, BOOL *cancel, DWORD flags);                               // 0x00081E9A
	virtual BOOL CreateDirectory(const char *path, void *security) { return CreateDirectoryA(path, security); }   // 0x000321D1
	virtual BOOL DeleteFile(const char *path) { return DeleteFileA(path); }                       // 0x000321D6
	virtual BOOL MoveFile(const char *source, const char *destination);                           // 0x00081DC1
	virtual BOOL MoveFileEx(const char *source, const char *destination, DWORD flags);            // 0x00081D90
	virtual BOOL MoveFileWithProgress(const char *source, const char *destination, CopyProgressRoutine progress,
	                                  void *data, DWORD flags);                                   // 0x00081D40
	virtual BOOL FileExists(const char *path);                                                    // 0x00081E60
	virtual BOOL DirectoryExists(const char *path);                                               // 0x00081E26
	virtual int GetDriveType() { return 0; }                                                       // 0x00068291
};

// A DVD data disc, through its own iso9660 reader. vtable 0x0025C38C,
// 8 bytes.
class CDirISO9660 : public CDirectory
{
public:
	CDirISO9660() { m_pIso = NULL; }                                          // 0x00081966 (this in eax)
	virtual ~CDirISO9660();                                                   // 0x00081948 (this in esi; deleting 0x0003270E)
	virtual BOOL FindClose(HANDLE find);                                      // 0x00081938
	virtual HANDLE FindFirst(const char *pattern, WIN32_FIND_DATAA *data);    // 0x00081D1B
	virtual BOOL FindNext(HANDLE find, WIN32_FIND_DATAA *data);               // 0x00081927
	virtual DWORD GetFileSize(HANDLE /*file*/, DWORD * /*high*/) { return 0; }         // 0x00081922
	virtual BOOL RemoveDirectory(const char * /*path*/) { return FALSE; }          // 0x000326FA
	// Copies off the disc (to the hard disk, through Win32).
	virtual BOOL CopyFile(const char *source, const char *destination, BOOL failIfExists);   // 0x00081B9D
	virtual BOOL CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress,
	                        void *data, BOOL *cancel, DWORD flags);           // 0x0008199C
	virtual BOOL CreateDirectory(const char * /*path*/, void * /*security*/) { return FALSE; }   // 0x000326FF
	virtual BOOL DeleteFile(const char * /*path*/) { return FALSE; }               // 0x000326FA
	virtual BOOL MoveFile(const char * /*source*/, const char * /*destination*/) { return FALSE; }   // 0x000326FF
	virtual BOOL MoveFileEx(const char * /*source*/, const char * /*destination*/, DWORD /*flags*/) { return FALSE; }   // 0x00032704
	virtual BOOL MoveFileWithProgress(const char * /*source*/, const char * /*destination*/, CopyProgressRoutine /*progress*/,
	                                  void * /*data*/, DWORD /*flags*/) { return FALSE; }   // 0x00032709
	virtual BOOL FileExists(const char *path);                                // 0x000818DF
	virtual BOOL DirectoryExists(const char *path);                           // 0x000818A1
	virtual int GetDriveType() { return 1; }                                  // 0x000111E7

	iso9660 *m_pIso;                   // +0x04 created on first use

private:
	BOOL CreateIso();                                                         // 0x00081971 (this in esi)
	// Copies 32 KB at a time into a Win32 file; progress may be NULL.
	BOOL CopyFromDisc(const char *source, const char *destination, DWORD creation, CopyProgressRoutine progress);
};

// A USB drive, through the FAT32 driver. vtable 0x0025BF64, 0xC bytes.
class CDirFAT32 : public CDirectory
{
public:
	explicit CDirFAT32(int drive);                                            // 0x00087414 (this in esi, drive in eax)
	virtual ~CDirFAT32() { m_pVolume = NULL; }                                // 0x0008738C (deleting 0x000321DB)
	virtual BOOL FindClose(HANDLE /*find*/) { return TRUE; }                      // 0x00087386
	virtual HANDLE FindFirst(const char *pattern, WIN32_FIND_DATAA *data);    // 0x000873EF
	virtual BOOL FindNext(HANDLE find, WIN32_FIND_DATAA *data);               // 0x00087397
	virtual DWORD GetFileSize(HANDLE file, DWORD *high);                      // 0x000873B3
	virtual BOOL RemoveDirectory(const char *path);                           // 0x00087915
	virtual BOOL CopyFile(const char *source, const char *destination, BOOL failIfExists);   // 0x00087365
	virtual BOOL CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress,
	                        void *data, BOOL *cancel, DWORD flags);           // 0x0008769D
	virtual BOOL CreateDirectory(const char *path, void *security);           // 0x000874A6
	virtual BOOL DeleteFile(const char *path);                                // 0x00087478
	virtual BOOL MoveFile(const char *source, const char *destination);       // 0x0008734F
	virtual BOOL MoveFileEx(const char *source, const char *destination, DWORD flags);   // 0x00087337
	virtual BOOL MoveFileWithProgress(const char *source, const char *destination, CopyProgressRoutine progress,
	                                  void *data, DWORD flags);               // 0x000872FE
	virtual BOOL FileExists(const char *path);                                // 0x0008742A
	virtual BOOL DirectoryExists(const char *path);                           // 0x000872EE
	virtual int GetDriveType() { return 2; }                                  // 0x00023494

	CFat32Volume *m_pVolume;           // +0x04 looked up again before each call
	int m_nDrive;                      // +0x08 0 = P:

private:
	CFat32Volume *GetVolume();
};

// NULL if the drive isn't there (U: and T: always count as hard disk).
// (path in edi)
IFile *CreateFileObject(const char *path);   // 0x001376AF
// (path in eax)
CDirectory *CreateDirObject(const char *path);     // 0x0013D5CF

// Whether path exists as a directory (directory = TRUE) or a file. A drive
// root ("C:\") counts if the drive is mounted. Falls back to asking the
// drive's CDirectory. (path in edi; FileIO.cpp)
BOOL PathExists(const char *path, BOOL directory);   // 0x0002FB84
