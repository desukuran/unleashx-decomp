// File and drive helpers.
#pragma once

#include <string>

#include "Platform.h"

BOOL FileExists(const char *path);                                  // 0x0009DE34 (not a directory)
BOOL DirectoryExists(const char *path);                             // 0x0009DDFC
BOOL IsDriveAvailable(char drive);                                  // 0x000A4AAD
std::string ReadTextFile(const char *path);                         // 0x000A3745
// A text file that may be UTF-16 (a byte-order mark FF FE or FE FF, both
// read as little-endian): narrowed to the current code page. Others are
// read as ReadTextFile does. Quirk: a file under 2 bytes reads as "".
// (The XDK's TitleMeta.xbx and SaveMeta.xbx.)
std::string ReadMetaFile(const char *path);                         // 0x000A446C
// A whole file in memory (FileLoad.cpp; malloc'd, the size rounded up to 2 KB; size
// gets the bytes read). An HRESULT, or the Win32 error code on a failed
// read.
HRESULT LoadFileToMemory(const char *path, void **data, DWORD *size);   // 0x00092B01
HRESULT GetDriveDevicePath(char drive, std::string &devicePath);    // 0x000A3D25
std::string GetXbePath();                                           // 0x000A3934

// Turns a Config.xml path into an absolute DOS path: relative paths are
// resolved against the dashboard's home directory. See FileUtil.cpp.
void ResolvePath(const char *path, std::string &out);               // 0x000AC73F

// Writes text to path, replacing it: as is, or (unicode) as UTF-16 with a
// byte-order mark and a closing CRLF. FALSE for empty text. (text in edi)
BOOL WriteTextFile(const char *path, const char *text, BOOL unicode);   // 0x000A5BC6
// UTF-16 with a byte-order mark and a closing CRLF; creates the folder.
// (path in eax)
BOOL WriteUnicodeTextFile(const char *path, const wchar_t *text);    // 0x000A5A6C

// A file's size in KB (unused; see FileUtil.cpp for its quirk).
DWORD GetFileSizeKB(const char *path);                                  // 0x0009E11C

// Walks the matches of a pattern in a folder, through the virtual file
// layer (so it works on any drive type).
class CFileFinder
{
public:
	CFileFinder();                                        // 0x0001331C (this in eax)
	~CFileFinder();                                       // 0x00013564 (this in esi)
	// The first match's name, or NULL. The folder gets a trailing
	// backslash; an empty pattern means "*".
	const char *FindFirst(const char *dir, const char *pattern);   // 0x00013338
	const char *FindNext();                               // 0x000134C3 (this in esi)
	void Close();                                         // 0x0001352D (this in eax)
	// The current match (the find data, read directly by the callers).
	const char *GetName() const { return (const char *)m_findData + 0x2C; }
	DWORD GetAttributes() const { return *(const DWORD *)m_findData; }

	HANDLE m_hFind;                                       // +0x000
	std::string m_strPattern;                             // +0x004
	std::string m_strDir;                                 // +0x020
	BYTE m_findData[0x140];                               // +0x03C WIN32_FIND_DATA (the name at +0x068)
	int m_nFilter;                                        // +0x17C 1 skips folders, 2 skips files
	int m_unknown180;                                     // +0x180
	void *m_pDir;                                         // +0x184 the drive's directory object
};

// The folder part of a path, without a trailing backslash (except "C:\").
// A last part without a dot counts as a file name; see FileUtil.cpp.
std::string GetPathDirectory(const char *path);                       // 0x000A4B0E
// Creates the folder and any missing ones above it ('/' counts as '\').
BOOL CreateDirectoryTree(const char *path);                           // 0x000A3F51
// Writes data to path (replacing it), creating its folder. FALSE when
// there's nothing to write.
BOOL WriteBinaryFile(const char *path, const void *data, DWORD size);   // 0x000A439A
// Fixes up a path for FATX (name length/characters). TODO: verify
// Whether name is a legal FATX file name: 1-42 characters from the allowed
// set (characters >= 0x80 pass too).
BOOL IsFatxName(const char *name);                                    // 0x0009F0CA
// If the file name in path isn't FATX-legal: out = path with disallowed
// characters removed and the name shortened to 42 characters, and TRUE.
// Otherwise FALSE and out is left alone.
BOOL MakeFatxPath(const char *path, std::string &out);                // 0x000A4630
// A folder that survives reboots: the save game name on the utility drive
// U: (else X:), made with creation (0: OPEN_ALWAYS; with bit 0 set the old
// one is deleted first). With a trailing backslash; "" when it can't be
// made.
std::string GetPersistentDataPath(const wchar_t *name, unsigned int creation);   // 0x000A6D44

// The file name part of path (after the last '\\'); "" if requireExtension
// and the path has no '.'.
std::string GetPathFileName(const char *path, BOOL requireExtension);   // 0x000A4C4C
