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
