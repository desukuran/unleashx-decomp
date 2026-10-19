// The file manager's operation thread: copy, move, delete and zip from the
// active pane into the other one, over the list the collect thread made
// (g_fileList). It reports through the FileOps.h progress state and log.
#include "FileManager.h"

#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/SystemControl.h"
#include "../thirdparty/XZip.h"

// A pane's folder as an Xbox path with a trailing backslash; in Root the
// highlighted drive ("C" -> "C:\"). (inlined twice)
static std::string PaneFolder(IFileFolder *pane)
{
	std::string dir = StrFromWide(pane->GetTitle());
	if (StrCmpNoCase(dir.c_str(), "Root") == 0 && pane->m_pCurrent != NULL)
	{
		dir = pane->m_pCurrent->GetName();
		if (dir.size() == 1)
			dir.append(":");
	}
	if (StrRight(dir, 1).compare("\\") != 0)
		dir.append("\\");
	return dir;
}

// The list's item under source, as the same path under destination.
static std::string MapPath(const std::string &path, const std::string &source, const std::string &destination)
{
	std::string target = path;
	StrReplaceAll(target, source.c_str(), destination.c_str());
	return target;
}

// After a run: an empty log is dropped, else closed with END. (inlined)
static void FinishLog(BOOL logged, BOOL failedButContinued, const char *end)
{
	if (!g_bOperationFailed && !(g_bOperationFailed = failedButContinued) && !logged)
		g_strOperationLog.erase();
	else
		LogOperation(end, FALSE);
}

// 0x0012B374
unsigned __stdcall CFileManager::OperationThread(void *param)
{
	CFileManager *fm = (CFileManager *)param;
	BOOL logged = FALSE;               // something went to the log
	BOOL failedButContinued = FALSE;   // a failure without AbortOnError
	BOOL result = FALSE;
	g_strOperationLog.erase();
	std::string message;
	g_dwOperationError = 0;
	SetLastError(0);
	g_dFilePercent = 0.0;
	g_nOperationProgress = 0;
	g_bCancelOperation = FALSE;
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	g_ullOperationDone = 0;

	// From the active pane into the other.
	IFileFolder *destination = &fm->m_panes[1];
	IFileFolder *source = &fm->m_panes[0];
	if (fm->m_nActivePane != 0)
	{
		destination = &fm->m_panes[0];
		source = &fm->m_panes[1];
	}
	std::string sourceDir = PaneFolder(source);
	std::string destDir = PaneFolder(destination);
	CreateDirectoryTree(destDir.c_str());
	BOOL otherDrive = StrCmpNoCase(sourceDir.c_str(), destDir.c_str()) != 0;   // (it compares the whole folders)
	CDirectory *sourceFs = CreateDirObject(sourceDir.c_str());
	CDirectory *destFs = CreateDirObject(destDir.c_str());
	if (sourceFs == NULL)
	{
		SetLastError(8);
		g_bOperationFailed = TRUE;
		return 8;
	}
	if (destFs == NULL && StrCmpNoCase(destDir.c_str(), "Root") != 0)
	{
		delete sourceFs;
		SetLastError(8);
		g_bOperationFailed = TRUE;
		return 8;
	}
	g_pFileSettings = CAppSettings::GetInstance();
	SPreferences &prefs = g_pFileSettings->m_prefs;
	BOOL led = FALSE;
	std::string sourcePath, targetPath;

	if (fm->m_nState == FMSTATE_COPY)
	{
		LogOperation("BEGIN Copy Operation", TRUE);
		led = SetLedPattern(prefs.m_nLEDFileCopy, FALSE);
		std::vector<SFileListItem>::iterator it = g_fileList.begin();
		for (; it != g_fileList.end(); ++it)
		{
			if (g_bCancelOperation)
			{
				LogOperation("Copy Operation Canceled", FALSE);
				break;
			}
			sourcePath = it->path;
			targetPath = MapPath(it->path, sourceDir, destDir);
			if (!it->isFolder)
			{
				// "Copy (resume)": a file of the same size is already there.
				if (g_bCopyResume)
				{
					WIN32_FIND_DATAA data;
					HANDLE find = destFs->FindFirst(targetPath.c_str(), &data);
					if (find != INVALID_HANDLE_VALUE)
					{
						BOOL same = data.nFileSizeLow == it->size;
						CloseHandle(find);   // (not FindClose)
						if (same)
							continue;
					}
				}
				if (prefs.m_bAutoRename && MakeFatxPath(targetPath.c_str(), targetPath))
				{
					StrFormat(message, "Renamed \"%s\" to \"%s\"", sourcePath.c_str(), targetPath.c_str());
					LogOperation(message.c_str(), FALSE);
					logged = TRUE;
				}
				StrUtf8ToWide(sourcePath.c_str(), g_szOperationFile, 0x12F);
				StrUtf8ToWide(targetPath.c_str(), g_szOperationTarget, 0x12F);
				// Three tries.
				BOOL copied = FALSE;
				for (int tries = 3; tries >= 1;)
				{
					CreateDirectoryTree(GetPathDirectory(targetPath.c_str()).c_str());
					copied = sourceFs->CopyFileEx(sourcePath.c_str(), targetPath.c_str(), OperationProgress, NULL, NULL, 0);
					tries--;
					Sleep(copied ? 1 : 10);
					if (copied)
						break;
				}
				if (copied)
				{
					SetFileAttributesA(targetPath.c_str(), FILE_ATTRIBUTE_NORMAL);
					continue;
				}
				g_dwOperationError = GetLastError();
				StrFormat(message, "Copy FAILED \"%s\". Error Code: %d", sourcePath.c_str(), (int)g_dwOperationError);
				LogOperation(message.c_str(), FALSE);
				logged = TRUE;
				if (prefs.m_bAbortOnError)
				{
					g_bOperationFailed = TRUE;
					result = TRUE;
					break;
				}
				failedButContinued = TRUE;
			}
			else if (!destFs->DirectoryExists(targetPath.c_str()))
			{
				if (!CreateDirectoryTree(targetPath.c_str()) && !destFs->CreateDirectory(targetPath.c_str(), NULL))
				{
					g_dwOperationError = GetLastError();
					g_bOperationFailed = g_dwOperationError != 0;
					if (g_bOperationFailed)
					{
						StrFormat(message, "Unable to create folder \"%s\". Error Code: %d", targetPath.c_str(),
						          (int)g_dwOperationError);
						LogOperation(message.c_str(), FALSE);
						logged = TRUE;
						result = TRUE;
						break;
					}
				}
				Sleep(1);
			}
		}
		FinishLog(logged, failedButContinued, "END Copy Operation");
		if (!g_bOperationFailed)
		{
			RefreshDrives(TRUE);
			destination->Reload();
		}
	}
	else if (fm->m_nState == FMSTATE_MOVE)
	{
		LogOperation("BEGIN Move Operation", TRUE);
		led = SetLedPattern(prefs.m_nLEDBusy, FALSE);
		for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end(); ++it)
		{
			if (g_bCancelOperation)
				break;
			DWORD flags = it->isFolder ? MOVEFILE_COPY_ALLOWED : MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING;
			sourcePath = it->path;
			targetPath = MapPath(it->path, sourceDir, destDir);
			StrUtf8ToWide(sourcePath.c_str(), g_szOperationFile, 0x12F);
			StrUtf8ToWide(targetPath.c_str(), g_szOperationTarget, 0x12F);
			Sleep(1);
			SetFileAttributesA(sourcePath.c_str(), FILE_ATTRIBUTE_NORMAL);
			// Folders going elsewhere are created; the rest (and folders
			// already there) is moved.
			if (otherDrive && it->isFolder && !destFs->DirectoryExists(targetPath.c_str()))
			{
				if (!CreateDirectoryTree(targetPath.c_str()) && !destFs->CreateDirectory(targetPath.c_str(), NULL))
				{
					g_dwOperationError = GetLastError();
					g_bOperationFailed = g_dwOperationError != 0;
					if (g_bOperationFailed)
					{
						g_dwOperationError = GetLastError();
						StrFormat(message, "Move FAILED \"%s\". Error Code: %d", targetPath.c_str(),
						          (int)g_dwOperationError);
						LogOperation(message.c_str(), FALSE);
						logged = TRUE;
						result = TRUE;
						break;
					}
				}
				Sleep(1);
				continue;
			}
			if (sourceFs->MoveFileWithProgress(sourcePath.c_str(), targetPath.c_str(), OperationProgress, NULL, flags))
				continue;
			g_dwOperationError = GetLastError();
			StrFormat(message, "Move FAILED \"%s\". Error Code: %d", sourcePath.c_str(), (int)g_dwOperationError);
			LogOperation(message.c_str(), FALSE);
			logged = TRUE;
			result = TRUE;
			if (prefs.m_bAbortOnError)
			{
				g_bOperationFailed = TRUE;
				break;
			}
			failedButContinued = TRUE;
		}
		// Moved elsewhere: the emptied folders go, deepest first.
		if (otherDrive && !g_bOperationFailed)
		{
			Sleep(5);
			for (int i = (int)g_fileList.size() - 1; i >= 0 && !g_bCancelOperation; i--)
			{
				if (g_fileList[i].isFolder && !sourceFs->RemoveDirectory(g_fileList[i].path.c_str()))
					Sleep(1);
			}
		}
		g_bFolderListChanged = TRUE;
		if (!g_bOperationFailed && (!result || !(g_bOperationFailed = failedButContinued)) && !logged)
			g_strOperationLog.erase();
		else
			LogOperation("END Move Operation", FALSE);
		if (!g_bOperationFailed)
		{
			RefreshDrives(TRUE);
			destination->Reload();
			source->Reload();
		}
	}
	else if (fm->m_nState == FMSTATE_DELETE)
	{
		led = SetLedPattern(prefs.m_nLEDBusy, FALSE);
		source->DeleteListed();
		if (!g_bOperationFailed)
		{
			RefreshDrives(TRUE);
			destination->Reload();
			source->Reload();
		}
		g_bFolderListChanged = TRUE;
	}
	else if (fm->m_nState == FMSTATE_ZIP)
	{
		led = SetLedPattern(prefs.m_nLEDBusy, FALSE);
		std::string zipDir = GetPathDirectory(fm->m_strZipFile.c_str());
		if (!CreateDirectoryTree(zipDir.c_str()))
		{
			g_dwOperationError = GetLastError();
			g_bOperationFailed = g_dwOperationError != 0;
			if (g_bOperationFailed)
				result = TRUE;
		}
		if (!g_bOperationFailed)
		{
			HZIP zip = CreateZip((void *)fm->m_strZipFile.c_str(), 0, ZIP_FILENAME);   // (0x000BDE3B)
			if (zip == NULL)
			{
				g_bOperationFailed = TRUE;
				result = TRUE;
			}
			else
			{
				LogOperation("BEGIN Zip Operation", TRUE);
				for (std::vector<SFileListItem>::iterator it = g_fileList.begin();
				     it != g_fileList.end() && !g_bCancelOperation; ++it)
				{
					// Files only; the names are relative to the source.
					if (it->isFolder)
						continue;
					Sleep(5);
					StrUtf8ToWide(it->path.c_str(), g_szOperationFile, 0x12F);
					Sleep(1);
					std::string name = it->path;
					StrReplaceAll(name, sourceDir.c_str(), "");
					if (ZipAdd(zip, name.c_str(), (void *)it->path.c_str(), 0, ZIP_FILENAME) == ZR_OK)
					{
						g_nOperationProgress++;
						continue;
					}
					g_dwOperationError = GetLastError();
					StrFormat(message, "Zip FAILED \"%s\". Error Code: %d", it->path.c_str(), (int)g_dwOperationError);
					LogOperation(message.c_str(), FALSE);
					logged = TRUE;
					result = TRUE;
					if (!prefs.m_bAbortOnError)
					{
						failedButContinued = TRUE;
						continue;
					}
					g_bOperationFailed = TRUE;
					break;
				}
				CloseAnyZip(zip);
				FinishLog(logged, failedButContinued, "END Zip Operation");
				if (!g_bOperationFailed)
					RefreshDrives(TRUE);
				destination->Reload();
				source->Reload();
			}
		}
	}

	delete sourceFs;
	delete destFs;
	Sleep(100);
	g_bCancelOperation = FALSE;
	g_bFileOperationPaused = TRUE;
	RefreshDrives(TRUE);
	if (led)
		SetLedPattern(LED_DEFAULT, FALSE);
	return result;
}
