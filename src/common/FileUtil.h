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

// Directory part of a path to a file ("" if path has no file name).          TODO: verify
std::string GetPathDirectory(const char *path);                       // 0x000A4B0E TODO: decompile
BOOL CreateDirectoryTree(const char *path);                           // 0x000A3F51 TODO: decompile
// Fixes up a path for FATX (name length/characters). TODO: verify
// The path with its file name made FATX-legal (allowed characters, at most
// 42); returns TRUE if it had to be changed.
BOOL MakeFatxPath(const char *path, std::string &out);                // 0x000A4630 TODO: decompile
// Per-title persistent folder for name (flags as XapiMapLetterToDirectory). TODO: verify
std::string GetPersistentDataPath(const wchar_t *name, unsigned int flags);   // 0x000A6D44 TODO: decompile
