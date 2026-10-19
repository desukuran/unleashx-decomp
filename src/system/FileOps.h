// File operations behind menu actions and the file manager (FileOps.cpp):
// copy, move, delete, zip, unzip, unrar and "copy DVD". Each runs on its own
// thread, one at a time, with the progress state below for the progress
// boxes and a log of what went wrong (g_strOperationLog).
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "../common/XboxKernel.h"

// Set to stop a running file operation (or a menu batch).
extern BOOL g_bCancelOperation;      // 0x00277128

// State of the running file operation (copy, delete, ...), shown by the
// progress boxes.
// Set by an operation's thread while it runs (the next one waits).
extern BOOL g_bFileOperationQueued;      // 0x00277650
// Set while files are being listed and copied (the progress box shows).
extern BOOL g_bFileOperationActive;      // 0x00277120
// Set when an operation has finished (or failed to start). TODO: rename
// (it means "finished", not "paused")
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
// The clock of the running operation (DrawProgressBox), in seconds.
extern DWORD g_dwOperationRemaining;     // 0x00277638
extern DWORD g_dwOperationElapsed;       // 0x0027763C
extern DWORD g_dwOperationUnknown640;    // 0x00277640 GetTickCount() when the first file started
extern double g_dOperationRate;          // 0x00277630 KB per second
extern ULONGLONG g_ullFileDone;          // 0x00277628 bytes of the current file
// The operation's arguments ('/' turned into '\').
extern std::string g_strOperationSource; // 0x0026C29C
extern std::string g_strOperationDest;   // 0x0026C2B8
extern std::string g_strOperationVerb;   // 0x0026C264 "Moving" (set, never read)
// Where failed operations are logged ("" = no log).
extern std::string g_strOperationLog;    // 0x0026C280
void ResetFileOperationStats();          // inlined
// Adds "hh:mm:ss<TAB>text<CR><LF>" to the log (clear: start a new log).
void LogOperation(const char *text, BOOL clear);   // 0x000A416E
// The CopyFileEx progress callback of the operations: per-file and total
// percentages, elapsed and remaining time, rate. Cancels on
// g_bCancelOperation.
DWORD __stdcall OperationProgress(LARGE_INTEGER totalSize, LARGE_INTEGER totalTransferred, LARGE_INTEGER streamSize,
                                  LARGE_INTEGER streamTransferred, DWORD streamNumber, DWORD reason, HANDLE source,
                                  HANDLE destination, void *data);   // 0x0009E74C
// Counts files, folders and bytes under path into the stats above.
void CountFolder(const char *path);      // 0x000A6E7C

void CopyDVD(const char *destination);                       // 0x000A9A30 NULL = ask
void CopyFiles(const char *source, const char *destination);  // 0x000A9923
void MoveFiles(const char *source, const char *destination);  // 0x000A9812
void DeleteFolder(const char *path);                          // 0x000A9738
void ZipFiles(const char *source, const char *zipFile, int unknown);   // 0x000A963D
void UnzipFile(const char *zipFile, const char *destination); // 0x000A633E
void UnRarFile(const char *rarFile, const char *destination); // 0x000AA245

// A file or folder an operation works through. 0x24 bytes.
struct SFileListItem
{
	SFileListItem(const std::string &path, BOOL isFolder, DWORD size);   // 0x000132B3 (this in esi)

	std::string path;               // +0x00
	BOOL isFolder;                  // +0x1C
	DWORD size;                     // +0x20 (the low 32 bits)
};
// The running operation's list.
extern std::vector<SFileListItem> g_fileList;   // 0x002788E8

// Adds everything under path to list (NULL = g_fileList): each folder's
// files, then each subfolder (itself, then what's in it), counting them in
// the operation stats. Stops on g_bCancelOperation. (FileList.cpp; LTCG
// copies at 0x00084186, 0x000A6436, 0x000F5B4B, 0x0010A5C6, 0x001292EF.)
void CollectFiles(const char *path, std::vector<SFileListItem> *list);   // 0x000F5B4B
