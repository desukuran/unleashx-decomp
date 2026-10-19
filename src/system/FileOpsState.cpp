// The state of the running file operation (see FileOps.h), kept apart from
// FileOps.cpp so the parts that only report progress (the folder walk, the
// FTP client) build on their own.
#include "FileOps.h"

BOOL g_bCancelOperation;
BOOL g_bFileOperationQueued;
BOOL g_bFileOperationActive;
BOOL g_bFileOperationPaused;
int g_nFileOperationType = 6;
BOOL g_bOperationFailed;
DWORD g_dwOperationError;
unsigned g_nOperationProgress;
unsigned g_nOperationFiles;
unsigned g_nOperationFolders;
ULONGLONG g_ullOperationBytes;
wchar_t g_szOperationFile[0x12F];
wchar_t g_szOperationTarget[0x12F];
double g_dFilePercent;
double g_dTotalPercent;
ULONGLONG g_ullOperationTotal;
ULONGLONG g_ullOperationDone;
DWORD g_dwOperationRemaining;
DWORD g_dwOperationElapsed;
DWORD g_dwOperationUnknown640;
double g_dOperationRate;
ULONGLONG g_ullFileDone;
std::string g_strOperationLog;
std::string g_strOperationSource;
std::string g_strOperationDest;
std::string g_strOperationVerb;
std::vector<SFileListItem> g_fileList;
