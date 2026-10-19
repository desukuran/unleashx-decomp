// The file operations: copy, move, delete, zip, unzip, unrar and "copy
// DVD". See FileOps.h.
//
// Each entry point stores its arguments, checks them and starts a thread.
// A thread waits for the one before it, lists the files (CollectFiles),
// checks the free space and works through the list, logging failures
// (and renames and skips) to g_strOperationLog; a log with nothing but its
// BEGIN line is dropped at the end.
#include <cstring>
#include <cwchar>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../settings/AppSettings.h"
#include "../thirdparty/Unrar.h"
#include "../thirdparty/XUnzip.h"
#include "../thirdparty/XZip.h"
#include "Drives.h"
#include "FileIO.h"
#include "FileOps.h"
#include "Storage.h"
#include "SystemControl.h"

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

// OperationProgress's state.
static DWORD s_dwLastSecond;            // 0x00278DB0
static ULONGLONG s_ullLastTransferred;  // 0x00278DB8

// The settings the operations read. 0x002788E4
static CAppSettings *Settings() { return CAppSettings::GetInstance(); }

// The paths' wide copies for the progress box. (inlined)
static void ShowOperationFile(const std::string &path)
{
	MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, g_szOperationFile, 0x12F);
}
static void ShowOperationTarget(const std::string &path)
{
	MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, g_szOperationTarget, 0x12F);
}

// (inlined, TODO: verify which counters the original clears)
void ResetFileOperationStats()
{
	g_ullOperationTotal = 0;
	g_ullOperationBytes = 0;
	g_nOperationFiles = 0;
	g_nOperationFolders = 0;
}

// 0x000A416E
void LogOperation(const char *text, BOOL clear)
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	std::string line;
	StrFormat(line, "%02d:%02d:%02d\t%s\r\n", now.wHour, now.wMinute, now.wSecond, text);
	if (clear)
		g_strOperationLog.erase();
	g_strOperationLog.append(line);
}

// 0x0009E74C
DWORD __stdcall OperationProgress(LARGE_INTEGER totalSize, LARGE_INTEGER totalTransferred,
                                  LARGE_INTEGER /*streamSize*/, LARGE_INTEGER /*streamTransferred*/,
                                  DWORD /*streamNumber*/, DWORD reason, HANDLE /*source*/, HANDLE /*destination*/,
                                  void * /*data*/)
{
	ULONGLONG transferred = (ULONGLONG)totalTransferred.QuadPart;
	if (reason == 0)   // CALLBACK_CHUNK_FINISHED
	{
		g_dwOperationElapsed = (GetTickCount() - g_dwOperationUnknown640) / 1000;
		if (g_ullOperationBytes == 0)
		{
			g_dwOperationRemaining = 0;
		}
		else
		{
			g_ullFileDone = transferred;
			g_dFilePercent = (double)(transferred * 100) / (double)totalSize.QuadPart;
			ULONGLONG total = g_ullOperationDone * 100 / g_ullOperationBytes;
			g_dTotalPercent = (double)(total > 100 ? 100 : total);
			if (g_dwOperationElapsed != 0 && g_dwOperationElapsed != s_dwLastSecond)
			{
				// Once a second: time left from the average rate.
				s_dwLastSecond = g_dwOperationElapsed;
				double done = (double)g_ullOperationDone;
				double left = (double)(g_ullOperationBytes - g_ullOperationDone);
				g_dwOperationRemaining = (DWORD)(left / (done / (double)s_dwLastSecond + 1.0) + 1.0);
				g_dOperationRate = done / (double)(s_dwLastSecond << 10);
			}
		}
		g_ullOperationDone += transferred - s_ullLastTransferred;
		s_ullLastTransferred = transferred;
	}
	else if (reason == 1)   // CALLBACK_STREAM_SWITCH: a new file
	{
		g_nOperationProgress++;
		g_dFilePercent = 0.0;
		s_ullLastTransferred = transferred;
		if (g_ullOperationDone == 0)
			g_dwOperationUnknown640 = GetTickCount();
		g_ullFileDone = 0;
	}
	return g_bCancelOperation ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}

// 0x000A6E7C (and the other CountFolder thunk 0x000A6E73)
void CountFolder(const char *path)
{
	while (g_bFileOperationActive)
		Sleep(50);
	g_bFileOperationActive = TRUE;
	g_fileList.clear();
	CollectFiles(path, NULL);
	g_bFileOperationActive = FALSE;
}

// The start of every thread: waits for the running operation, then takes
// the arguments ('*' removed). (inlined)
static void BeginOperation(int type, std::string &source, std::string *destination)
{
	g_bFileOperationQueued = TRUE;
	while (g_bFileOperationActive)
		Sleep(50);
	Sleep(500);
	g_nFileOperationType = type;
	g_strOperationLog.erase();
	memset(g_szOperationTarget, 0, sizeof(g_szOperationTarget));
	StrReplaceAll(g_strOperationSource, "*", "");
	source = g_strOperationSource;
	if (destination != NULL)
	{
		StrReplaceAll(g_strOperationDest, "*", "");
		*destination = g_strOperationDest;
	}
}

// Clears the counters before a copy or move. (inlined)
static void ResetCounters()
{
	g_dFilePercent = 0.0;
	g_ullOperationTotal = 0;
	g_dTotalPercent = 0.0;
	g_ullOperationBytes = 0;
	g_ullOperationDone = 0;
	g_bFileOperationPaused = FALSE;
	g_bCancelOperation = FALSE;
	g_nOperationFolders = 0;
	g_nOperationFiles = 0;
	g_nOperationProgress = 0;
	g_fileList.clear();
}

// For a single file: the destination's folder and file name. A
// destination without a dot after its last backslash is a folder (the file
// keeps its name); otherwise it names the file. (inlined in Copy, Move
// and Zip)
static void SplitDestination(const std::string &source, std::string &destination, std::string &name)
{
	int slash = StrReverseFind(destination, "\\", -1);
	if (slash == -1)
		slash = 0;
	if (StrFind(destination, ".", slash) == -1)
	{
		if (StrRight(destination, 1).compare("\\") != 0)
			destination.append("\\");
		slash = StrReverseFind(source, "\\", -1);
		name = StrMid(source, slash, (int)source.size() - slash);
		StrReplaceAll(name, "\\", "");
	}
	else
	{
		slash = StrReverseFind(destination, "\\", -1);
		name = StrMid(destination, slash, (int)destination.size() - slash);
		StrReplaceAll(name, "\\", "");
		StrReplaceAll(destination, name.c_str(), "");
	}
}

// The destination drive has room for g_ullOperationBytes; else the error
// (3 not mounted, 112 full). (inlined in Copy and Move)
static DWORD CheckFreeSpace(const std::string &destination)
{
	SDriveInfo *info = GetDriveInfo(StrLeft(destination, 1)[0]);
	if (info == NULL)
		return 0;
	if (!info->mounted)
		return 3;   // ERROR_PATH_NOT_FOUND
	if (g_ullOperationBytes > info->freeBytes)
		return 112;   // ERROR_DISK_FULL
	return 0;
}

// Ends an operation's log: kept when something failed, was renamed or
// skipped (with an END line), dropped otherwise. (inlined in the threads)
static void EndLog(const char *endLine, BOOL anyError, BOOL softError, BOOL logged)
{
	// (A plain error turns into "failed" only when it was a soft one.)
	if (!g_bOperationFailed && (!anyError || (g_bOperationFailed = softError) == FALSE) && !logged)
		g_strOperationLog.erase();
	else
		LogOperation(endLine, FALSE);
}

// Starts a thread for an operation (suspended, then raised a little).
// (inlined in the entry points)
static void StartOperationThread(unsigned (__stdcall *thread)(void *), void *param)
{
	HANDLE handle = (HANDLE)_beginthreadex(NULL, 0, thread, param, CREATE_SUSPENDED, NULL);
	if (handle != NULL && handle != INVALID_HANDLE_VALUE)
	{
		SetThreadPriority(handle, THREAD_PRIORITY_ABOVE_NORMAL);
		ResumeThread(handle);
		Sleep(5);
		CloseHandle(handle);
	}
	Sleep(50);
}

// Fails an operation before it starts. (inlined)
static void FailOperation(DWORD error)
{
	g_dwOperationError = error;
	SetLastError(error);
	g_bOperationFailed = TRUE;
	g_bFileOperationPaused = TRUE;
}

// Stores source and destination and starts thread, unless one is empty or
// they're the same. (inlined in Copy, Move, Zip, Unzip and UnRar)
static void StartTwoPathOperation(int type, const char *source, const char *destination,
                                  unsigned (__stdcall *thread)(void *), void *param)
{
	while (g_bFileOperationQueued)
		Sleep(500);
	g_nFileOperationType = type;
	g_strOperationSource = source;
	g_strOperationDest = destination;
	StrReplaceAll(g_strOperationSource, "/", "\\");
	StrReplaceAll(g_strOperationDest, "/", "\\");
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	g_bCancelOperation = FALSE;
	if (g_strOperationSource.size() != 0 && g_strOperationDest.size() != 0 &&
	    StrCmpNoCase(g_strOperationSource, g_strOperationDest.c_str()) != 0)
	{
		StartOperationThread(thread, param);
		return;
	}
	FailOperation(3);   // ERROR_PATH_NOT_FOUND
}

// ---- copy ----

// 0x000A88D8
static unsigned __stdcall CopyThread(void *)
{
	std::string source, destination;
	BeginOperation(0, source, &destination);
	if (destination.size() == 0)
	{
		g_dwOperationError = 3;
		SetLastError(3);
		g_bOperationFailed = TRUE;
		g_bFileOperationActive = FALSE;
		return 1;
	}
	ResetCounters();
	CDirectory *dir = CreateDirObject(source.c_str());
	if (dir == NULL)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	if (dir->FileExists(source.c_str()))
	{
		// One file.
		std::string name;
		SplitDestination(source, destination, name);
		CreateDirectoryTree(destination.c_str());
		destination.append(name);
		MultiByteToWideChar(CP_ACP, 0, source.c_str(), (int)source.size(), g_szOperationFile, 0x12F);
		MultiByteToWideChar(CP_ACP, 0, destination.c_str(), (int)destination.size(), g_szOperationTarget, 0x12F);
		Sleep(1);
		if (Settings()->m_prefs.m_bAutoRename)
			MakeFatxPath(destination.c_str(), destination);
		if (!dir->CopyFileEx(source.c_str(), destination.c_str(), OperationProgress, NULL, NULL, 0))
			g_bOperationFailed = TRUE;
		g_bFileOperationActive = FALSE;
		g_bFileOperationPaused = TRUE;
		RefreshDrives(TRUE);
		g_bFileOperationQueued = FALSE;
		delete dir;
		return g_bOperationFailed != FALSE;
	}

	// A folder: list it first.
	g_bFileOperationActive = TRUE;
	BOOL led = SetLedPattern(Settings()->m_prefs.m_nLEDBusy, FALSE);
	CollectFiles(source.c_str(), NULL);
	if (led)
		SetLedPattern(LED_DEFAULT, FALSE);
	if (g_bCancelOperation)
	{
		g_bFileOperationActive = FALSE;
		return 0;   // (dir is leaked)
	}
	DWORD spaceError = CheckFreeSpace(destination);
	if (spaceError != 0)
	{
		g_dwOperationError = spaceError;
		SetLastError(spaceError);
		g_bOperationFailed = TRUE;
		g_bFileOperationActive = FALSE;
		g_bFileOperationPaused = TRUE;
		delete dir;
		return 1;
	}

	BOOL anyError = FALSE;
	g_bCancelOperation = FALSE;
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	if (StrRight(source, 1).compare("\\") != 0)
		source.append("\\");
	if (StrRight(destination, 1).compare("\\") != 0)
		destination.append("\\");
	CreateDirectoryTree(destination.c_str());
	std::string to, from, message;
	BOOL softError = FALSE;
	BOOL logged = FALSE;
	g_dwOperationError = 0;
	SetLastError(0);
	BOOL fromDvd = StrCmpNoCase(StrLeft(g_strOperationSource, 2).c_str(), "D:") == 0;
	led = SetLedPattern(Settings()->m_prefs.m_nLEDFileCopy, FALSE);
	Sleep(5);
	StrFormat(message, "BEGIN Copy from \"%s\" to \"%s\"", source.c_str(), destination.c_str());
	LogOperation(message.c_str(), TRUE);

	for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end(); ++it)
	{
		if (g_bCancelOperation)
		{
			LogOperation("Copy Operation Canceled", FALSE);
			break;
		}
		SFileListItem item = *it;
		from = item.path;
		to = item.path;
		StrReplaceAll(to, source.c_str(), destination.c_str());
		if (Settings()->m_prefs.m_bAutoRename && MakeFatxPath(to.c_str(), to))
		{
			StrFormat(message, "Renamed \"%s\" to \"%s\"", from.c_str(), to.c_str());
			LogOperation(message.c_str(), FALSE);
			logged = TRUE;
		}

		if (item.isFolder)
		{
			// (Asks the source drive's object about the destination.)
			if (dir->DirectoryExists(to.c_str()))
				continue;
			if (!CreateDirectoryTree(to.c_str()) && (g_dwOperationError = GetLastError()) != 0)
			{
				StrFormat(message, "Unable to create folder \"%s\". Error Code: %d", to.c_str(),
				          (int)g_dwOperationError);
				LogOperation(message.c_str(), FALSE);
				g_bOperationFailed = g_dwOperationError != 0;
				logged = TRUE;
				anyError = TRUE;
				if (g_bOperationFailed)
					break;
			}
			Sleep(1);
			continue;
		}

		WIN32_FIND_DATAA data;
		HANDLE find = FindFirstFileA(to.c_str(), &data);
		if (find != INVALID_HANDLE_VALUE)
		{
			BOOL same = data.nFileSizeLow == item.size;
			CloseHandle(find);
			if (same)
			{
				// Already there (by its size).
				StrFormat(message, "Skipped \"%s\". File is the same.", from.c_str());
				LogOperation(message.c_str(), FALSE);
				logged = TRUE;
				continue;
			}
		}
		else if (fromDvd && (StrCmpNoCase(from.c_str(), "D:\\dashupdate.xbe") == 0 ||
		                     StrCmpNoCase(from.c_str(), "D:\\update.xbe") == 0 ||
		                     StrCmpNoCase(from.c_str(), "D:\\downloader.xbe") == 0))
		{
			// The dashboard updaters on game discs aren't copied.
			StrFormat(message, "Skipped \"%s\"", from.c_str());
			LogOperation(message.c_str(), FALSE);
			continue;
		}

		memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
		ShowOperationFile(from);
		ShowOperationTarget(to);
		BOOL copied = FALSE;
		for (int tries = 3; tries > 0; tries--)
		{
			g_ullFileDone = 0;
			if (dir->CopyFileEx(from.c_str(), to.c_str(), OperationProgress, NULL, NULL, 0))
			{
				SetFileAttributesA(to.c_str(), FILE_ATTRIBUTE_NORMAL);
				copied = TRUE;
				break;
			}
			// Take the failed try's bytes off the total.
			Sleep(1);
			g_ullOperationDone -= g_ullFileDone;
		}
		if (copied)
			continue;
		g_dwOperationError = GetLastError();
		StrFormat(message, "Copy FAILED \"%s\". Error Code: %d", from.c_str(), (int)g_dwOperationError);
		LogOperation(message.c_str(), FALSE);
		anyError = TRUE;
		logged = TRUE;
		if (Settings()->m_prefs.m_bAbortOnError)
		{
			g_bOperationFailed = TRUE;
			break;
		}
		softError = TRUE;
	}
	delete dir;
	g_bFileOperationActive = FALSE;
	g_bFileOperationPaused = TRUE;

	// A game disc's XBEs, patched to run from the hard disk.
	if (!anyError && Settings()->m_prefs.m_bAutoPatch && StrCmpNoCase(source.c_str(), "D:\\") == 0)
	{
		std::string folder = destination;
		std::string pattern = destination;
		if (StrRight(pattern, 1).compare("\\") == 0)
		{
			pattern.append("*.xbe");
		}
		else
		{
			pattern.append("\\*.xbe");
			folder.append("\\");
		}
		WIN32_FIND_DATAA data;
		HANDLE find = FindFirstFileA(pattern.c_str(), &data);
		if (find != INVALID_HANDLE_VALUE)
		{
			BOOL more;
			do
			{
				std::string xbe, backup;
				StrFormat(xbe, "%s%s", folder.c_str(), data.cFileName);
				backup = StrLeft(xbe, (int)xbe.size() - 3);
				backup.append("bak");
				CopyFileA(xbe.c_str(), backup.c_str(), TRUE);
				if (!PatchXbe(xbe.c_str()))
				{
					DeleteFileA(backup.c_str());
				}
				else
				{
					StrFormat(message, "\"%s\" Patched successfully. Back up created as \"%s\"", xbe.c_str(),
					          backup.c_str());
					LogOperation(message.c_str(), FALSE);
					logged = TRUE;
				}
				more = FindNextFileA(find, &data) == TRUE;
			} while (more);
			CloseHandle(find);
		}
	}

	RefreshDrives(TRUE);
	EndLog("END Copy Operation", anyError, softError, logged);
	if (led)
		SetLedPattern(LED_DEFAULT, FALSE);
	g_bFileOperationQueued = FALSE;
	return anyError;
}

// 0x000A9923
void CopyFiles(const char *source, const char *destination)
{
	g_dwOperationError = 0;
	SetLastError(0);
	StartTwoPathOperation(0, source, destination, CopyThread, NULL);
}

// ---- move ----

// 0x000A79FF
static unsigned __stdcall MoveThread(void *)
{
	std::string source, destination;
	BeginOperation(1, source, &destination);
	g_strOperationVerb = "Moving";
	if (destination.size() == 0)
	{
		g_dwOperationError = 3;
		SetLastError(3);
		g_bFileOperationActive = FALSE;
		g_bOperationFailed = TRUE;
		return 1;
	}
	ResetCounters();
	g_dwOperationError = 0;

	if (FileExists(source.c_str()))
	{
		// One file.
		std::string name;
		SplitDestination(source, destination, name);
		CreateDirectoryTree(destination.c_str());
		destination.append(name);
		ShowOperationFile(source);
		ShowOperationTarget(destination);
		Sleep(1);
		if (Settings()->m_prefs.m_bAutoRename)
			MakeFatxPath(destination.c_str(), destination);
		if (!MoveFileExA(source.c_str(), destination.c_str(), 3))   // REPLACE_EXISTING | COPY_ALLOWED
			g_bOperationFailed = TRUE;
		g_bFileOperationActive = FALSE;
		g_bFileOperationPaused = TRUE;
		RefreshDrives(TRUE);
		g_bFileOperationQueued = FALSE;
		return g_bOperationFailed != FALSE;
	}

	g_bFileOperationActive = TRUE;
	CollectFiles(source.c_str(), NULL);
	if (g_bCancelOperation)
	{
		g_bFileOperationActive = FALSE;
		return 0;
	}
	DWORD spaceError = CheckFreeSpace(destination);
	if (spaceError != 0)
	{
		g_dwOperationError = spaceError;
		SetLastError(spaceError);
		g_bFileOperationPaused = TRUE;
		g_bFileOperationActive = FALSE;
		g_bOperationFailed = TRUE;
		return 1;
	}

	BOOL anyError = FALSE;
	g_bCancelOperation = FALSE;
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	if (StrRight(source, 1).compare("\\") != 0)
		source.append("\\");
	if (StrRight(destination, 1).compare("\\") != 0)
		destination.append("\\");
	// The folder moves into the destination, unless that already ends
	// with its name.
	int slash = StrReverseFind(source, "\\", (int)source.size() - 2);
	if (slash != -1)
	{
		std::string folder = StrMid(source, slash + 1, (int)source.size() - (slash + 1));
		if (folder.size() != 0 &&
		    StrCmpNoCase(folder.c_str(), StrRight(destination, (int)folder.size()).c_str()) != 0)
			destination.append(folder);
	}
	CreateDirectoryTree(destination.c_str());
	std::string to, from, message;
	BOOL otherDrive = StrCmpNoCase(StrLeft(source, 1).c_str(), StrLeft(destination, 1).c_str()) != 0;
	BOOL softError = FALSE;
	BOOL logged = FALSE;
	g_dwOperationError = 0;
	SetLastError(0);
	BOOL led = SetLedPattern(Settings()->m_prefs.m_nLEDBusy, FALSE);
	Sleep(5);
	StrFormat(message, "BEGIN Move Operation from \"%s\" to \"%s\"", source.c_str(), destination.c_str());
	LogOperation(message.c_str(), TRUE);

	for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end(); ++it)
	{
		if (g_bCancelOperation)
		{
			LogOperation("Move Operation Canceled", FALSE);
			break;
		}
		SFileListItem item = *it;
		from = item.path;
		to = item.path;
		StrReplaceAll(to, source.c_str(), destination.c_str());
		DWORD flags = item.isFolder ? 2 : 3;   // COPY_ALLOWED (| REPLACE_EXISTING for files)
		SetFileAttributesA(from.c_str(), FILE_ATTRIBUTE_NORMAL);

		if (item.isFolder)
		{
			// On the same drive folders aren't made (TODO: verify how the
			// files in subfolders get there; the moves would need them).
			if (!otherDrive)
				continue;
			if (CreateDirectoryTree(to.c_str()))
			{
				Sleep(1);
				continue;
			}
			g_dwOperationError = GetLastError();
			StrFormat(message, "Move FAILED \"%s\". Error Code: %d", to.c_str(), (int)g_dwOperationError);
			LogOperation(message.c_str(), FALSE);
			anyError = TRUE;
			logged = TRUE;
			g_bOperationFailed = TRUE;
			break;
		}

		memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
		ShowOperationFile(from);
		ShowOperationTarget(to);
		g_ullFileDone = 0;
		if (!MoveFileWithProgressA(from.c_str(), to.c_str(), (void *)OperationProgress, NULL, flags))
		{
			g_ullOperationDone -= g_ullFileDone;
			g_dwOperationError = GetLastError();
			StrFormat(message, "Move FAILED \"%s\". Error Code: %d", from.c_str(), (int)g_dwOperationError);
			LogOperation(message.c_str(), FALSE);
			logged = TRUE;
			anyError = TRUE;
			if (Settings()->m_prefs.m_bAbortOnError)
			{
				g_bOperationFailed = TRUE;
				break;
			}
			softError = TRUE;
		}
	}

	// Across drives the emptied folders go, deepest first.
	if (otherDrive && !g_bOperationFailed)
	{
		Sleep(5);
		for (int i = (int)g_fileList.size() - 1; i >= 0; i--)
		{
			if (g_bCancelOperation)
				break;
			if (g_fileList[i].isFolder && !RemoveDirectoryA(g_fileList[i].path.c_str()))
				Sleep(1);
		}
	}
	if (!g_bOperationFailed && !softError)
		RemoveDirectoryA(source.c_str());
	g_bFileOperationActive = FALSE;
	g_bFileOperationPaused = TRUE;
	RefreshDrives(TRUE);
	EndLog("END Move Operation", anyError, softError, logged);
	if (led)
		SetLedPattern(LED_DEFAULT, FALSE);
	g_bFileOperationQueued = FALSE;
	return anyError;
}

// 0x000A9812
void MoveFiles(const char *source, const char *destination)
{
	g_dwOperationError = 0;
	SetLastError(0);
	StartTwoPathOperation(1, source, destination, MoveThread, NULL);
}

// ---- delete ----

// 0x000A8365
static unsigned __stdcall DeleteThread(void *)
{
	std::string source;
	BeginOperation(2, source, NULL);
	g_dFilePercent = 0.0;
	g_dTotalPercent = 0.0;
	g_ullOperationTotal = 0;
	g_ullOperationBytes = 0;
	g_ullOperationDone = 0;
	g_bFileOperationPaused = FALSE;
	g_bCancelOperation = FALSE;
	g_nOperationFolders = 0;
	g_nOperationFiles = 0;
	g_dwOperationError = 0;
	g_nOperationProgress = 0;
	g_fileList.clear();

	if (FileExists(source.c_str()))
	{
		// One file.
		ShowOperationFile(source);
		Sleep(1);
		if (!DeleteFileA(source.c_str()))
			g_bOperationFailed = TRUE;
		g_bFileOperationQueued = FALSE;
		g_bFileOperationActive = FALSE;
		g_bFileOperationPaused = TRUE;
		RefreshDrives(TRUE);
		return g_bOperationFailed != FALSE;
	}
	if (!DirectoryExists(source.c_str()))
	{
		// (g_bFileOperationQueued stays set.)
		g_dwOperationError = 3;
		SetLastError(3);
		g_bOperationFailed = TRUE;
		g_bFileOperationActive = FALSE;
		return TRUE;
	}

	g_bFileOperationActive = TRUE;
	CollectFiles(source.c_str(), NULL);
	if (g_bCancelOperation)
	{
		g_bFileOperationActive = FALSE;
		return FALSE;
	}
	g_bCancelOperation = FALSE;
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	BOOL logged = FALSE;
	BOOL softError = FALSE;
	std::string message;
	SetLastError(0);
	g_dwOperationError = 0;
	BOOL led = SetLedPattern(Settings()->m_prefs.m_nLEDBusy, FALSE);
	Sleep(5);
	StrFormat(message, "BEGIN Delete from \"%s\"", source.c_str());
	LogOperation(message.c_str(), TRUE);

	// The files...
	for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end(); ++it)
	{
		if (g_bCancelOperation)
		{
			LogOperation("Delete Operation Canceled", FALSE);
			break;
		}
		SFileListItem item = *it;
		if (item.isFolder)
			continue;
		ShowOperationFile(item.path);
		Sleep(1);
		SetFileAttributesA(item.path.c_str(), FILE_ATTRIBUTE_NORMAL);
		if (DeleteFileA(item.path.c_str()))
		{
			g_ullOperationDone += item.size;
			g_nOperationProgress++;
			continue;
		}
		g_dwOperationError = GetLastError();
		StrFormat(message, "Delete FAILED \"%s\". Error Code: %d", item.path.c_str(), (int)g_dwOperationError);
		LogOperation(message.c_str(), FALSE);
		logged = TRUE;
		if (Settings()->m_prefs.m_bAbortOnError)
		{
			g_bOperationFailed = TRUE;
			break;
		}
		softError = TRUE;
	}

	// ...then the folders, deepest first, then the folder itself.
	if (!g_bOperationFailed)
	{
		Sleep(5);
		for (int i = (int)g_fileList.size() - 1; i >= 0; i--)
		{
			if (g_bCancelOperation)
				break;
			SFileListItem &item = g_fileList[i];
			if (item.path.size() <= 1 || !item.isFolder)
				continue;
			ShowOperationFile(item.path);
			SetFileAttributesA(item.path.c_str(), FILE_ATTRIBUTE_NORMAL);
			Sleep(1);
			if (!RemoveDirectoryA(item.path.c_str()))
			{
				g_dwOperationError = GetLastError();
				StrFormat(message, "Delete FAILED \"%s\". Error Code: %d", item.path.c_str(),
				          (int)g_dwOperationError);
				LogOperation(message.c_str(), FALSE);
				logged = TRUE;
				if (Settings()->m_prefs.m_bAbortOnError)
				{
					g_bOperationFailed = TRUE;
					break;
				}
				softError = TRUE;
			}
			g_nOperationProgress++;
		}
		RemoveDirectoryA(source.c_str());
	}
	RefreshDrives(TRUE);
	g_bFileOperationActive = FALSE;
	g_bFileOperationPaused = TRUE;
	if (!g_bOperationFailed && (g_bOperationFailed = softError) == FALSE && !logged)
		g_strOperationLog.erase();
	else
		LogOperation("END Delete Operation", FALSE);
	if (led)
		SetLedPattern(LED_DEFAULT, FALSE);
	g_bFileOperationQueued = FALSE;
	return g_bOperationFailed != FALSE;
}

// 0x000A9738
void DeleteFolder(const char *path)
{
	while (g_bFileOperationQueued)
		Sleep(500);
	g_nFileOperationType = 2;
	g_strOperationSource = path;
	StrReplaceAll(g_strOperationSource, "/", "\\");
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	g_bCancelOperation = FALSE;
	if (g_strOperationSource.size() == 0)
	{
		g_dwOperationError = 3;
		SetLastError(3);
		g_bFileOperationPaused = TRUE;
		g_bOperationFailed = TRUE;
		return;
	}
	if (!DirectoryExists(path))
	{
		// (A file isn't deleted from here.)
		g_bFileOperationPaused = TRUE;
		g_bFileOperationQueued = FALSE;
		return;
	}
	StartOperationThread(DeleteThread, NULL);
}

// ---- zip ----

// The zip's name in the destination folder: the source's own name with
// ".zip" (any dots doubled). (inlined twice in ZipThread)
static BOOL PrepareZipName(const std::string &source, std::string &destination, BOOL oneFile)
{
	std::string name;
	int slash = StrReverseFind(destination, "\\", -1);
	if (slash == -1)
		slash = 0;
	if (StrFind(destination, ".", slash) == -1)
	{
		if (StrRight(destination, 1).compare("\\") != 0)
			destination.append("\\");
		slash = StrReverseFind(source, "\\", -1);
		name = StrMid(source, slash, (int)source.size() - slash);
		StrReplaceAll(name, "\\", "");
	}
	else
	{
		slash = StrReverseFind(destination, "\\", -1);
		name = StrMid(destination, slash, (int)destination.size() - slash);
		StrReplaceAll(name, "\\", "");
		StrReplaceAll(destination, name.c_str(), "");
	}
	if (!CreateDirectoryTree(destination.c_str()))
		return FALSE;
	int dot = StrFind(name, ".", 0);
	if (oneFile)
	{
		// A file: its extension becomes ".zip".
		if (dot != -1)
			name = StrLeft(name, dot);
		name.append(".zip");
		StrReplaceAll(name, "..", ".");
	}
	else if (dot == -1)
	{
		// A folder (a dotted name is kept as it is).
		name = StrLeft(name, -1);
		name.append(".zip");
		StrReplaceAll(name, "..", ".");
	}
	destination.append(name);
	return TRUE;
}

// 0x000A6F75 (param: TRUE to store full paths)
static unsigned __stdcall ZipThread(void *param)
{
	BOOL fullPaths = param != NULL;
	std::string source, destination;
	BeginOperation(3, source, &destination);
	Sleep(50);
	g_fileList.clear();

	if (FileExists(source.c_str()))
	{
		// One file.
		if (!PrepareZipName(source, destination, TRUE))
		{
			g_dwOperationError = 3;
			SetLastError(3);
			g_bOperationFailed = TRUE;
			g_bFileOperationActive = FALSE;
			return TRUE;
		}
		ShowOperationFile(source);
		ShowOperationTarget(destination);
		Sleep(1);
		if (!FileExists(destination.c_str()))
		{
			HZIP zip = CreateZip((void *)destination.c_str(), 0, ZIP_FILENAME);
			if (zip == NULL)
			{
				g_bOperationFailed = TRUE;
				return TRUE;   // (Active and Queued stay as they were)
			}
			// Quirk: the entry is named with the zip's own path.
			if (ZipAdd(zip, destination.c_str(), (void *)source.c_str(), 0, ZIP_FILENAME) == ZR_OK)
				Sleep(1);
			else
				g_bOperationFailed = TRUE;
			CloseAnyZip(zip);
		}
		else
		{
			g_bOperationFailed = TRUE;
		}
		g_bFileOperationActive = FALSE;
		RefreshDrives(TRUE);
		g_bFileOperationQueued = FALSE;
		return g_bOperationFailed != FALSE;
	}

	g_bFileOperationActive = TRUE;
	CollectFiles(source.c_str(), NULL);
	if (g_bCancelOperation)
	{
		g_bFileOperationActive = FALSE;
		return FALSE;
	}
	if (destination.size() == 0)
	{
		g_dwOperationError = 3;
		SetLastError(3);
		g_bOperationFailed = TRUE;
		g_bFileOperationActive = FALSE;
		return TRUE;
	}
	if (!PrepareZipName(source, destination, FALSE))
	{
		g_dwOperationError = 3;
		SetLastError(3);
		g_bOperationFailed = TRUE;
		g_bFileOperationActive = FALSE;
		return TRUE;
	}
	DeleteFileA(destination.c_str());
	BOOL softError = FALSE;
	BOOL logged = FALSE;
	std::string message;
	g_nOperationProgress = 0;
	g_bCancelOperation = FALSE;
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	g_dwOperationError = 0;
	SetLastError(0);
	Sleep(5);
	StrFormat(message, "BEGIN Zipping \"%s\"", source.c_str());
	LogOperation(message.c_str(), TRUE);
	HZIP zip = CreateZip((void *)destination.c_str(), 0, ZIP_FILENAME);
	if (zip == NULL)
		g_bOperationFailed = TRUE;
	if (!g_bOperationFailed && g_fileList.size() != 0)
	{
		BOOL led = SetLedPattern(Settings()->m_prefs.m_nLEDBusy, FALSE);
		std::string name;
		for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end(); ++it)
		{
			if (g_bCancelOperation)
			{
				LogOperation("Zip Operation Canceled", FALSE);
				break;
			}
			SFileListItem item = *it;
			if (item.isFolder)
				continue;
			ShowOperationFile(item.path);
			Sleep(1);
			// The name in the zip: relative to the source folder (or the
			// whole path).
			name = item.path;
			if (!fullPaths)
				StrReplaceAll(name, source.c_str(), "");
			if (ZipAdd(zip, name.c_str(), (void *)item.path.c_str(), 0, ZIP_FILENAME) == ZR_OK)
			{
				g_nOperationProgress++;
				continue;
			}
			g_dwOperationError = GetLastError();
			StrFormat(message, "Zip FAILED \"%s\". Error Code: %d", item.path.c_str(), (int)g_dwOperationError);
			LogOperation(message.c_str(), FALSE);
			logged = TRUE;
			if (Settings()->m_prefs.m_bAbortOnError)
			{
				g_bOperationFailed = TRUE;
				break;
			}
			softError = TRUE;
		}
		CloseAnyZip(zip);
		if (led)
			SetLedPattern(LED_DEFAULT, FALSE);
		RefreshDrives(TRUE);
	}
	g_bFileOperationActive = FALSE;
	g_bFileOperationPaused = TRUE;
	if (!g_bOperationFailed && (g_bOperationFailed = softError) == FALSE && !logged)
		g_strOperationLog.erase();
	else
		LogOperation("END Zip Operation", FALSE);
	g_bFileOperationQueued = FALSE;
	return g_bOperationFailed != FALSE;
}

// 0x000A963D (and the thunk 0x000A9631)
void ZipFiles(const char *source, const char *zipFile, int fullPaths)
{
	StartTwoPathOperation(3, source, zipFile, ZipThread, (void *)(uintptr_t)fullPaths);
}

// ---- unzip ----

// 0x000A51AC
static unsigned __stdcall UnzipThread(void *)
{
	std::string zipFile, destination;
	BeginOperation(4, zipFile, &destination);
	if (!FileExists(zipFile.c_str()))
	{
		g_dwOperationError = 2;
		SetLastError(2);
		g_bOperationFailed = TRUE;
		g_bFileOperationQueued = FALSE;
		return TRUE;
	}
	if (!CreateDirectoryTree(destination.c_str()))
	{
		g_dwOperationError = 3;
		SetLastError(3);
		g_bOperationFailed = TRUE;
		g_bFileOperationQueued = FALSE;
		return TRUE;
	}
	Sleep(50);
	g_bFileOperationPaused = FALSE;
	g_dwOperationError = 0;
	BOOL logged = FALSE;
	std::string message;
	StrFormat(message, "BEGIN Unzipping \"%s\"", zipFile.c_str());
	LogOperation(message.c_str(), TRUE);
	HZIP zip = OpenZip((void *)zipFile.c_str(), 0, ZIP_FILENAME);
	if (zip == NULL)
	{
		g_bOperationFailed = TRUE;
	}
	else
	{
		if (StrRight(destination, 1).compare("\\") != 0)
			destination.append("\\");
		ZIPENTRY entry;
		memset(&entry, 0, sizeof(entry));
		ZRESULT result = GetZipItem(zip, -1, &entry);
		g_nOperationProgress = 0;
		if (result == ZR_OK && entry.index > 0)
		{
			int count = entry.index;
			g_nOperationFiles = count;
			std::string path, folder;
			BOOL led = SetLedPattern(Settings()->m_prefs.m_nLEDBusy, FALSE);
			for (int i = 0; i < count; i++)
			{
				if (g_bCancelOperation)
				{
					LogOperation("Unzip Operation Canceled", FALSE);
					break;
				}
				memset(&entry, 0, sizeof(entry));
				if (GetZipItem(zip, i, &entry) != ZR_OK)
					continue;
				path = entry.name;
				StrReplaceAll(path, ":", "_");
				std::string full;
				StrFormat(full, "%s%s", destination.c_str(), path.c_str());
				path = full;
				StrReplaceAll(path, "\\\\", "\\");
				StrReplaceAll(path, "/", "\\");
				DeleteFileA(path.c_str());
				folder = path;
				int slash = StrReverseFind(folder, "\\", -1);
				if (slash > 3 && slash > (int)destination.size())
				{
					folder = StrLeft(folder, slash);
					CreateDirectoryTree(folder.c_str());
				}
				if (MakeFatxPath(path.c_str(), path))
				{
					// (The message names the folder, not the original name.)
					StrFormat(message, "Renamed \"%s\" to \"%s\"", folder.c_str(), path.c_str());
					LogOperation(message.c_str(), FALSE);
					logged = TRUE;
				}
				ShowOperationFile(path);
				if (UnzipItem(zip, i, (void *)path.c_str(), 0, ZIP_FILENAME) != ZR_OK)
				{
					g_dwOperationError = GetLastError();
					StrFormat(message, "Unzip FAILED \"%s\". Error Code: %d", path.c_str(), (int)g_dwOperationError);
					LogOperation(message.c_str(), FALSE);
					g_bOperationFailed = TRUE;
					break;
				}
				Sleep(50);
				SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
				g_nOperationProgress++;
			}
			if (led)
				SetLedPattern(LED_DEFAULT, FALSE);
		}
		CloseAnyZip(zip);
	}
	g_bFileOperationPaused = TRUE;
	if (!g_bOperationFailed)
	{
		if (!logged)
			g_strOperationLog.erase();
	}
	else
	{
		LogOperation("END Unzip Operation", FALSE);
	}
	g_bFileOperationQueued = FALSE;
	return g_bOperationFailed != FALSE;
}

// 0x000A633E (and the thunk 0x000A6332)
void UnzipFile(const char *zipFile, const char *destination)
{
	StartTwoPathOperation(4, zipFile, destination, UnzipThread, NULL);
}

// ---- unrar ----

// Shows each extracted file and logs failures. vtable 0x0025A988.
class USink : public UnrarSink
{
public:
	// 0x00031ABB
	virtual void OnFile(const char *name, bool ok, bool *proceed)
	{
		memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
		if (name != NULL)
			MultiByteToWideChar(CP_ACP, 0, name, (int)strlen(name), g_szOperationFile, 0x12F);
		if (!ok)
		{
			g_dwOperationError = GetLastError();
			std::string message;
			StrFormat(message, "Unrar FAILED \"%s\". Error Code: %d", name, (int)g_dwOperationError);
			LogOperation(message.c_str(), FALSE);
			g_bOperationFailed = TRUE;
			*proceed = false;
		}
		else
		{
			g_nOperationProgress++;
		}
		if (g_bCancelOperation)
		{
			LogOperation("Unrar Operation Canceled", FALSE);
			*proceed = false;
		}
	}
};

// 0x000A9F1B
static unsigned __stdcall UnRarThread(void *)
{
	std::string rarFile, destination;
	BeginOperation(5, rarFile, &destination);
	if (!FileExists(rarFile.c_str()))
	{
		g_dwOperationError = 2;
		SetLastError(2);
		g_bOperationFailed = TRUE;
		g_bFileOperationQueued = FALSE;
		return TRUE;
	}
	if (!CreateDirectoryTree(destination.c_str()))
	{
		g_dwOperationError = 3;
		SetLastError(3);
		g_bOperationFailed = TRUE;
		g_bFileOperationQueued = FALSE;
		return TRUE;
	}
	Sleep(50);
	g_bFileOperationPaused = FALSE;
	g_dwOperationError = 0;
	std::string message;
	StrFormat(message, "BEGIN Extracting \"%s\"", rarFile.c_str());
	LogOperation(message.c_str(), TRUE);
	ArchiveList_struct *list = NULL;
	int count = 0;
	try
	{
		count = urarlib_list(rarFile.c_str(), &list, NULL);
	}
	catch (...)
	{
	}
	if (list != NULL && count != 0)
	{
		g_nOperationProgress = 0;
		g_nOperationFiles = count;
		StrAddBackslash(destination);
		// An archive that starts with a folder: don't nest it twice.
		if (list->attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			std::string top = "\\";
			top.append(list->name);
			StrAddBackslash(top);
			StrReplaceAll(destination, top.c_str(), "\\");
		}
		BOOL led = SetLedPattern(Settings()->m_prefs.m_nLEDBusy, FALSE);
		USink sink;
		CreateDirectoryTree(destination.c_str());
		if (!urarlib_get(rarFile.c_str(), destination.c_str(), NULL, NULL, &sink))
			g_bOperationFailed = TRUE;
		urarlib_freelist(list);
		if (led)
			SetLedPattern(LED_DEFAULT, FALSE);
	}
	g_bFileOperationPaused = TRUE;
	if (!g_bOperationFailed)
		g_strOperationLog.erase();
	else
		LogOperation("END Unrar Operation", FALSE);
	g_bFileOperationQueued = FALSE;
	return g_bOperationFailed != FALSE;
}

// 0x000AA245 (and the thunk 0x000AA239)
void UnRarFile(const char *rarFile, const char *destination)
{
	StartTwoPathOperation(5, rarFile, destination, UnRarThread, NULL);
}

// ---- copy DVD ----

// A title usable as a folder name: up to the first ':' (when it isn't at
// the start), no symbols, nothing above '|', no spaces, at most 20
// characters. (title in eax)
static void MakeFolderName(std::string &title)   // 0x000A408F
{
	int colon = StrFind(title, ":", 0);
	if (colon > 3)
		title = StrLeft(title, colon);
	static const char s_symbols[] = "<>=?:;\"*+,/\\|";
	for (int i = 0; s_symbols[i] != 0; i++)
		StrReplaceChar(title, s_symbols[i], ' ');
	for (int c = 0x7D; c < 0x100; c++)
		StrRemoveChar(title, (char)c);
	StrReplaceAll(title, " ", "");
	if (title.size() > 20)
		title = StrLeft(title, 20);
}

// 0x000A9A30
void CopyDVD(const char *destination)
{
	SDriveInfo *dvd = GetDriveInfo('D');
	if (dvd == NULL)
		return;
	RefreshDrives(FALSE);
	if (!dvd->mounted || !dvd->hasMedia)
		return;

	// An XBE given as the "destination" is the game to copy (from its
	// folder on the disc).
	std::string target = destination != NULL ? destination : "";
	StrReplaceAll(target, "/", "\\");
	BOOL fromXbe = target.size() != 0 && StrCmpNoCase(StrRight(target, 4).c_str(), ".xbe") == 0 &&
	               FileExists(target.c_str());

	if (fromXbe || destination == NULL || target.size() == 0)
	{
		// Pick the folder: <drive>:\Games\<title>, Videos\Video or
		// Data\<disc title>.
		std::string name = "";
		std::string category = "Games";
		int disc = GetDiscType();
		if (disc == DISC_GAME || fromXbe)
		{
			EnterFileLock();
			SXbeInfo info;
			if (ReadXbeInfo(fromXbe ? destination : "D:\\default.xbe", &info))
			{
				name = info.titleName;
				MakeFolderName(name);
				if (name.size() == 0)
					StrFormat(name, "%08x", info.titleId);
			}
			LeaveFileLock();
		}
		else if (disc == DISC_VIDEO)
		{
			name = "Video";
			category = "Videos";
		}
		else if (disc == DISC_DATA || disc == DISC_DATA2)
		{
			name = StrFromWide(GetDiscTitle());
			StrTrimRight(StrTrimLeft(name));
			if (name.size() == 0)
				name = "Data";
			category = "Data";
		}
		else
		{
			FailOperation(1);   // ERROR_INVALID_FUNCTION
			return;
		}

		// The <DefaultFolder> when it's on a drive with over 1 GB free (and
		// the disc is a game, or data with something on it)...
		ULONGLONG free = 0;
		CAppSettings *settings = Settings();
		if ((disc == DISC_GAME || fromXbe || ((disc == DISC_DATA || disc == DISC_DATA2) && dvd->totalBytes != 0)) &&
		    settings->m_prefs.m_strDefaultFolder.size() != 0)
		{
			SDriveInfo *info = GetDriveInfo(StrLeft(settings->m_prefs.m_strDefaultFolder, 1)[0]);
			if (info != NULL && info->mounted && info->freeBytes > 0x40000000)
			{
				free = info->freeBytes;
				target = settings->m_prefs.m_strDefaultFolder;
				StrAddBackslash(target);
			}
		}
		// ...else the roomiest of F: and G:, unless E: has more.
		if (free == 0)
		{
			SDriveInfo *f = GetDriveInfo('F');
			SDriveInfo *g = GetDriveInfo('G');
			BOOL fBig = f != NULL && f->freeBytes >= 0xFFFFFFFF80000000ULL;   // TODO: verify (as written: a -1 high dword)
			if (!fBig && g != NULL && g->mounted)
			{
				if (f != NULL && f->mounted)
				{
					if (g->freeBytes <= f->freeBytes)
					{
						StrFormat(target, "F:\\%s\\", category.c_str());
						free = f->freeBytes;
					}
					else
					{
						StrFormat(target, "G:\\%s\\", category.c_str());
						free = g->freeBytes;
					}
				}
			}
			else if (f != NULL && f->mounted)
			{
				free = f->freeBytes;
				StrFormat(target, "F:\\%s\\", category.c_str());
			}
		}
		SDriveInfo *e = GetDriveInfo('E');
		if (free == 0 || (e != NULL && e->mounted && e->freeBytes > free))
			StrFormat(target, "E:\\%s\\", category.c_str());
		target.append(name);
	}

	if (!CreateDirectoryTree(target.c_str()))
	{
		g_dwOperationError = GetLastError();
		g_bOperationFailed = g_dwOperationError != 0;
		if (g_bOperationFailed)
		{
			g_bFileOperationPaused = TRUE;
			return;
		}
	}
	std::string source = "D:\\";
	if (fromXbe)
		source = GetPathDirectory(destination);
	CopyFiles(source.c_str(), target.c_str());
}
