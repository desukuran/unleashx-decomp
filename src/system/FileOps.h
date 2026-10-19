// File operations behind menu actions and the file manager. All run with a
// progress screen. TODO: decompile
#pragma once

#include <string>

#include "../common/Platform.h"

// Set to stop a running file operation (or a menu batch).
extern BOOL g_bCancelOperation;      // 0x00277128

// State of the running file operation (copy, delete, ...), shown by the
// progress boxes.
extern BOOL g_bFileOperationQueued;      // 0x00277650 TODO: verify
extern BOOL g_bFileOperationActive;      // 0x00277120
// TODO: verify. PrepareHardDisk waits after CopyFiles until this (or
// failed/cancelled) is set, and sets it at the end: it may mean "finished".
extern BOOL g_bFileOperationPaused;      // 0x00277124
// The running (or last) operation: 0 copy, 1 move, 2 delete, 3 zip, 4 unzip,
// 5 unrar; 6 = none (FrameMove sets it back after reporting the end).
extern int g_nFileOperationType;         // 0x0026BC60
extern BOOL g_bOperationFailed;          // 0x00277648
extern DWORD g_dwOperationError;         // 0x00277600
extern unsigned g_nOperationProgress;    // 0x00277644 items done
extern unsigned g_nOperationFiles;       // 0x002775FC
extern unsigned g_nOperationFolders;     // 0x002775F8
extern ULONGLONG g_ullOperationBytes;    // 0x00277618
extern wchar_t g_szOperationFile[0x12F]; // 0x00277138 the current file
extern wchar_t g_szOperationTarget[0x12F];   // 0x00277398 where it goes (copies, moves)
extern double g_dFilePercent;            // 0x00277608 of the current file
extern double g_dTotalPercent;           // 0x00277610 0 = work it out from the counts below
extern ULONGLONG g_ullOperationTotal;    // 0x00277130 bytes to copy
extern ULONGLONG g_ullOperationDone;     // 0x00277620 bytes copied
// Where failed operations are logged ("" = no log).
extern std::string g_strOperationLog;    // 0x0026C280
void ResetFileOperationStats();          // inlined
// Counts files, folders and bytes under path into the stats above.
void CountFolder(const char *path);      // 0x000A6E7C

void CopyDVD(const char *destination);                       // 0x000A9A30 NULL = ask
void CopyFiles(const char *source, const char *destination);  // 0x000A9923
void MoveFiles(const char *source, const char *destination);  // 0x000A9812
void DeleteFolder(const char *path);                          // 0x000A9738
void ZipFiles(const char *source, const char *zipFile, int unknown);   // 0x000A963D
void UnzipFile(const char *zipFile, const char *destination); // 0x000A633E
void UnRarFile(const char *rarFile, const char *destination); // 0x000AA245
