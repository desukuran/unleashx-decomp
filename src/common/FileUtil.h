// File and drive helpers.
#pragma once

#include <string>

#include "Platform.h"

BOOL FileExists(const char *path);                                  // 0x0009DE34 (not a directory)
BOOL DirectoryExists(const char *path);                             // 0x0009DDFC
BOOL IsDriveAvailable(char drive);                                  // 0x000A4AAD
std::string ReadTextFile(const char *path);                         // 0x000A3745
HRESULT GetDriveDevicePath(char drive, std::string &devicePath);    // 0x000A3D25
std::string GetXbePath();                                           // 0x000A3934

// Turns a Config.xml path into an absolute DOS path: relative paths are
// resolved against the dashboard's home directory. See FileUtil.cpp.
void ResolvePath(const char *path, std::string &out);               // 0x000AC73F

// Writes text to path (append: add to the end instead of replacing). Returns
// FALSE for empty text.
BOOL WriteTextFile(const char *path, const char *text, BOOL append);   // 0x000A5BC6 TODO: decompile

// Walks the matches of a pattern in a folder, through the virtual file
// layer (so it works on any drive type). TODO: decompile
class CFileFinder
{
public:
	CFileFinder();                                        // 0x0001331C (this in eax)
	~CFileFinder();                                       // 0x00013564 (this in esi)
	// The first match's name, or NULL. The folder gets a trailing
	// backslash; an empty pattern means "*".
	const char *FindFirst(const char *dir, const char *pattern);   // 0x00013338
	const char *FindNext();                               // 0x000134C3
	void Close();                                         // 0x0001352D (this in eax)
	// The current match (the find data, read directly by the callers).
	const char *GetName() const { return (const char *)m_findData + 0x2C; }
	DWORD GetAttributes() const { return *(const DWORD *)m_findData; }

	HANDLE m_hFind;                                       // +0x000
	std::string m_strPattern;                             // +0x004
	std::string m_strDir;                                 // +0x020
	BYTE m_findData[0x140];                               // +0x03C WIN32_FIND_DATA (the name at +0x068)
	int m_nFilter;                                        // +0x17C 1 skips folders, 2 skips files. TODO: verify
	int m_unknown180;                                     // +0x180
	void *m_pDir;                                         // +0x184 the drive's directory object
};

// Directory part of a path to a file ("" if path has no file name).          TODO: verify
std::string GetPathDirectory(const char *path);                       // 0x000A4B0E TODO: decompile
BOOL CreateDirectoryTree(const char *path);                           // 0x000A3F51 TODO: decompile
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
// Per-title persistent folder for name (flags as XapiMapLetterToDirectory). TODO: verify
std::string GetPersistentDataPath(const wchar_t *name, unsigned int flags);   // 0x000A6D44 TODO: decompile

// The file name part of path (after the last '\\'); "" if requireExtension
// and the path has no '.'.
std::string GetPathFileName(const char *path, BOOL requireExtension);   // 0x000A4C4C TODO: decompile
