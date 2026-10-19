// CFTPManager::ProcessInput: the running operation, the message box, the
// log, the site editor, the keyboard, the menus and the panes. See
// FtpManager.h.
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../net/Network.h"
#include "../render/Fonts.h"
#include "../system/FileOps.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"
#include "FileManager.h"
#include "FtpManager.h"

// The log window: opened from either menu or the left stick click.
// (0x0011F682 -> 0x0001E769)
static void OpenLog()
{
	g_ftpLog.ResetRect();
	g_ftpLog.m_bOpen = TRUE;
	g_ftpLog.m_bMessageShown = FALSE;
	g_ftpLog.m_bMenuOpen = FALSE;
	g_ftpLog.m_bKeyboardOpen = FALSE;
}

// The site editor closed: save the site, or quick connect.
void CFTPManager::FinishSiteEditor()
{
	CFTPSite *editor = m_pSiteEditor;
	if (!editor->m_bSaved)
		return;
	if (editor->m_bNewSite)
	{
		m_siteList.AddSite(editor->m_strName.c_str(), editor->m_strAddress.c_str(), editor->m_nPort,
		                   editor->m_strUser.c_str(), editor->m_strPassword.c_str(), editor->m_bPassive,
		                   editor->m_strRemotePath.c_str(), editor->m_strLocalPath.c_str());
		return;
	}
	if (m_bQuickConnect)
	{
		Connect();
		return;
	}
	// An edit: saved under the highlighted site's name.
	std::string oldName;
	StrFormat(oldName, "%S", m_siteList.m_pCurrent->m_pszName);
	m_siteList.SaveSite(oldName.c_str(), editor->m_strName.c_str(), editor->m_strAddress.c_str(), editor->m_nPort,
	                    editor->m_strUser.c_str(), editor->m_strPassword.c_str(), editor->m_bPassive,
	                    editor->m_strRemotePath.c_str(), editor->m_strLocalPath.c_str());
}

// The keyboard closed: rename, new folder or a raw command. Quirk: a
// failed new folder reports with the rename message (and the highlighted
// item's name, which must exist).
void CFTPManager::FinishKeyboard()
{
	IFileFolder *pane = m_bRightPane ? (IFileFolder *)m_pRemote : m_pLocal;
	if (g_virtualKeyboard.m_bCancelled || wcslen(g_virtualKeyboard.GetText()) == 0)
	{
		m_nState = FTPSTATE_BROWSE;
		return;
	}
	std::string text = g_virtualKeyboard.GetResult();
	std::string message;
	if (m_nState == FTPSTATE_RENAME || m_nState == FTPSTATE_NEW_FOLDER)
	{
		BOOL ok = m_nState == FTPSTATE_RENAME ? pane->RenameCurrent(text.c_str()) : pane->CreateFolder(text.c_str());
		if (ok)
		{
			pane->Reload();
		}
		else
		{
			if (!m_bRightPane)
			{
				DWORD error = GetLastError();
				StrFormat(message, GetStringA(STR_MSG_RENAME_ERROR), pane->m_pCurrent->GetName().c_str(), text.c_str(),
				          (int)error);
			}
			else
			{
				std::string reply = m_pRemote->m_strReply;
				StrFormat(message, "An error occured while renaming %s to %s.\nReply: %s",
				          pane->m_pCurrent->GetName().c_str(), text.c_str(), reply.c_str());
			}
			ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		}
	}
	else if (m_nState == FTPSTATE_RAW_COMMAND)
	{
		m_pRemote->SendRawCommand(text.c_str());
		std::string reply = m_pRemote->m_strReply;
		ShowMessage(reply.c_str(), L"Site Response", MSGBOX_BUTTONS_OK);
	}
	m_nState = FTPSTATE_BROWSE;
}

// A transfer or delete is running: report its end, or ask to cancel it.
// TRUE: still running, the input goes on to the message box.
BOOL CFTPManager::HandleOperation(int *action)
{
	// (A cancelled one ends when the thread stops, with or without an
	// error.)
	BOOL running = !g_bCancelOperation ? !g_bFileOperationPaused : !g_bFileOperationPaused || !g_bOperationFailed;
	if (running)
	{
		if (m_nState == FTPSTATE_CANCELLING)
		{
			if (!g_bFileOperationPaused)
			{
				Sleep(5);
				return FALSE;
			}
			m_nState = FTPSTATE_BROWSE;
			m_bOperationRunning = FALSE;
			m_bAskingCancel = FALSE;
			m_pLocal->Reload();
			m_pRemote->Reload();
			return FALSE;
		}
		if ((*action == INPUT_BACK || *action == INPUT_B || *action == INPUT_A) && !m_bAskingCancel)
		{
			m_bAskingCancel = TRUE;
			ShowMessage("Are you sure you want to cancel the current operation?", GetString(STR_CONFIRM),
			            MSGBOX_BUTTONS_YESNO);
			*action = INPUT_NONE;
			return FALSE;
		}
		return TRUE;
	}

	std::string message;
	if (!g_bOperationFailed)
	{
		if (m_nState == FTPSTATE_TRANSFER)
		{
			message = m_pRemote->m_strTransferResult;
		}
		else if (m_nState == FTPSTATE_COLLECT || m_nState == FTPSTATE_DELETE)
		{
			if (!m_bRightPane)
				StrFormat(message, GetStringA(STR_MSG_DELETE_SUCCESS), FormatInt((int)g_nOperationFiles).c_str(),
				          FormatInt((int)g_nOperationFolders).c_str());
			else
				message = "Successfully deleted selected object(s).";
		}
		ShowMessage(message.c_str(), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
	}
	else
	{
		if (m_nState == FTPSTATE_TRANSFER)
		{
			message = "An error occured while transfering file(s).\nSee FTP log for details.";
		}
		else if (m_nState == FTPSTATE_COLLECT || m_nState == FTPSTATE_DELETE)
		{
			if (m_bRightPane)
				message = "An error occured while deleting file(s).\nSee FTP log for details.";
			else
				StrFormat(message, "An error occured while deleting file(s).\nError Code: %d", (int)GetLastError());
		}
		ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		g_bOperationFailed = FALSE;
	}
	m_nState = FTPSTATE_BROWSE;
	m_bOperationRunning = FALSE;
	m_bAskingCancel = FALSE;
	m_pLocal->Reload();
	m_pRemote->Reload();
	return FALSE;
}

// A menu entry was chosen.
void CFTPManager::HandleMenuChoice()
{
	IFileFolder *pane = m_bRightPane ? (IFileFolder *)m_pRemote : m_pLocal;
	std::string text;
	if (m_nState == FTPSTATE_SITES)
	{
		switch (m_pMenu->GetSelection())
		{
		case FTPSITE_CONNECT:
			m_bQuickConnect = FALSE;
			Connect();
			break;

		case FTPSITE_QUICK_CONNECT:
			if (m_pSiteEditor == NULL)
			{
				m_pSiteEditor = new CFTPSite;
				if (m_pSiteEditor != NULL)
					m_pSiteEditor->Init(m_pFont);
			}
			// The last quick connect's values, or the defaults.
			if (!m_bQuickConnect)
			{
				m_pSiteEditor->m_strName = "Quick Connect";
				m_pSiteEditor->m_strAddress = "";
				m_pSiteEditor->m_nPort = 21;
				m_pSiteEditor->m_strUser = "xbox";
				m_pSiteEditor->m_strPassword = "xbox";
				m_pSiteEditor->m_strRemotePath.clear();
				m_pSiteEditor->m_strLocalPath.clear();
				m_pSiteEditor->m_bPassive = TRUE;
			}
			m_pSiteEditor->m_bQuickConnect = TRUE;
			m_pSiteEditor->m_bNewSite = FALSE;
			m_pSiteEditor->Reset();
			m_bSiteEditorOpen = TRUE;
			m_bQuickConnect = TRUE;
			break;

		case FTPSITE_NEW:
			m_bQuickConnect = FALSE;
			if (m_pSiteEditor == NULL)
			{
				m_pSiteEditor = new CFTPSite;
				if (m_pSiteEditor != NULL)
					m_pSiteEditor->Init(m_pFont);
			}
			if (m_pSiteEditor != NULL)
			{
				m_pSiteEditor->m_bNewSite = TRUE;
				m_pSiteEditor->m_bQuickConnect = FALSE;
				m_pSiteEditor->Reset();
				m_bSiteEditorOpen = TRUE;
			}
			break;

		case FTPSITE_EDIT:
			m_bQuickConnect = FALSE;
			if (m_pSiteEditor == NULL)
			{
				m_pSiteEditor = new CFTPSite;
				if (m_pSiteEditor == NULL)
					break;
				m_pSiteEditor->Init(m_pFont);
			}
			if (m_siteList.m_pCurrent != NULL)
			{
				SFTPSiteInfo site = m_siteList.GetSelectedSite();
				StrFormat(m_pSiteEditor->m_strName, "%S", site.name);
				m_pSiteEditor->m_strAddress = site.host;
				m_pSiteEditor->m_nPort = site.port;
				m_pSiteEditor->m_strUser = site.user;
				m_pSiteEditor->m_strPassword = site.password;
				m_pSiteEditor->m_bPassive = site.passive;
				m_pSiteEditor->m_strRemotePath = site.remotePath;
				m_pSiteEditor->m_strLocalPath = site.localPath;
				m_pSiteEditor->m_bNewSite = FALSE;
				m_pSiteEditor->m_bQuickConnect = FALSE;
				m_pSiteEditor->Reset();
				m_bSiteEditorOpen = TRUE;
			}
			break;

		case FTPSITE_DELETE:
			if (m_siteList.m_pCurrent != NULL)
			{
				StrFormat(text, "%S", m_siteList.m_pCurrent->m_pszName);
				m_siteList.DeleteSite(text.c_str());
			}
			break;

		case FTPSITE_REFRESH:
			ScanNetworkClients();
			m_siteList.Reload();
			break;

		case FTPSITE_VIEW_LOG:
			m_bLogShown = TRUE;
			OpenLog();
			break;

		case FTPSITE_LOCAL_BROWSER:
			m_bOpen = FALSE;
			break;
		}
		return;
	}

	int choice = m_pMenu->GetSelection();
	switch (choice)
	{
	case FTPMENU_TRANSFER:
		// The selection too when the queue is empty.
		if (g_ftpQueue.size() == 0)
			QueueSelection();
		g_bFtpStartQueue = TRUE;
		break;

	case FTPMENU_ADD_TO_QUEUE:
		QueueSelection();
		break;

	case FTPMENU_DELETE:
		if (!m_bRightPane)
		{
			if (CFileManager::StartCollect(pane))
				m_nState = FTPSTATE_COLLECT;
		}
		else
		{
			// The FTP pane deletes trees itself: nothing to list.
			g_nOperationFolders = 0;
			g_nOperationFiles = 0;
			if (m_pRemote->HasMarks() || (m_pRemote->m_pCurrent != NULL && !m_pRemote->m_pCurrent->m_bParent))
			{
				g_bFilesCollected = TRUE;
				g_nOperationFolders = 1;
			}
			m_nState = FTPSTATE_COLLECT;
		}
		if (m_nState == FTPSTATE_COLLECT)
			ShowMessage(GetStringA(STR_MSG_DELETE_CHECK), GetString(STR_INFORMATION), MSGBOX_BUTTONS_CANCEL);
		else
			ShowMessage("Error Creating FileCopy Thread", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		break;

	case FTPMENU_RENAME:
	case FTPMENU_CREATE_FOLDER:
	case FTPMENU_RAW_COMMAND:
		m_bKeyboardOpen = TRUE;
		if (g_virtualKeyboard.m_bNumeric)
			g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bNumeric = FALSE;
		g_virtualKeyboard.Open();
		g_virtualKeyboard.m_nMaxLength = 42;
		if (choice == FTPMENU_RAW_COMMAND)
		{
			g_virtualKeyboard.SetTitle(L"Raw Site Command");
			m_nState = FTPSTATE_RAW_COMMAND;
			g_virtualKeyboard.m_nMaxLength = 80;
		}
		else if (choice == FTPMENU_CREATE_FOLDER)
		{
			g_virtualKeyboard.SetTitle(L"Create Folder");
			m_nState = FTPSTATE_NEW_FOLDER;
		}
		else
		{
			g_virtualKeyboard.SetTitle(L"Rename");
			g_virtualKeyboard.SetText(pane->m_pCurrent->m_pszName);
			m_nState = FTPSTATE_RENAME;
		}
		g_virtualKeyboard.m_bPassword = FALSE;
		break;

	case FTPMENU_REBOOT:
	case FTPMENU_SHUTDOWN:
	case FTPMENU_POWER_CYCLE:
	case FTPMENU_CLOSE_TRAY:
	case FTPMENU_EJECT_TRAY:
	case FTPMENU_DRIVE_STATUS:
	{
		// UnleashX's own server commands (FtpEvents.cpp on the other side).
		static const char *const commands[] = {
			"SITE REBOOT", "SITE SHUTDOWN", "SITE POWERCYCLE", "SITE TRAYCLOSE", "SITE EJECT", "SITE DRIVESTAT",
		};
		text = commands[choice - FTPMENU_REBOOT];
		m_pRemote->SendRawCommand(text.c_str());
		std::string reply = m_pRemote->m_strReply;
		ShowMessage(reply.c_str(), L"Site Response", MSGBOX_BUTTONS_OK);
		break;
	}

	case FTPMENU_VIEW_LOG:
		m_bLogShown = TRUE;
		OpenLog();
		break;

	case FTPMENU_REFRESH:
		m_pRemote->Reload();
		break;

	case FTPMENU_DISCONNECT:
		m_pRemote->Disconnect();
		delete m_pRemote;
		m_pRemote = NULL;
		m_nState = FTPSTATE_SITES;
		LayoutPanes();
		break;
	}
}

// 0x00124A37
HRESULT CFTPManager::ProcessInput(int *action, SGamepadState **pad)
{
	IFileFolder *pane = m_bRightPane ? (IFileFolder *)m_pRemote : m_pLocal;
	if (m_bOperationRunning && !HandleOperation(action))
		return S_OK;

	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		if (!m_pMessageBox->IsOpen())
		{
			*action = INPUT_NONE;
			m_bMessageShown = FALSE;
			if (m_bAskingCancel)
			{
				if (m_pMessageBox->GetResult() == MSGBOX_YES)
				{
					m_nState = FTPSTATE_CANCELLING;
					Sleep(100);
					g_bCancelOperation = TRUE;
				}
				m_bAskingCancel = FALSE;
			}
			else if (m_nState == FTPSTATE_DELETE && m_pMessageBox->GetChosenButton() == 0)
			{
				StartDeleteThread(pane);
				m_bOperationRunning = TRUE;
			}
			else if (m_nState != FTPSTATE_SITES)
			{
				if (m_nState == FTPSTATE_ASK_EXIT && m_pMessageBox->GetChosenButton() == 0)
					m_bOpen = FALSE;
				m_nState = FTPSTATE_BROWSE;
			}
		}
		// Listing what to delete: ask once it's done.
		if (m_nState != FTPSTATE_COLLECT || pane == NULL || !pane->m_bCollected)
			return S_OK;
		if (!g_bFilesCollected || g_bCancelOperation)
		{
			m_bMessageShown = FALSE;
			return S_OK;
		}
		if (g_nOperationFolders == 0 && g_nOperationFiles == 0)
		{
			ShowMessage("Invalid selection. Please select at least one file to delete.", GetString(STR_ERROR),
			            MSGBOX_BUTTONS_OK);
			m_nState = FTPSTATE_BROWSE;
			return S_OK;
		}
		std::string message;
		if (!m_bRightPane)
		{
			std::string value;
			const char *unit;
			FormatTotal(g_ullOperationTotal, value, unit);
			StrFormat(message, GetStringA(STR_MSG_ABOUT_TO_DELETE), FormatInt((int)g_nOperationFiles).c_str(),
			          FormatInt((int)g_nOperationFolders).c_str(), value.c_str(), unit);
		}
		else
		{
			message = "Are you sure you want to delete the selected object(s)?";
		}
		ShowMessage(message.c_str(), GetString(STR_CONFIRM), MSGBOX_BUTTONS_OKCANCEL);
		if (m_pMessageBox != NULL)
		{
			m_pMessageBox->SetSelection(MSGBOX_CANCEL);
			m_nState = FTPSTATE_DELETE;
		}
		return S_OK;
	}
	if (m_bOperationRunning)
		return S_OK;

	if (m_bLogShown)
	{
		g_ftpLog.ProcessInput(action, pad);
		m_bLogShown = g_ftpLog.m_bOpen;
		return S_OK;
	}

	if (m_bSiteEditorOpen)
	{
		m_pSiteEditor->HandleInput(action, pad);
		m_bSiteEditorOpen = m_pSiteEditor->IsOpen();
		if (!m_bSiteEditorOpen)
			FinishSiteEditor();
		return S_OK;
	}

	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (!m_bKeyboardOpen)
			FinishKeyboard();
		return S_OK;
	}

	if (m_bMenuOpen)
	{
		m_pMenu->ProcessInput(action, pad);
		m_bMenuOpen = m_pMenu->IsOpen();
		if (!m_bMenuOpen && m_pMenu->m_bChosen)
			HandleMenuChoice();
		*action = INPUT_NONE;
		return S_OK;
	}

	int state = m_nState;
	if (state == FTPSTATE_CONNECTING)
	{
		if (m_pRemote->m_bConnecting)
			return S_OK;
		if (m_pRemote->m_bConnected)
		{
			LayoutPanes();
			m_pMenu = &m_remoteMenu;
			m_nState = FTPSTATE_BROWSE;
			return S_OK;
		}
		std::string message;
		std::string reply = m_pRemote->m_strReply;
		StrFormat(message, "Unable to Connect to the server.\n%s", reply.c_str());
		ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		delete m_pRemote;
		m_pRemote = NULL;
		m_nState = FTPSTATE_SITES;
		m_pMenu = &m_siteMenu;
		return S_OK;
	}

	int a = *action;
	if (a == INPUT_NONE)
	{
		// The queue's "Transfer" (or the menu's).
		if (!g_bFtpStartQueue)
			return S_OK;
		m_bQueueActive = FALSE;
		UpdateActivePane();
		m_queue.m_nTop = 0;
		g_bFtpStartQueue = FALSE;
		if (!m_pRemote->StartTransfer())
			return S_OK;
		ShowMessage("Transfering...\n\nPlease DO NOT turn your xbox off.", L"Transfering", MSGBOX_BUTTONS_CANCEL);
		m_nState = FTPSTATE_TRANSFER;
		m_bOperationRunning = TRUE;
		return S_OK;
	}

	BOOL update = FALSE;
	switch (a)
	{
	case INPUT_START:
	case INPUT_WHITE:
		if (m_bQueueActive)
		{
			m_queue.ProcessInput(action);
			break;
		}
		{
			SFTPSiteInfo site = m_siteList.GetSelectedSite();
			if (state == FTPSTATE_SITES)
			{
				m_pMenu = &m_siteMenu;
				m_siteMenu.SetOptionEnabled(FTPSITE_CONNECT, m_bRightPane && m_siteList.m_pCurrent != NULL);
				// Only saved sites can be edited or deleted.
				m_siteMenu.SetOptionEnabled(FTPSITE_EDIT, site.saved);
				m_siteMenu.SetOptionEnabled(FTPSITE_DELETE, site.saved);
			}
			else
			{
				BOOL item = pane->m_pCurrent != NULL && !pane->m_pCurrent->m_bParent;
				BOOL marks = pane->HasMarks();
				m_pMenu->SetOptionEnabled(FTPMENU_TRANSFER, item || marks || g_ftpQueue.size() > 0);
				m_pMenu->SetOptionEnabled(FTPMENU_ADD_TO_QUEUE, item || marks);
				m_pMenu->SetOptionEnabled(FTPMENU_DELETE, item || marks);
				m_pMenu->SetOptionEnabled(FTPMENU_RENAME, item);
			}
			m_pMenu->Show(0);
			m_bMenuOpen = TRUE;
		}
		break;

	case INPUT_BACK:
		if (state == FTPSTATE_SITES)
		{
			m_bOpen = FALSE;
		}
		else
		{
			ShowMessage("Are you sure you want to disconnect and exit?", GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
			if (m_pMessageBox != NULL)
			{
				m_pMessageBox->SetSelection(0);
				m_nState = FTPSTATE_ASK_EXIT;
			}
		}
		break;

	case INPUT_LTRIGGER:
		m_bRightPane = FALSE;
		m_bQueueActive = FALSE;
		UpdateActivePane();
		m_pMenu = state != FTPSTATE_SITES ? &m_remoteMenu : &m_siteMenu;
		break;

	case INPUT_RTRIGGER:
		m_bQueueActive = FALSE;
		m_bRightPane = TRUE;
		update = TRUE;
		break;

	case INPUT_LTHUMB_CLICK:
		if (m_bQueueActive)
		{
			m_queue.ProcessInput(action);
			break;
		}
		m_bLogShown = TRUE;
		OpenLog();
		break;

	case INPUT_RTHUMB_CLICK:
		// The queue takes the input (or gives it back).
		if (state == FTPSTATE_SITES)
			break;
		m_bQueueActive = !m_bQueueActive;
		update = TRUE;
		break;

	case INPUT_LEFT:
	case INPUT_IR_LEFT:
		// Left: from the queue to the right pane, from the right pane to
		// the left one, from the left one down to the queue.
		update = TRUE;
		if (state == FTPSTATE_SITES)
		{
			m_bRightPane = FALSE;
		}
		else if (state == FTPSTATE_BROWSE)
		{
			if (m_bQueueActive)
			{
				m_bQueueActive = FALSE;
				m_bRightPane = TRUE;
			}
			else if (!m_bRightPane)
			{
				m_bQueueActive = TRUE;
			}
			else
			{
				m_bRightPane = FALSE;
			}
		}
		else
		{
			update = FALSE;
		}
		break;

	case INPUT_RIGHT:
	case INPUT_IR_RIGHT:
		update = TRUE;
		if (state == FTPSTATE_SITES)
		{
			m_bRightPane = TRUE;
		}
		else if (state == FTPSTATE_BROWSE)
		{
			if (m_bQueueActive)
			{
				m_bQueueActive = FALSE;
				m_bRightPane = FALSE;
			}
			else if (!m_bRightPane)
			{
				m_bRightPane = TRUE;
			}
			else
			{
				m_bQueueActive = TRUE;
			}
		}
		else
		{
			update = FALSE;
		}
		break;

	default:
		if (state == FTPSTATE_SITES)
		{
			if (!m_bRightPane)
			{
				m_pLocal->ProcessInput(action, pad);
			}
			else if (a == INPUT_A)
			{
				m_bQuickConnect = FALSE;
				Connect();
			}
			else
			{
				m_siteList.ProcessInput(action, pad);
			}
			break;
		}
		if (state == FTPSTATE_TRANSFER)
		{
			if (m_pRemote->m_bTransferDone)
			{
				m_nState = FTPSTATE_BROWSE;
				m_pLocal->Reload();
				m_pRemote->Reload();
			}
			break;
		}
		if (m_bQueueActive)
		{
			m_queue.ProcessInput(action);
			m_bQueueActive = m_queue.m_bActive;
			if (!m_bQueueActive)
				update = TRUE;
			break;
		}
		if (!m_bRightPane)
		{
			m_pLocal->ProcessInput(action, pad);
			break;
		}
		m_pRemote->ProcessInput(action, pad);
		if (!m_pRemote->IsActive())
		{
			// (The pane gave up the input: back to the sites.)
			delete m_pRemote;
			m_pRemote = NULL;
			m_nState = FTPSTATE_SITES;
			return S_OK;
		}
		break;
	}
	if (update)
		UpdateActivePane();
	*action = INPUT_NONE;
	return S_OK;
}
