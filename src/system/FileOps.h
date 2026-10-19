// File operations behind menu actions and the file manager. All run with a
// progress screen. TODO: decompile
#pragma once

#include "../common/Platform.h"

// Set to stop a running file operation (or a menu batch).
extern BOOL g_bCancelOperation;      // 0x00277128

void CopyDVD(const char *destination);                       // 0x000A9A30 NULL = ask
void CopyFiles(const char *source, const char *destination);  // 0x000A9923
void MoveFiles(const char *source, const char *destination);  // 0x000A9812
void DeleteFolder(const char *path);                          // 0x000A9738
void ZipFiles(const char *source, const char *zipFile, int unknown);   // 0x000A963D
void UnzipFile(const char *zipFile, const char *destination); // 0x000A633E
void UnRarFile(const char *rarFile, const char *destination); // 0x000AA245
