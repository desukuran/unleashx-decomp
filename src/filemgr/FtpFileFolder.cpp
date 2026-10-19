// CFTPFileFolder: a pane listing an FTP site, and the queue's transfers.
// See FtpManager.h.
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../net/XNet.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../system/FileOps.h"
#include "../system/SystemControl.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "FileManager.h"
#include "FtpManager.h"

static void PlayDoneSound()
{
	if (g_bFolderSounds && g_pFolderDoneSound != NULL)
		g_pFolderDoneSound->Play(0);
}

// 0x001214C6 (this in ebx)
CFTPFileFolder::CFTPFileFolder() : m_ftp(NULL)
{
	m_bConnected = FALSE;
	m_bError = FALSE;
	m_bConnecting = TRUE;
	m_ftp.m_pLog = &g_ftpLog;
	m_ftp.SetMinPort(g_pFtpSettings->m_ftp.m_nClientMinPort);
	m_ftp.SetMaxPort(g_pFtpSettings->m_ftp.m_nClientMaxPort);
	m_nNavigate = FTPNAV_CURRENT;
	m_strUnknownA14 = "";
	m_bClosing = FALSE;
	m_bUnknown9EC = TRUE;
	m_bTransferDone = TRUE;
	m_bListed = TRUE;
}

// 0x0011F6EA
CFTPFileFolder::~CFTPFileFolder()
{
	m_bClosing = TRUE;
	Sleep(5);
	m_ftp.m_pLog = NULL;
	while (m_ftp.m_bBusy)
		Sleep(5);
	m_ftp.Reset();
}

// 0x0011F1C8
unsigned __stdcall CFTPFileFolder::LoadThread(void *param)
{
	((IFileFolder *)param)->LoadList();
	return 0;
}

// 0x0011F5E1
void CFTPFileFolder::Refresh()
{
	// (LoadList() takes m_bListed down itself.)
	while (!m_bListed)
		Sleep(5);
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, LoadThread, this, CREATE_SUSPENDED, NULL);
	if (thread == NULL)
	{
		LoadList();
	}
	else
	{
		ResumeThread(thread);
		CloseHandle(thread);
	}
	m_dwRefreshTick = GetTickCount();
}

// 0x00122E00
BOOL CFTPFileFolder::Connect(const char *host, int port, const char *user, const char *password)
{
	m_bBoostSupported = FALSE;
	m_bListed = TRUE;
	m_ftp.m_strHost = host;
	m_ftp.m_nControlPort = port;
	m_ftp.m_strUser = user;
	m_ftp.m_strPassword = password;
	if (m_ftp.Connect(host, port, user, password))
	{
		m_bConnected = TRUE;
		m_ftp.m_bPassive = m_bPassive;
		// Boost mode: asked of anything that isn't a UNIX server.
		std::string reply = m_ftp.m_strSystem;
		StrMakeUpper(reply);
		if (StrFind(reply, "UNIX TYPE:") == -1)
		{
			m_ftp.SendCommand("SITE BOOST SUPPORTED", TRUE, TRUE);
			const char *text = m_ftp.ReadReply();
			reply = text != NULL ? text : "";
			if (m_ftp.ParseReply(reply) == 226)
				m_bBoostSupported = TRUE;
		}
		else
		{
			m_bBoostSupported = FALSE;
		}
	}
	m_bConnecting = FALSE;
	return m_bConnected;
}

// 0x00122DDA (this in edi)
void CFTPFileFolder::Disconnect()
{
	if (m_bConnected)
	{
		m_ftp.Disconnect();
		m_bConnected = FALSE;
	}
	m_bListed = TRUE;
}

// 0x00122CCD
BOOL CFTPFileFolder::SendRawCommand(const char *command)
{
	if (!m_bConnected)
	{
		m_strReply = "Client Disconnected from Host.";
		m_bError = TRUE;
		return FALSE;
	}
	m_bError = FALSE;
	const char *format;
	if (m_ftp.SendCommand(command, TRUE, TRUE) == -1)
	{
		format = "'%s' Not sent.";
	}
	else
	{
		const char *reply = m_ftp.ReadReply();
		m_strReply = reply != NULL ? reply : "";
		if (m_strReply.size() != 0)
		{
			std::string copy = m_strReply;
			int code = m_ftp.ParseReply(copy);
			if (code != 0 && code < 400)
				return TRUE;
			m_bError = TRUE;
			m_strReply = m_ftp.m_strError;
			return FALSE;
		}
		format = "'%s' sent successfully. No reply from the server.";
	}
	StrFormat(m_strReply, format, command);
	return FALSE;
}

// 0x00123860
void CFTPFileFolder::LoadList()
{
	while (!m_bListed)
		Sleep(5);
	m_bListed = FALSE;
	while (!m_bNotDrawing)
		Sleep(1);
	Sleep(5);
	m_pCurrent = NULL;
	m_nFolderCount = 0;
	m_nFileCount = 0;
	m_bError = FALSE;
	memset(m_szTitle, 0, sizeof(m_szTitle));
	std::string reply;

	BOOL list = TRUE;
	if (!m_bConnected)
	{
		m_bConnecting = TRUE;
		if (!Connect(m_strHost.c_str(), m_nPort, m_strUser.c_str(), m_strPassword.c_str()))
		{
			m_bConnected = FALSE;
			m_bConnecting = FALSE;
			m_strReply = m_ftp.m_strError;
			if (m_strReply.size() == 0)
				m_strReply = "Unable to Connect.";
			m_bError = TRUE;
			list = FALSE;
		}
	}
	if (list)
	{
		ClearItems();
		m_ullTotalSize = 0;
		memset(m_szTotalSize, 0, sizeof(m_szTotalSize));
		// No folder yet: where the server put us.
		if (m_strPath.size() == 0)
		{
			if (m_ftp.SendCommand("PWD", TRUE, TRUE) != -1)
			{
				const char *text = m_ftp.ReadReply();
				reply = text != NULL ? text : "";
				if (m_ftp.ParseReply(reply) == 257)
				{
					int quote = StrFind(reply, "\"", 1);
					if (quote != -1)
					{
						m_strPath = StrMid(reply, 1, quote - 1);
						m_nNavigate = FTPNAV_CURRENT;
					}
				}
			}
			if (m_strPath.size() == 0)
			{
				m_strReply = "Unable to Retrieve List.";
				m_bError = TRUE;
				list = FALSE;
			}
		}
	}
	if (list)
	{
		m_ftp.ChangeDir(m_nNavigate == FTPNAV_UP ? "CDUP" : m_strPath.c_str());
		m_strPath = m_ftp.m_strCurrentDir;
		if (m_strPath.size() == 0)
		{
			StrFormat(m_strReply, "Unable to Connect to Host.\n Socket Error %d.\nPlease Try Again", WSAGetLastError());
			m_bError = TRUE;
			list = FALSE;
		}
	}
	if (list)
	{
		if (m_strPath.size() < 2)
		{
			m_nLevel = 1;
			wcscpy(m_szTitle, L"Root");
		}
		else
		{
			StrReplaceAll(m_strPath, "//", "/");
			StrUtf8ToWide(m_strPath.c_str(), m_szTitle, 0x104);
			if (m_nLevel == 0)
			{
				// The depth from the path.
				for (int slash = StrFind(m_strPath, "/"); slash != -1; slash = StrFind(m_strPath, "/", slash + 1))
					m_nLevel++;
				if (m_nLevel == 0)
					m_nLevel = 1;
			}
		}
		m_nNavigate = FTPNAV_CURRENT;
		WIN32_FIND_DATAA data;
		HANDLE find = m_ftp.FindFirst(m_strPath.c_str(), &data);
		if (find != INVALID_HANDLE_VALUE)
		{
			do
			{
				AddEntry(&data, 0, 0, TRUE);
			} while (m_ftp.FindNext(find, &data));
		}
		m_ftp.FindClose(find);
	}

	// "..": first among the folders (so it's freed with them).
	if (m_strPath.size() > 1)
	{
		SFolderItem *parent = new SFolderItem;
		if (parent != NULL)
		{
			FILETIME none = {0, 0};
			if (!parent->Init("..", 0, none, 0, 0, TRUE, FALSE))
				delete parent;
			else
				m_folders.insert(m_folders.begin(), parent);
		}
	}
	m_items.clear();
	if (m_nFilter != FILTER_FILES && m_folders.size() != 0)
	{
		for (std::vector<SFolderItem *>::iterator it = m_folders.begin(); it != m_folders.end(); ++it)
		{
			if (*it != NULL)
				m_items.push_back(*it);
		}
	}
	if (m_nFilter != FILTER_FOLDERS && m_files.size() != 0)
	{
		for (std::vector<SFolderItem *>::iterator it = m_files.begin(); it != m_files.end(); ++it)
		{
			if (*it != NULL)
				m_items.push_back(*it);
		}
	}
	if ((m_ullTotalSize >> 30) > 10)
	{
		FormatNumberW(m_szTotalSize, m_ullTotalSize >> 30);
		wcscat(m_szTotalSize, L" GB");
	}
	else if ((m_ullTotalSize >> 20) > 10)
	{
		FormatNumberW(m_szTotalSize, m_ullTotalSize >> 20);
		wcscat(m_szTotalSize, L" MB");
	}
	else if ((m_ullTotalSize >> 10) > 10)
	{
		FormatNumberW(m_szTotalSize, m_ullTotalSize >> 10);
		wcscat(m_szTotalSize, L" KB");
	}
	else
	{
		FormatNumberW(m_szTotalSize, m_ullTotalSize);
	}
	FitList();
	Sleep(5);
	m_bListed = TRUE;
}

// 0x0001E7AE
std::string CFTPFileFolder::GetCurrentPath()
{
	if (_wcsicmp(m_szTitle, L"Root") == 0)
		return "/";
	return StrFromWide(m_szTitle);
}

// 0x00120500
void CFTPFileFolder::SetPath(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return;
	m_strPath = path;
	// (Adds to the level instead of setting it.)
	for (int slash = StrFind(m_strPath, "/", 1); slash != -1; slash = StrFind(m_strPath, "/", slash + 1))
		m_nLevel++;
	m_nNavigate = FTPNAV_SET;
	Refresh();
	PlayDoneSound();
}

// 0x0011F18F
BOOL CFTPFileFolder::CreateFolder(const char *name)
{
	if (name != NULL && strlen(name) != 0)
		return m_ftp.CreateDirectory(name, NULL);
	return TRUE;
}

// 0x0012047A
BOOL CFTPFileFolder::RenameCurrent(const char *name)
{
	std::string current = m_pCurrent->GetName();
	if (StrCmpNoCase(current, name) == 0)
		return TRUE;
	return m_ftp.MoveFile(current.c_str(), name);
}

// 0x00122738
HRESULT CFTPFileFolder::Render()
{
	if (!m_bListed && GetTickCount() - m_dwRefreshTick > 250)
	{
		DrawBusyBox(m_pFont, m_bConnecting ? GetString(STR_HTTP_CONNECTING) : L"Please Wait");
		return S_OK;
	}
	return IFileFolder::Render();
}

// 0x00122795
HRESULT CFTPFileFolder::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_NONE || !m_bListed)
		return S_OK;
	if (*action == INPUT_A)
	{
		// Folders open; files don't (they're transferred instead).
		if (m_nSelected >= 0 && m_nSelected < m_nCount && m_nSelected < (int)m_items.size())
		{
			SFolderItem *item = m_items[m_nSelected];
			if (item != NULL)
			{
				if (!item->m_bFolder)
				{
					if (item->m_bParent)
					{
						m_nNavigate = FTPNAV_UP;
						Refresh();
						PlayDoneSound();
					}
				}
				else
				{
					m_topStack[m_nLevel] = m_nTop;
					m_selectionStack[m_nLevel] = m_nSelected;
					m_nLevel++;
					m_nSelected = 0;
					if (StrRight(m_strPath, 1).compare("/") != 0)
						m_strPath.append("/");
					m_strPath.append(StrFromWide(item->m_pszName));
					Refresh();
					PlayDoneSound();
				}
			}
		}
	}
	else if (*action == INPUT_B)
	{
		// Up, unless a transfer runs.
		if (m_bTransferDone && m_strPath.size() > 1)
		{
			m_nLevel--;
			m_nSelected = m_selectionStack[m_nLevel];
			m_nNavigate = FTPNAV_UP;
			Refresh();
			PlayDoneSound();
		}
	}
	else if (*action == INPUT_X)
	{
		// Quirk: the first line can't be marked.
		if (m_nSelected > 0 && m_nSelected < (int)m_items.size())
		{
			SFolderItem *item = m_items[m_nSelected];
			if (item != NULL)
			{
				item->m_bMarked = !item->m_bMarked;
				PlayDoneSound();
				if (item->m_bMarked && m_nSelected < m_nCount - 1)
				{
					m_nSelected++;
					if (m_nBottom < m_nSelected)
					{
						m_nTop++;
						m_nBottom++;
					}
				}
			}
		}
	}
	else
	{
		IFileFolder::ProcessInput(action, pad);
	}
	*action = INPUT_NONE;
	return S_OK;
}

// 0x001229DF
void CFTPFileFolder::CollectFiles(const char *path)
{
	if (!m_ftp.ChangeDir(path))
		return;
	std::string dir = path;
	StrReplaceAll(dir, "\\\\?\\", "");
	if (StrRight(dir, 1).compare("/") != 0)
		dir.append("/");

	// The files first...
	WIN32_FIND_DATAA data;
	HANDLE find = m_ftp.FindFirst(dir.c_str(), &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (g_bCancelOperation)
				break;
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strlen(data.cFileName) != 0)
			{
				std::string file = dir;
				file.append(data.cFileName);
				g_fileList.push_back(SFileListItem(file, FALSE, data.nFileSizeLow));
				g_ullOperationTotal += data.nFileSizeLow;
				g_ullOperationBytes += data.nFileSizeLow;
				g_nOperationFiles++;
			}
		} while (m_ftp.FindNext(find, &data));
	}
	m_ftp.FindClose(find);

	// ...then the folders, each listed in turn. The listing being read
	// is shared, so it's put back after each.
	find = m_ftp.FindFirst(dir.c_str(), &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (g_bCancelOperation)
				break;
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				std::string folder = dir;
				folder.append(data.cFileName);
				g_nOperationFolders++;
				memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
				swprintf(g_szOperationFile, 0x12F, L"%hs", folder.c_str());
				g_fileList.push_back(SFileListItem(folder, TRUE, 0));
				std::string rest = m_ftp.m_strListLeft;
				CollectFiles(folder.c_str());
				m_ftp.m_strListLeft = rest;   // (via a _strdup copy that leaks)
			}
		} while (m_ftp.FindNext(find, &data) == TRUE);
	}
}

// One item of DeleteListed(): DELE, or the whole tree. A failure is logged.
// (inlined twice)
static BOOL DeleteFtpItem(CDirFTP &ftp, SFolderItem *item, const std::string &dir)
{
	memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
	wcscpy(g_szOperationFile, item->m_pszName);
	std::string path;
	StrFormat(path, "%s%S", dir.c_str(), item->m_pszName);
	BOOL ok;
	if (!item->m_bFolder)
	{
		ok = ftp.DeleteFile(path.c_str());
		if (ok)
			g_ullOperationDone += item->m_dwSize;
		g_nOperationProgress++;
	}
	else
	{
		ok = ftp.RemoveDirectory(path.c_str());
	}
	if (!ok)
	{
		g_dwOperationError = (DWORD)WSAGetLastError();
		std::string message;
		StrFormat(message, "Delete FAILED \"%s\". Error Code: %d\tSee FTP log for details.", path.c_str(),
		          (int)g_dwOperationError);
		LogOperation(message.c_str(), FALSE);
	}
	return ok;
}

// 0x001200AD. The marked items, or else the highlighted one. TRUE if
// something failed.
BOOL CFTPFileFolder::DeleteListed()
{
	BOOL result = FALSE;
	BOOL failed = FALSE;      // something failed (and was logged)
	BOOL continued = FALSE;   // ... and the operation went on
	m_bOperationDone = FALSE;
	LogOperation("BEGIN Delete Operation", TRUE);
	std::string dir = GetCurrentPath();
	if (StrRight(dir, 1).compare("/") != 0)
		dir.append("/");
	BOOL abortOnError = g_pFtpSettings->m_prefs.m_bAbortOnError;

	BOOL marked = FALSE;
	for (std::vector<SFolderItem *>::iterator it = m_items.begin(); it != m_items.end() && !g_bOperationFailed; ++it)
	{
		if (g_bCancelOperation)
			break;
		SFolderItem *item = *it;
		if (item == NULL || !item->m_bMarked)
			continue;
		marked = TRUE;
		if (!DeleteFtpItem(m_ftp, item, dir))
		{
			failed = TRUE;
			result = TRUE;
			if (abortOnError)
				g_bOperationFailed = TRUE;
			else
				continued = TRUE;
		}
	}
	if (!marked && !continued && m_pCurrent != NULL && !m_pCurrent->m_bParent)
	{
		if (!DeleteFtpItem(m_ftp, m_pCurrent, dir))
		{
			failed = TRUE;
			result = TRUE;
			if (abortOnError)
				g_bOperationFailed = TRUE;
			else
				continued = TRUE;
		}
	}

	BOOL keepLog = TRUE;
	if (!g_bOperationFailed)
	{
		g_bOperationFailed = continued;
		if (!continued && !failed)
		{
			// All went well: no log.
			g_strOperationLog.clear();
			keepLog = FALSE;
		}
	}
	if (keepLog)
		LogOperation("END Delete Operation", FALSE);
	m_bOperationDone = TRUE;
	g_bFileOperationPaused = TRUE;
	return result;
}

// " (12.34 KBps)": bytes over milliseconds. (inlined)
static float TransferRate(ULONGLONG bytes, DWORD ms)
{
	float rate = (float)((double)(LONGLONG)bytes * (1.0 / 1024.0));
	if (ms != 0)
		rate = rate * 1000.0f / (float)ms;
	return rate;
}

// 0x00120588
void CFTPFileFolder::RunQueue()
{
	g_bOperationFailed = FALSE;
	g_bCancelOperation = FALSE;
	m_ftp.ResetStats();
	m_ftp.m_bBoost = m_bBoostSupported;
	BOOL queued = g_bFileOperationQueued;
	g_bFileOperationQueued = TRUE;
	unsigned skipped = 0;
	unsigned count = (unsigned)g_ftpQueue.size();
	if (count == 0)
		return;   // (g_bFileOperationQueued stays set)
	BOOL led = SetLedPattern(g_pFtpSettings->m_prefs.m_nLEDFileCopy, FALSE);

	// Each item in turn; a failed one stays queued and is skipped.
	unsigned index = 0;
	while (g_ftpQueue.size() != 0)
	{
		if (g_bCancelOperation || skipped == count)
			break;
		SFtpQueueItem item = g_ftpQueue[index];
		memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
		memset(g_szOperationTarget, 0, sizeof(g_szOperationTarget));
		swprintf(g_szOperationFile, 0x12F, L"%hs", item.source.c_str());
		swprintf(g_szOperationTarget, 0x12F, L"%hs", item.target.c_str());
		if (!m_ftp.CopyFileEx(item.source.c_str(), item.target.c_str(), OperationProgress, NULL, NULL,
		                      item.isFolder ? COPY_FTP_FOLDER : 0))
		{
			skipped++;
			index++;
			g_bOperationFailed = TRUE;
		}
		else
		{
			g_ftpQueue.erase(g_ftpQueue.begin() + index);
			if (g_ftpQueue.size() == 0)
				break;
			count = (unsigned)g_ftpQueue.size();
		}
		g_bFileOperationQueued = TRUE;
	}
	if (led)
		SetLedPattern(LED_DEFAULT, FALSE);
	g_bFileOperationQueued = queued;

	if (g_bOperationFailed)
	{
		if (g_bCancelOperation)
			m_strTransferResult = "Transfer Cancelled.";
		else
			m_strTransferResult.clear();
	}
	else
	{
		// "Uploaded 3 files totaling 1234 bytes in 00:05 (2.50 KBps)",
		// and the same for downloads. Quirk: the download minutes divide
		// by 6000, not 60000.
		DWORD upMs = m_ftp.m_dwUploadMs;
		DWORD downMs = m_ftp.m_dwDownloadMs;
		int upMinutes = (int)(upMs / 60000);
		int upSeconds = (int)((upMs / 1000) % 60);
		int downMinutes = (int)(downMs / 6000);
		int downSeconds = (int)((downMs / 1000) % 60);
		float upRate = TransferRate(m_ftp.m_ullBytesUploaded, upMs);
		float downRate = TransferRate(m_ftp.m_ullBytesDownloaded, downMs);
		std::string upRateText, downRateText, bytes;
		if (m_ftp.m_nFilesUploaded != 0 && m_ftp.m_nFilesDownloaded != 0)
		{
			std::string uploaded;
			StrFormat(upRateText, " (%.02f KBps)", (double)upRate);
			StrFormat(downRateText, " (%.02f KBps)", (double)downRate);
			StrFormat(bytes, "%Iu", (DWORD)m_ftp.m_ullBytesUploaded);
			StrFormat(uploaded, "Uploaded %d files totaling %s bytes in %02d:%02d", m_ftp.m_nFilesUploaded,
			          bytes.c_str(), upMinutes, upSeconds);
			StrFormat(bytes, "%Iu", (DWORD)m_ftp.m_ullBytesDownloaded);
			StrFormat(m_strTransferResult, "Downloaded %d files totaling %s bytes in %02d:%02d",
			          m_ftp.m_nFilesDownloaded, bytes.c_str(), downMinutes, downSeconds);
			m_strTransferResult.append(downRateText + "\n" + uploaded + upRateText);
		}
		else if (m_ftp.m_nFilesDownloaded != 0)
		{
			StrFormat(downRateText, " (%.02f KBps)", (double)downRate);
			StrFormat(bytes, "%Iu", (DWORD)m_ftp.m_ullBytesDownloaded);
			StrFormat(m_strTransferResult, "Downloaded %d files totaling %s bytes in %02d:%02d",
			          m_ftp.m_nFilesDownloaded, bytes.c_str(), downMinutes, downSeconds);
			m_strTransferResult.append(downRateText);
		}
		else if (m_ftp.m_nFilesUploaded != 0)
		{
			StrFormat(upRateText, " (%.02f KBps)", (double)upRate);
			StrFormat(bytes, "%Iu", (DWORD)m_ftp.m_ullBytesUploaded);
			StrFormat(m_strTransferResult, "Uploaded %d files totaling %s bytes in %02d:%02d",
			          m_ftp.m_nFilesUploaded, bytes.c_str(), upMinutes, upSeconds);
			m_strTransferResult.append(upRateText);
		}
		else
		{
			m_strTransferResult = "Transfer Complete.";
		}
	}
	m_bTransferDone = TRUE;
	g_bFileOperationPaused = TRUE;
}

// 0x00120B3A
unsigned __stdcall CFTPFileFolder::TransferThread(void *param)
{
	CFTPFileFolder *pane = (CFTPFileFolder *)param;
	pane->RunQueue();
	pane->m_bTransferDone = TRUE;
	return 0;
}

// 0x00120B55 (this in ebx). (TRUE also when one is already running.)
BOOL CFTPFileFolder::StartTransfer()
{
	if (!m_bTransferDone)
		return TRUE;
	g_dFilePercent = 0.0;
	m_bTransferDone = FALSE;
	g_nOperationProgress = 0;
	g_bCancelOperation = FALSE;
	g_bFileOperationPaused = FALSE;
	g_bOperationFailed = FALSE;
	g_ullOperationDone = 0;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, TransferThread, this, CREATE_SUSPENDED, NULL);
	if (thread != NULL)
	{
		SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
		ResumeThread(thread);
		CloseHandle(thread);
	}
	else
	{
		RunQueue();
	}
	return thread != NULL;
}

// 0x00126274
static unsigned __stdcall DeleteListedThread(void *param)
{
	IFileFolder *pane = (IFileFolder *)param;
	if (pane == NULL)
	{
		GetLastError();
		return 0;
	}
	SetLastError(0);
	pane->DeleteListed();
	return 0;
}

// 0x00126296 (pane in ebx)
BOOL StartDeleteThread(IFileFolder *pane)
{
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, DeleteListedThread, pane, CREATE_SUSPENDED, NULL);
	if (thread == NULL || thread == INVALID_HANDLE_VALUE)
		return FALSE;
	pane->m_bOperationDone = FALSE;
	g_bFileOperationPaused = FALSE;
	g_bCancelOperation = FALSE;
	g_bOperationFailed = FALSE;
	memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
	ResumeThread(thread);
	CloseHandle(thread);
	return TRUE;
}
