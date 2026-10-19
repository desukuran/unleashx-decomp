// CFileManager::ProcessInput: the viewers and pop-ups first, then the
// running operation's step, then the context menu, the keyboard and the
// panes. See FileManager.h.
#include "FileManager.h"

#include <cctype>
#include <cstring>
#include <cwchar>

#include "../app/UnleashX.h"
#include "../audio/MusicManager.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../media/WMVideoPlayer.h"
#include "../net/Network.h"
#include "../render/Fonts.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/Storage.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/Menu.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"
#include "../musicplayer/AudioGUI.h"
#include "FtpManager.h"
#include "Viewers.h"

// The drive letter of a pane path ("/C/..." or "C:\..."). (inlined)
static char PathDrive(const std::string &path)
{
	char drive = StrLeft(path, 1)[0];
	if (drive == '/')
		drive = StrMid(path, 1, 1)[0];
	return drive;
}

// See FileManager.h. (inlined in each)
void FormatTotal(ULONGLONG total, std::string &value, const char *&unit)
{
	double mb = (double)(LONGLONG)total * (1.0 / 1048576.0);
	if (mb >= 10.0)
	{
		value = FormatDouble(2, mb);
		unit = "MB";
		return;
	}
	double kb = (double)(LONGLONG)total * (1.0 / 1024.0);
	if (kb >= 10.0)
	{
		value = FormatDouble(2, kb);
		unit = "KB";
		return;
	}
	value = FormatInt((int)(LONGLONG)total);
	unit = "bytes";
}

// "Invalid selection. Please select at least one file to <verb>." (inlined)
static void FormatInvalidSelection(std::string &message, int verb)
{
	StrFormat(message, GetStringA(STR_MSG_INVALID_SELECTION), GetStringA(verb));
}

// Opens the keyboard. (inlined)
static void OpenKeyboard(const wchar_t *title, UINT maxLength)
{
	g_virtualKeyboard.SetTitle(title);
	g_virtualKeyboard.m_nMaxLength = maxLength;
	g_virtualKeyboard.Open();
	g_virtualKeyboard.m_bPassword = FALSE;
}

// The current folder of pane with a trailing backslash. (inlined)
static std::string FolderOf(IFileFolder *pane)
{
	std::string dir = pane->GetCurrentPath();
	if (dir.size() == 1)
		dir.append(":\\");
	else if (StrRight(dir, 1).compare("\\") != 0)
		dir.append("\\");
	return dir;
}

// Copy (2), move (3) and delete (4): collect, ask, run, report.
void CFileManager::StepOperation(int *action)
{
	std::string message;
	int state = m_nState;
	int verb = state == FMSTATE_COPY ? STR_COPY : state == FMSTATE_MOVE ? STR_MOVE : STR_DELETE;
	switch (m_nStep)
	{
	case FMSTEP_COLLECTING:
	{
		if (!m_bMessageShown)
		{
			g_bCancelOperation = TRUE;
			m_nState = FMSTATE_BROWSE;
			break;
		}
		if (!g_bFilesCollected || g_bCancelOperation)
			break;
		if (g_nOperationFolders == 0 && g_nOperationFiles == 0)
		{
			FormatInvalidSelection(message, verb);
			ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			m_nState = FMSTATE_BROWSE;
			break;
		}

		// Room at the destination (moves: only to another drive; the
		// panes' first two characters are compared).
		BOOL checkSpace = TRUE;
		if (state == FMSTATE_MOVE)
			checkSpace = StrLeft(m_panes[1].m_strPath, 2).compare(StrLeft(m_panes[0].m_strPath, 2).c_str()) != 0;
		else if (state == FMSTATE_DELETE)
			checkSpace = FALSE;
		if (checkSpace)
		{
			SDriveInfo *info = GetDriveInfo(PathDrive(OtherPane()->m_strPath));
			if (info != NULL)
			{
				UpdateDriveSpace(info);
				if (info->freeBytes < g_ullOperationTotal)
				{
					m_nStep = FMSTEP_DONE;
					std::string required = FormatNumberA(g_ullOperationTotal);
					std::string available = FormatNumberA(info->freeBytes);
					StrFormat(message, GetStringA(STR_MSG_INSUFFICIENTSPACE), available.c_str(), required.c_str());
					ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
					break;
				}
			}
		}

		// "You are about to ..."
		m_nStep = FMSTEP_CONFIRM;
		std::string value;
		const char *unit;
		FormatTotal(g_ullOperationTotal, value, unit);
		std::string files = FormatInt((int)g_nOperationFiles);
		std::string folders = FormatInt((int)g_nOperationFolders);
		int ask = state == FMSTATE_COPY ? STR_MSG_ABOUT_TO_COPY
		        : state == FMSTATE_MOVE ? STR_MSG_ABOUT_TO_MOVE : STR_MSG_ABOUT_TO_DELETE;
		StrFormat(message, GetStringA(ask), files.c_str(), folders.c_str(), value.c_str(), unit);
		ShowMessage(message.c_str(), GetString(STR_CONFIRM), MSGBOX_BUTTONS_OKCANCEL);
		// Moves and deletes start on Cancel.
		if (state != FMSTATE_COPY)
			m_pMessageBox->SetSelection(MSGBOX_CANCEL);
		break;
	}

	case FMSTEP_CONFIRM:
	{
		if (m_bMessageShown)
			break;
		if (m_pMessageBox->GetChosenButton() != MSGBOX_OK)
		{
			m_nState = FMSTATE_BROWSE;
			break;
		}
		HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, OperationThread, this, 0, NULL);
		if (thread == NULL || thread == INVALID_HANDLE_VALUE)
		{
			m_nPrevState = m_nState;
			m_nState = FMSTATE_BROWSE;
			ShowMessage("Error Creating Thread", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			break;
		}
		m_nStep = FMSTEP_RUNNING;
		int running = state == FMSTATE_COPY ? STR_MSG_COPYING : state == FMSTATE_MOVE ? STR_MSG_MOVING : STR_MSG_DELETING;
		ShowMessage(GetStringA(running), GetString(STR_INFORMATION), MSGBOX_BUTTONS_CANCEL);
		SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
		CloseHandle(thread);
		break;
	}

	case FMSTEP_RUNNING:
		if (!m_bMessageShown)
		{
			// The box was closed: Cancel.
			g_bCancelOperation = TRUE;
			m_nState = FMSTATE_BROWSE;
			break;
		}
		if (!g_bCancelOperation)
		{
			if (!g_bFileOperationPaused)
				break;
			m_nStep = FMSTEP_DONE;
			std::string files = FormatInt((int)g_nOperationFiles);
			std::string folders = FormatInt((int)g_nOperationFolders);
			int done = state == FMSTATE_COPY ? STR_MSG_COPY_SUCCESS
			         : state == FMSTATE_MOVE ? STR_MSG_MOVE_SUCCESS : STR_MSG_DELETE_SUCCESS;
			StrFormat(message, GetStringA(done), files.c_str(), folders.c_str());
			ShowResult(message, GetString(STR_SUCCESS));
		}
		if (g_bFileOperationPaused && g_bOperationFailed)
		{
			m_nStep = FMSTEP_DONE;
			int error = state == FMSTATE_COPY ? STR_MSG_COPY_ERROR
			          : state == FMSTATE_MOVE ? STR_MSG_MOVE_ERROR : STR_MSG_DELETE_ERROR;
			StrFormat(message, GetStringA(error), (int)GetOperationError());
			ShowResult(message, GetString(STR_ERROR));
			if (state == FMSTATE_COPY)
				g_bOperationFailed = FALSE;
		}
		break;

	case FMSTEP_DONE:
		if (!m_bMessageShown)
			m_nState = FMSTATE_BROWSE;
		break;
	}
	// Quirk: X cancels (the message box takes B and A).
	if (*action == INPUT_X)
	{
		g_bCancelOperation = TRUE;
		m_nState = FMSTATE_BROWSE;
	}
}

// Add to zip (11): collect, then compress straight away.
void CFileManager::StepZip(int *action)
{
	std::string message;
	switch (m_nStep)
	{
	case FMSTEP_COLLECTING:
		if (!m_bMessageShown)
		{
			g_bCancelOperation = TRUE;
			m_nState = FMSTATE_BROWSE;
			break;
		}
		if (g_bFilesCollected && !g_bCancelOperation)
		{
			HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, OperationThread, this, 0, NULL);
			if (thread == NULL)
			{
				m_nPrevState = m_nState;
				m_nState = FMSTATE_BROWSE;
				ShowMessage("Error Creating Thread", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
				break;
			}
			m_nStep = FMSTEP_RUNNING;
			ShowMessage(GetStringA(STR_MSG_ZIPPING), GetString(STR_INFORMATION), MSGBOX_BUTTONS_CANCEL);
			CloseHandle(thread);
		}
		break;

	case FMSTEP_RUNNING:
		if (!m_bMessageShown)
		{
			g_bCancelOperation = TRUE;
			m_nState = FMSTATE_BROWSE;
			break;
		}
		if (!g_bCancelOperation)
		{
			if (!g_bFileOperationPaused)
				break;
			m_nStep = FMSTEP_DONE;
			std::string files = FormatInt((int)g_nOperationFiles);
			std::string folders = FormatInt((int)g_nOperationFolders);
			StrFormat(message, GetStringA(STR_MSG_ZIP_SUCCESS), files.c_str(), folders.c_str());
			ShowResult(message, GetString(STR_SUCCESS));
		}
		if (g_bFileOperationPaused && g_bOperationFailed)
		{
			m_nStep = FMSTEP_DONE;
			StrFormat(message, GetStringA(STR_MSG_ZIP_ERROR), (int)GetOperationError());
			ShowResult(message, GetString(STR_ERROR));
		}
		break;

	case FMSTEP_DONE:
		if (!m_bMessageShown)
			m_nState = FMSTATE_BROWSE;
		break;
	}
	if (*action == INPUT_B)
	{
		g_bCancelOperation = TRUE;
		m_nState = FMSTATE_BROWSE;
	}
}

// Calculate used space (7).
void CFileManager::StepCalcSize(int *action)
{
	if (m_nStep == FMSTEP_COLLECTING)
	{
		if (!m_bMessageShown)
		{
			g_bCancelOperation = TRUE;
			m_nState = FMSTATE_BROWSE;
		}
		else if (g_bFilesCollected && !g_bCancelOperation)
		{
			// "Disk space used by n File(s) in n Folder(s): n bytes / x MB"
			m_nStep = FMSTEP_DONE;
			double mb = (double)(LONGLONG)g_ullOperationTotal * (1.0 / 1048576.0);
			std::string value;
			const char *unit;
			if (mb >= 10.0)
			{
				value = FormatDouble(2, mb);
				unit = "MB";
			}
			else
			{
				value = FormatDouble(2, (double)(LONGLONG)g_ullOperationTotal * (1.0 / 1024.0));
				unit = "KB";
			}
			std::string bytes = FormatNumberA(g_ullOperationBytes);
			std::string folders = FormatInt((int)g_nOperationFolders);
			std::string files = FormatInt((int)g_nOperationFiles);
			std::string message;
			StrFormat(message, GetStringA(STR_MSG_CALC_SPACE), files.c_str(), folders.c_str(), bytes.c_str(), "bytes",
			          value.c_str(), unit);
			ShowMessage(message.c_str(), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
		}
	}
	else if (m_nStep == FMSTEP_DONE && !m_bMessageShown)
	{
		m_nState = FMSTATE_BROWSE;
	}
	if (*action == INPUT_B)
	{
		g_bCancelOperation = TRUE;
		m_nState = FMSTATE_BROWSE;
	}
}

// Copy DVD (8): CopyDVD() runs as a file operation of its own. TRUE when
// the input goes on to the menus and panes (the box was closed).
BOOL CFileManager::StepCopyDVD(int *action)
{
	if (m_nStep == FMSTEP_COLLECTING)
	{
		if (!m_bMessageShown)
		{
			m_panes[0].Reload();
			m_panes[1].Reload();
			m_nState = FMSTATE_BROWSE;
		}
		return FALSE;
	}
	if (!m_bMessageShown)
	{
		g_bCancelOperation = TRUE;
		m_nState = FMSTATE_BROWSE;
		return TRUE;
	}
	if (*action == INPUT_X)
	{
		g_bCancelOperation = TRUE;
		m_nState = FMSTATE_BROWSE;
	}
	if (!g_bFileOperationPaused)
		return FALSE;
	// Quirk: shown again on every frame while the box is up; the error
	// uses the game saves' copy message.
	std::string message;
	if (g_bOperationFailed)
	{
		StrFormat(message, GetStringA(STR_MSG_GAMESAVES_COPY_ERROR), (int)GetOperationError());
		ShowResult(message, GetString(STR_ERROR));
	}
	else
	{
		ShowResult(std::string(GetStringA(STR_MSG_DVD_COPY_SUCCESS)), GetString(STR_SUCCESS));
	}
	g_bOperationFailed = FALSE;
	m_nStep = FMSTEP_DONE;
	return FALSE;
}

// Format (14): the answer to "Are you sure ...".
void CFileManager::StepFormat()
{
	if (m_nStep != FMSTEP_CONFIRM || m_bMessageShown)
		return;
	if (m_pMessageBox->GetResult() != MSGBOX_YES)
	{
		m_nState = FMSTATE_BROWSE;
		return;
	}
	// Not C: or D:; a drive without sizes, or H: to Z:.
	SFolderItem *item = ActivePane()->m_pCurrent;
	char drive = (char)toupper((char)item->m_pszName[0]);
	BOOL formatted = FALSE;
	if (drive != 'C' && drive != 'D')
	{
		SDriveInfo *info = GetDriveInfo(drive);
		if (info != NULL && ((info->freeBytes == 0 && info->usedBytes == 0) || (drive > 'G' && drive < '[')) &&
		    SUCCEEDED(FormatDrive(drive)))
			formatted = TRUE;
	}
	m_nPrevState = m_nState;
	m_nState = FMSTATE_BROWSE;
	m_nStep = FMSTEP_DONE;
	std::string drivePath, message;
	StrFormat(drivePath, "%c:\\.", drive);
	// (Both results are titled Success.)
	StrFormat(message, GetStringA(formatted ? STR_MSG_MENU_FORMAT_SUCCESS : STR_MSG_MENU_FORMAT_ERROR),
	          drivePath.c_str());
	ShowMessage(message.c_str(), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
	if (formatted)
	{
		Sleep(50);
		m_panes[1].Reload();
		m_panes[0].Reload();
	}
}

// The context menu, enabled for what's highlighted and marked.
void CFileManager::ShowMenu(IFileFolder *pane)
{
	m_bMenuOpen = TRUE;
	BOOL inRoot = pane->m_strPath.size() < 2;
	BOOL onParent = pane->m_pCurrent != NULL && pane->m_pCurrent->m_bParent;
	m_menu.SetOptionEnabled(FMMENU_ROOT, !inRoot);
	m_menu.SetOptionEnabled(FMMENU_SELECT_ALL, pane->m_strPath.size() > 1 && pane->m_items.size() > 1);
	BOOL marks = !inRoot && pane->HasMarks();
	m_menu.SetOptionEnabled(FMMENU_CLEAR_SELECTION, marks);
	m_menu.SetOptionEnabled(FMMENU_INVERT_SELECTION, marks);
	m_menu.SetOptionEnabled(FMMENU_COPY, marks || !onParent);
	m_menu.SetOptionEnabled(FMMENU_COPY_RESUME, marks || !onParent);
	m_menu.SetOptionEnabled(FMMENU_MOVE, !inRoot && !onParent);
	m_menu.SetOptionEnabled(FMMENU_DELETE, !inRoot && !onParent);
	m_menu.SetOptionEnabled(FMMENU_RENAME, !inRoot && !onParent);
	m_menu.SetOptionEnabled(FMMENU_NEW_FOLDER, !onParent);
	m_menu.SetOptionEnabled(FMMENU_CALC_SIZE, pane->m_pCurrent != NULL || pane->HasMarks());
	BOOL disc = IsDiscPresent();
	m_menu.SetOptionEnabled(FMMENU_COPY_DISC, disc);
	m_menu.SetOptionEnabled(FMMENU_QUICK_COPY_DISC, disc);
	m_menu.SetOptionEnabled(FMMENU_ADD_TO_ZIP, pane->m_pCurrent != NULL || pane->HasMarks());
	BOOL xbe = pane->m_pCurrent != NULL && pane->m_pCurrent->m_nType == ITEM_XBE;
	m_menu.SetOptionEnabled(FMMENU_PATCH_XBE, xbe);
	m_menu.SetOptionEnabled(FMMENU_RENAME_XBE_TITLE, xbe);
	m_menu.SetOptionEnabled(FMMENU_OPEN_TEXT, pane->m_pCurrent != NULL && !pane->m_pCurrent->m_bParent &&
	                                              !pane->m_pCurrent->m_bFolder);
	m_menu.SetOptionEnabled(FMMENU_FTP, IsEthernetLinkUp() && IsNetworkReady());
	// Formatting: a drive in Root other than C: and D:, without sizes or
	// from H: on.
	m_menu.SetOptionEnabled(FMMENU_FORMAT, FALSE);
	if (inRoot && pane->m_pCurrent != NULL && !pane->m_pCurrent->m_bParent && pane->m_pCurrent->m_bFolder)
	{
		char drive = (char)toupper((char)ActivePane()->m_pCurrent->m_pszName[0]);
		if (drive != 'C' && drive != 'D')
		{
			SDriveInfo *info = GetDriveInfo(drive);
			if (info != NULL &&
			    ((info->totalBytes == 0 && info->freeBytes == 0 && info->usedBytes == 0) || (drive > 'G' && drive < '[')))
				m_menu.SetOptionEnabled(FMMENU_FORMAT, TRUE);
		}
	}
	m_menu.Show(0);
}

// The context menu's choice.
void CFileManager::OnMenu(IFileFolder *pane)
{
	std::string message;
	int choice = m_menu.GetSelection();
	switch (choice)
	{
	case FMMENU_ROOT:
		pane->SetPath("/");
		break;

	case FMMENU_SORT:
		if (m_pSortMenu == NULL)
		{
			m_pSortMenu = new CDropDown;
			if (m_pSortMenu == NULL)
				break;
			m_pSortMenu->AddItem(GetString(STR_FILENAME));
			m_pSortMenu->AddItem(GetString(STR_SIZE));
			m_pSortMenu->AddItem(GetString(STR_TYPE));
			m_pSortMenu->AddItem(GetString(STR_DATE));
			CFileFolder *active = m_nActivePane != 0 ? &m_panes[1] : &m_panes[0];
			float x = active->m_fX + 5.0f;
			float y = active->m_fHeight * 0.5f + active->m_fY + 5.0f;
			m_pSortMenu->Create(m_pFont, x, y);
			if (active->m_nSort >= 0 && active->m_nSort < m_pSortMenu->GetCount())
				m_pSortMenu->m_nSelection = active->m_nSort;
			m_pSortMenu->Show(x, y);
		}
		break;

	case FMMENU_SELECT_ALL:
		pane->MarkAll(TRUE);
		break;

	case FMMENU_CLEAR_SELECTION:
		pane->MarkAll(FALSE);
		break;

	case FMMENU_INVERT_SELECTION:
		pane->InvertMarks();
		break;

	case FMMENU_COPY:
	case FMMENU_COPY_RESUME:
	case FMMENU_MOVE:
	{
		BOOL move = choice == FMMENU_MOVE;
		if (!move)
			g_bCopyResume = choice == FMMENU_COPY_RESUME;
		if (_wcsicmp(m_panes[0].GetTitle(), m_panes[1].GetTitle()) == 0)
		{
			ShowMessage(GetStringA(STR_MSG_COPY_INVALID), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			break;
		}
		// The other pane's drive has to be there.
		SDriveInfo *info = GetDriveInfo(PathDrive(OtherPane()->m_strPath));
		if (info != NULL && (!info->mounted || !info->hasMedia))
		{
			ShowMessage(GetStringA(STR_MSG_NO_DESTINATION), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			break;
		}
		g_bFileOperationPaused = FALSE;
		g_bFilesCollected = FALSE;
		g_bCancelOperation = FALSE;
		if (move)
			g_bFolderListChanged = TRUE;   // (copies may reuse the last list)
		BOOL started = StartCollect(pane);
		m_nPrevState = m_nState;
		if (!started)
		{
			m_nState = FMSTATE_BROWSE;
			ShowMessage("Error Creating Thread", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			break;
		}
		m_nState = move ? FMSTATE_MOVE : FMSTATE_COPY;
		m_nStep = FMSTEP_COLLECTING;
		ShowMessage(GetStringA(move ? STR_MSG_MOVE_CHECK : STR_MSG_COPY_CHECK), GetString(STR_INFORMATION),
		            MSGBOX_BUTTONS_CANCEL);
		break;
	}

	case FMMENU_DELETE:
	case FMMENU_CALC_SIZE:
	{
		BOOL remove = choice == FMMENU_DELETE;
		g_bFileOperationPaused = FALSE;
		g_bFilesCollected = FALSE;
		g_bCancelOperation = FALSE;
		if (remove)
		{
			g_nOperationFolders = 0;
			g_nOperationFiles = 0;
			g_bFolderListChanged = TRUE;
		}
		BOOL started = StartCollect(pane);
		m_nPrevState = m_nState;
		if (!started)
		{
			m_nState = FMSTATE_BROWSE;
			ShowMessage("Error Creating Thread", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			break;
		}
		m_nState = remove ? FMSTATE_DELETE : FMSTATE_CALC_SIZE;
		m_nStep = FMSTEP_COLLECTING;
		ShowMessage(GetStringA(remove ? STR_MSG_DELETE_CHECK : STR_MSG_CALC_CHECK), GetString(STR_INFORMATION),
		            MSGBOX_BUTTONS_CANCEL);
		break;
	}

	case FMMENU_RENAME:
		if (pane->m_pCurrent == NULL)
			break;
		m_nPrevState = m_nState;
		m_nState = FMSTATE_RENAME;
		m_bKeyboardOpen = TRUE;
		OpenKeyboard(L"Rename", 0x40);
		g_virtualKeyboard.SetText(pane->m_pCurrent->m_pszName);
		break;

	case FMMENU_NEW_FOLDER:
		m_nPrevState = m_nState;
		m_nState = FMSTATE_NEW_FOLDER;
		m_bKeyboardOpen = TRUE;
		OpenKeyboard(GetString(STR_NEW_FOLDER), 0x40);
		break;

	case FMMENU_COPY_DISC:
	case FMMENU_QUICK_COPY_DISC:
		m_nPrevState = m_nState;
		if (choice == FMMENU_QUICK_COPY_DISC)
		{
			// Straight into the default games folder.
			m_nStep = FMSTEP_RUNNING;
			m_nPrevState = m_nState;
			ShowMessage(GetStringA(STR_MSG_DVD_COPYING), GetString(STR_INFORMATION), MSGBOX_BUTTONS_CANCEL);
			m_nState = FMSTATE_COPY_DVD;
			CopyDVD(NULL);
		}
		else
		{
			// Ask where, starting from the active pane's folder.
			std::string target;
			pane->ToXboxPath(&target, pane->m_strPath.c_str());
			if (!DirectoryExists(target.c_str()))
				target = "E:\\Games\\";
			m_bKeyboardOpen = TRUE;
			OpenKeyboard(GetString(STR_COPY_DISC), 0x40);
			wchar_t text[0x105];
			StrUtf8ToWide(target.c_str(), text, 0x104);
			g_virtualKeyboard.SetText(text);
			m_nState = FMSTATE_COPY_DISC;
		}
		break;

	case FMMENU_ADD_TO_ZIP:
	{
		// A name to suggest: the highlighted item's (up to its first dot)
		// or the folder's, with ".zip".
		m_bKeyboardOpen = TRUE;
		OpenKeyboard(GetString(STR_ADD_TO_ZIP_FILE), 0x40);
		std::string zip = pane->GetCurrentPath();
		if (zip.size() == 1)
			zip.append(":\\");
		if (pane->m_pCurrent != NULL && !pane->m_pCurrent->m_bParent)
		{
			if (StrRight(zip, 1).compare("\\") != 0)
				zip.append("\\");
			zip.append(pane->m_pCurrent->GetName());
			// Quirk: the first dot of the whole path.
			int dot = StrFind(zip, ".");
			if (dot != -1)
				zip = StrLeft(zip, dot);
			zip.append(".zip");
		}
		if (StrFind(zip, ".zip") == -1)
		{
			if (StrRight(zip, 1).compare("\\") == 0)
				zip = StrLeft(zip, (int)zip.size() - 1);
			int slash = StrReverseFind(zip, "\\");
			if (slash != -1)
				zip.append(StrMid(zip, slash, (int)zip.size() - slash));
			zip.append(".zip");
			StrReplaceAll(zip, "\\\\", "\\");
			StrReplaceAll(zip, "..", ".");
		}
		wchar_t text[0x105];
		StrUtf8ToWide(zip.c_str(), text, 0x104);
		g_virtualKeyboard.SetText(text);
		m_nPrevState = m_nState;
		m_nState = FMSTATE_ZIP_NAME;
		break;
	}

	case FMMENU_PATCH_XBE:
	{
		if (pane->m_pCurrent == NULL)
			break;
		std::string path = pane->GetCurrentPath();
		if (path.size() == 1)
			path.append(":\\");
		if (pane->m_pCurrent->m_bParent)
			break;
		if (StrRight(path, 1).compare("\\") != 0)
			path.append("\\");
		path.append(pane->m_pCurrent->GetName());
		// Hard disk XBEs only; the original is kept as .bak.
		if (StrCmpNoCase(StrLeft(path, 2), "D:") == 0 || !FileExists(path.c_str()))
		{
			ShowMessage(GetStringA(STR_MSG_XBEPATCH_SOURCE_ERR), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			break;
		}
		std::string backup = StrLeft(path, (int)path.size() - 4);
		backup.append(".bak");
		CopyFileA(path.c_str(), backup.c_str(), TRUE);
		if (!PatchXbe(path.c_str()))
		{
			ShowMessage(GetStringA(STR_MSG_XBEPATCH_ERROR), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			DeleteFileA(backup.c_str());
			break;
		}
		ShowMessage(GetStringA(STR_MSG_XBEPATCH_SUCCESS), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
		break;
	}

	case FMMENU_RENAME_XBE_TITLE:
	{
		if (pane->m_pCurrent == NULL)
			break;
		std::string path = FolderOf(pane);
		path.append(pane->m_pCurrent->GetName());
		m_nPrevState = m_nState;
		m_nState = FMSTATE_XBE_TITLE;
		m_bKeyboardOpen = TRUE;
		g_virtualKeyboard.SetTitle(GetString(STR_RENAME_XBE_TITLE));
		g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bPassword = FALSE;
		g_virtualKeyboard.m_nMaxLength = 0x28;
		std::string title = GetXbeTitleName(path.c_str());
		if (title.size() != 0)
		{
			wchar_t text[0x2A];
			memset(text, 0, sizeof(text));
			StrUtf8ToWide(title.c_str(), text, 0x28);
			g_virtualKeyboard.SetText(text);
		}
		break;
	}

	case FMMENU_NEW_TEXT:
	case FMMENU_OPEN_TEXT:
	{
		if (g_pFileViewer == NULL)
		{
			g_pFileViewer = new CNotePad;
			if (g_pFileViewer == NULL)
				break;
		}
		CNotePad *editor = (CNotePad *)g_pFileViewer;
		editor->Init(m_pFont, NULL, NULL, NULL);
		if (m_menu.GetSelection() == FMMENU_OPEN_TEXT && pane->m_pCurrent != NULL)
		{
			std::string path = FolderOf(pane);
			path.append(pane->m_pCurrent->GetName());
			editor->Open(path.c_str());
		}
		else
		{
			editor->NewFile();
		}
		g_bFileTextEditor = TRUE;
		g_bFileViewerOpen = TRUE;
		break;
	}

	case FMMENU_FORMAT:
	{
		char drive = (char)ActivePane()->m_pCurrent->m_pszName[0];
		if (GetDriveInfo(drive) == NULL)
			break;
		std::string drivePath;
		StrFormat(drivePath, "%c:\\", drive);
		StrFormat(message, GetStringA(STR_MSG_FORMAT_DRIVE_ASK), drivePath.c_str());
		ShowMessage(message.c_str(), GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
		m_nPrevState = m_nState;
		m_nState = FMSTATE_FORMAT;
		m_nStep = FMSTEP_CONFIRM;
		break;
	}

	case FMMENU_FTP:
		if (g_pFtpManager != NULL)
			delete g_pFtpManager;
		g_pFtpManager = new CFTPManager;
		if (g_pFtpManager != NULL)
		{
			g_pFtpManager->Setup(m_pFont, m_pBackground, &m_panes[0]);
			g_pFtpManager->m_bDrawBorder = m_bDrawBorder;
			g_pFtpManager->m_bSafeBorder = m_bSafeBorder;
			g_pFtpManager->Show();
			m_nActivePane = 0;
			g_bFtpManagerOpen = TRUE;
		}
		break;
	}
}

// The keyboard closed: rename, new folder, zip name, XBE title or disc
// copy target.
void CFileManager::OnKeyboardDone()
{
	if (g_virtualKeyboard.m_bCancelled || wcslen(g_virtualKeyboard.GetText()) == 0)
	{
		m_nState = FMSTATE_BROWSE;
		return;
	}
	IFileFolder *pane = ActivePane();
	std::string dir = pane->GetCurrentPath();
	std::string text = g_virtualKeyboard.GetResult();
	if (dir.size() == 1)
		StrAppend(dir, ":\\");
	else if (StrRight(dir, 1).compare("\\") != 0)
		StrAppend(dir, "\\");
	std::string message;
	switch (m_nState)
	{
	case FMSTATE_RENAME:
		if (!pane->RenameCurrent(text.c_str()))
		{
			DWORD error = GetOperationError();
			StrFormat(message, GetStringA(STR_MSG_RENAME_ERROR), pane->m_pCurrent->GetName().c_str(), text.c_str(),
			          (int)error);
			ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		}
		else
		{
			pane->Reload();
		}
		break;

	case FMSTATE_NEW_FOLDER:
		if (!pane->CreateFolder(text.c_str()))
		{
			StrFormat(message, GetStringA(STR_MSG_CREATEFOLDER_ERROR), (int)GetOperationError());
			ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		}
		else
		{
			pane->Reload();
		}
		break;

	case FMSTATE_ZIP_NAME:
	{
		// A relative name goes into the current folder; a folder gets
		// "new.zip", a name without a dot ".zip".
		m_strZipFile = text;
		std::string typed = m_strZipFile;
		if (StrFind(m_strZipFile, ":") == -1)
		{
			if (StrRight(m_strZipFile, 1).compare("\\") == 0)
				dir.append(StrMid(m_strZipFile, 1, (int)m_strZipFile.size() - 1));
			else
				dir.append(m_strZipFile);
			m_strZipFile = dir;
		}
		if (StrRight(m_strZipFile, 1).compare("\\") == 0)
			StrAppend(m_strZipFile, "new.zip");
		else if (StrFind(m_strZipFile, ".") == -1)
			StrAppend(m_strZipFile, ".zip");
		else if (StrCmpNoCase(StrRight(m_strZipFile, 4), ".zip") != 0)
			StrAppend(m_strZipFile, ".zip");
		g_bFileOperationPaused = FALSE;
		g_bFilesCollected = FALSE;
		g_bCancelOperation = FALSE;
		BOOL started = StartCollect(pane);
		m_nPrevState = m_nState;
		if (!started)
		{
			m_nState = FMSTATE_BROWSE;
			ShowMessage("Error Creating Thread", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		}
		else
		{
			m_nStep = FMSTEP_COLLECTING;
			m_nState = FMSTATE_ZIP;
			ShowMessage(GetStringA(STR_MSG_ZIP_CHECK), GetString(STR_INFORMATION), MSGBOX_BUTTONS_CANCEL);
		}
		return;
	}

	case FMSTATE_XBE_TITLE:
	{
		std::string path = dir + pane->m_pCurrent->GetName();
		if (SetXbeTitleName(path.c_str(), g_virtualKeyboard.GetText()))
			ShowMessage(GetStringA(STR_MSG_XBERENAME_SUCCESS), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
		else
			ShowMessage(GetStringA(STR_MSG_XBERENAME_ERROR), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		break;
	}

	case FMSTATE_COPY_DISC:
		m_nStep = FMSTEP_RUNNING;
		m_nPrevState = m_nState;
		ShowMessage(GetStringA(STR_MSG_DVD_COPYING), GetString(STR_INFORMATION), MSGBOX_BUTTONS_CANCEL);
		m_nState = FMSTATE_COPY_DVD;
		CopyDVD(text.c_str());
		return;
	}
	m_nState = FMSTATE_BROWSE;
}

// 0x0012C91D
HRESULT CFileManager::ProcessInput(int *action, SGamepadState **pad)
{
	if (!m_bInitialized)
		return S_OK;

	// A viewer has all the input until it closes.
	if (g_bFileViewerOpen)
	{
		g_pFileViewer->ProcessInput(action, pad);
		g_bFileViewerOpen = g_pFileViewer->IsOpen();
		if (!g_bFileViewerOpen)
		{
			if (!g_bFileTextEditor && !g_bFileZipOpen && !g_bFileRarOpen && !g_bFileXipOpen)
			{
				if (g_bFileVideoPlaying)
				{
					g_bFileVideoPlaying = FALSE;
					g_pFileVideoPlayer->Close();
					if (g_pMusicManager != NULL)
						g_pMusicManager->Resume();
				}
			}
			else
			{
				// The editor and the archives may have changed files.
				m_panes[0].Reload();
				m_panes[1].Reload();
				g_bFileTextEditor = FALSE;
				g_bFileZipOpen = FALSE;
				g_bFileRarOpen = FALSE;
				g_bFileXipOpen = FALSE;
			}
			if (g_bDeleteViewerOnClose && g_pFileViewer != NULL)
				delete g_pFileViewer;
			g_pFileViewer = NULL;
		}
		return S_OK;
	}
	if (g_bFtpManagerOpen)
	{
		g_pFtpManager->ProcessInput(action, pad);
		g_bFtpManagerOpen = g_pFtpManager->IsOpen();
		if (!g_bFtpManagerOpen)
		{
			if (g_pFtpManager != NULL)
				delete g_pFtpManager;
			g_pFtpManager = NULL;
			m_panes[0].m_bActive = TRUE;
			Layout();
		}
		return S_OK;
	}
	if (g_bFileAudioPlayer)
	{
		g_pAudioGUI->ProcessInput(action, pad);
		if (!g_pAudioGUI->IsOpen())
			g_bFileAudioPlayer = FALSE;
		return S_OK;
	}
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		if (!m_pMessageBox->IsOpen())
		{
			m_bMessageShown = FALSE;
			if (m_bViewLog && m_pMessageBox->GetResult() == MSGBOX_YES)
			{
				// "Would you like to view the log file?" Yes.
				g_bFileTextEditor = TRUE;
				g_bFileViewerOpen = TRUE;
			}
			else if (g_pFileViewer != NULL)
			{
				delete g_pFileViewer;
				g_pFileViewer = NULL;
			}
			m_bViewLog = FALSE;
			return S_OK;
		}
	}

	IFileFolder *pane = ActivePane();
	switch (m_nState)
	{
	case FMSTATE_COPY:
	case FMSTATE_MOVE:
	case FMSTATE_DELETE:
		StepOperation(action);
		return S_OK;
	case FMSTATE_CALC_SIZE:
		StepCalcSize(action);
		return S_OK;
	case FMSTATE_ZIP:
		StepZip(action);
		return S_OK;
	case FMSTATE_COPY_DVD:
		if (!StepCopyDVD(action))
			return S_OK;
		break;
	case FMSTATE_FORMAT:
		StepFormat();
		break;
	}

	// The sort drop-down.
	if (m_pSortMenu != NULL)
	{
		m_pSortMenu->ProcessInput(action);
		if (m_pSortMenu->IsOpen())
			return S_OK;
		if (!m_pSortMenu->m_bCancelled && m_pSortMenu->m_nSelection != -1)
		{
			CFileFolder *active = m_nActivePane != 0 ? &m_panes[1] : &m_panes[0];
			active->SetSort(m_pSortMenu->m_bCancelled ? -1 : m_pSortMenu->m_nSelection);
		}
		delete m_pSortMenu;
		m_pSortMenu = NULL;
		return S_OK;
	}

	// The context menu.
	if (m_bMenuOpen)
	{
		m_menu.ProcessInput(action, pad);
		m_bMenuOpen = m_menu.IsOpen();
		if (!m_bMenuOpen && m_menu.m_bChosen)
			OnMenu(pane);
		return S_OK;
	}

	// The keyboard.
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (!m_bKeyboardOpen)
			OnKeyboardDone();
		return S_OK;
	}

	if (m_nState != FMSTATE_BROWSE || m_bMessageShown)
		return S_OK;

	// Browsing. (The pointer can turn into another action.)
	for (;;)
	{
		switch (*action)
		{
		case INPUT_START:
		case INPUT_WHITE:
			if (!m_bMenuOpen)
				ShowMenu(pane);
			return S_OK;

		case INPUT_BACK:
			m_bOpen = FALSE;
			*action = INPUT_NONE;
			return S_OK;

		case INPUT_LTRIGGER:
		case INPUT_RTRIGGER:
		case INPUT_IR_LEFT:
		case INPUT_IR_RIGHT:
		{
			// The triggers pick the pane.
			int active = *action == INPUT_RTRIGGER || *action == INPUT_IR_RIGHT;
			m_nActivePane = active;
			if (m_panes[1].IsActive() != active)
				g_bFolderListChanged = TRUE;
			m_panes[1].m_bActive = active;
			m_panes[0].m_bActive = !active;
			return S_OK;
		}

		case INPUT_RTHUMB_CLICK:
		case INPUT_IR_DISPLAY:
			m_bSideBySide = !m_bSideBySide;
			Layout();
			return S_OK;

		case INPUT_POINTER_CLICK:
			if (m_bMenuOpen)
				return S_OK;
			pane->ProcessInput(action, pad);
			if (*action != INPUT_POINTER_CLICK)
				return S_OK;
			if (g_bPointerSecondButton)
			{
				*action = INPUT_WHITE;
				return S_OK;
			}
			// A click on the other pane switches to it.
			if (g_bPointerClicked)
			{
				if (m_nActivePane != 0)
				{
					if (PointInRect(m_panes[0].m_rect, g_nPointerX, g_nPointerY))
						*action = INPUT_LTRIGGER;
				}
				else if (PointInRect(m_panes[1].m_rect, g_nPointerX, g_nPointerY))
				{
					*action = INPUT_RTRIGGER;
				}
			}
			if (*action == INPUT_NONE || *action == INPUT_POINTER_CLICK)
				return S_OK;
			continue;

		default:
			// A memory unit came or went: Root panes refresh.
			if (GetMemoryUnitsChanged())
			{
				if (_wcsicmp(m_panes[1].GetTitle(), L"Root") == 0)
					m_panes[1].Reload();
				if (_wcsicmp(m_panes[0].GetTitle(), L"Root") == 0)
					m_panes[0].Reload();
			}
			pane->ProcessInput(action, pad);
			return S_OK;
		}
	}
}
