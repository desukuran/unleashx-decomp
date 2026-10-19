// CFTPManager: construction, setup, layout, drawing, connecting and
// queueing. The input is in FtpManagerInput.cpp. See FtpManager.h.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"
#include "../xml/XmlDocument.h"
#include "FtpManager.h"

CAppSettings *g_pFtpSettings = NULL;   // 0x00270FA0

// A pane frame: in the dialog colors, width 1 or 2 (the active one).
// (0x0011F6A8)
static void DrawPaneFrame(float x, float y, float width, float height, DWORD borderWidth)
{
	DrawRect(x, y, width, height, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, borderWidth);
}

// 0x00123641 (this in ebx)
CFTPManager::CFTPManager()
{
	g_pFtpSettings = CAppSettings::GetInstance();
	m_hUnknown008 = INVALID_HANDLE_VALUE;
	m_bSafeBorder = TRUE;
	m_bRightPane = FALSE;
	m_pBackground = NULL;
	m_pLocal = NULL;
	m_pRemote = NULL;
	m_unknown050 = 0;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_bKeyboardOpen = FALSE;
	m_bLogShown = FALSE;
	m_bDrawBorder = FALSE;
	m_bMenuOpen = FALSE;
	m_bOpen = FALSE;
	m_bInitialized = FALSE;
	m_fQueueHeight = 0.0f;
	g_ftpLog.Clear();
	g_ftpLog.Add("UnleashX FTP Client", FTPLOG_STATUS);
	g_ftpLog.Add(" ", FTPLOG_STATUS);
	g_ftpQueue.clear();
	m_pMenu = NULL;
	m_bSiteEditorOpen = FALSE;
	m_pSiteEditor = NULL;
	m_bQueueActive = FALSE;
	m_bQuickConnect = FALSE;
	m_bOperationRunning = FALSE;
	m_bAskingCancel = FALSE;

	// <Settings><FTPClient MinPort MaxPort DetectPeer>, read once.
	SFTPSettings &ftp = g_pFtpSettings->m_ftp;
	if (ftp.m_nClientMinPort == 0)
	{
		g_config.m_pCurrent = &g_config.m_root;
		CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
		if (settings != NULL)
		{
			CXmlElement *client = g_config.FindElement(settings, "FTPClient");
			if (client != NULL)
			{
				ftp.m_nClientMinPort = atol(client->GetAttribute("MinPort").c_str());
				ftp.m_nClientMaxPort = atol(client->GetAttribute("MaxPort").c_str());
				ftp.m_bClientDetectPeers = !IsFalseString(client->GetAttribute("DetectPeer").c_str());
			}
		}
	}
}

// 0x001213F8
CFTPManager::~CFTPManager()
{
	if (m_hUnknown008 != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hUnknown008);
		m_hUnknown008 = INVALID_HANDLE_VALUE;
	}
	delete m_pMessageBox;
	m_pMessageBox = NULL;
	if (m_pBackground != NULL)
		ReleaseTexture(m_pBackground);
	delete m_pRemote;
	m_pRemote = NULL;
	delete m_pSiteEditor;
	m_pSiteEditor = NULL;
	g_ftpLog.Clear();
}

// 0x0001E872 (this in ebx)
void CFTPManager::LayoutWindow()
{
	if (m_bSafeBorder)
	{
		m_fY = 20.0f;
		m_fX = 30.0f;
	}
	else
	{
		m_fY = 5.0f;
		m_fX = 10.0f;
	}
	m_fHeight = 480.0f - m_fY * 2.0f;
	m_fQueueHeight = 120.0f;
	m_fWidth = 640.0f - m_fX * 2.0f;
	// (The height and the queue's are scaled by the factor that comes
	// back in scale.)
	float scale = 1.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &scale, TRUE);
	m_fHeight *= scale;
	m_fQueueHeight *= scale;
}

// 0x0011F362 (this in esi)
void CFTPManager::LayoutPanes()
{
	float scale = g_pFtpSettings->m_fFontScale;
	m_pFont->SetScaleFactors(scale, scale);
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	float header = m_fLineHeight + 8.0f;
	float top = m_fY + header + 19.0f;
	float height = m_fHeight;
	if (m_nState != FTPSTATE_SITES)
		height -= m_fQueueHeight;
	float margins = (header + 19.0f) * 2.0f;
	float half = (m_fWidth - 15.0f) * 0.5f;
	if (m_pLocal != NULL)
	{
		m_pLocal->SetRect(m_fX + 15.0f, top, half - 15.0f, height - margins);
		m_pLocal->m_bDetails = FALSE;
	}
	float rightX = half + m_fX + 15.0f;
	m_siteList.SetRect(rightX, top, half - 15.0f, m_fHeight - margins);
	if (m_pRemote != NULL)
	{
		m_pRemote->SetRect(rightX, top, half - 15.0f, height - margins);
		m_pRemote->m_bDetails = FALSE;
	}
	m_queue.m_fY = (height - margins) + top + 4.0f;
	m_queue.m_fX = m_fX + 15.0f;
	if (m_fWidth - 30.0f > 0.0f)
		m_queue.m_fWidth = m_fWidth - 30.0f;
	if (m_fQueueHeight > 0.0f)
		m_queue.m_fHeight = m_fQueueHeight;
}

// 0x0011F2DA (this in eax)
void CFTPManager::UpdateActivePane()
{
	if (m_nState == FTPSTATE_SITES)
	{
		if (!m_bRightPane)
		{
			if (m_pLocal != NULL)
				m_pLocal->m_bActive = TRUE;
			m_siteList.m_bActive = FALSE;
		}
		else
		{
			m_siteList.m_bActive = TRUE;
			if (m_pLocal != NULL)
				m_pLocal->m_bActive = FALSE;
		}
		return;
	}
	if (m_bQueueActive)
	{
		m_queue.m_bActive = TRUE;
		m_pRemote->m_bActive = FALSE;
		m_pLocal->m_bActive = FALSE;
	}
	else
	{
		m_queue.m_bActive = FALSE;
		m_pLocal->m_bActive = !m_bRightPane;
		m_pRemote->m_bActive = m_bRightPane;
	}
}

// 0x0011F5A2
void CFTPManager::Show()
{
	m_bMenuOpen = FALSE;
	m_bKeyboardOpen = FALSE;
	m_bMessageShown = FALSE;
	m_bLogShown = FALSE;
	LayoutWindow();
	LayoutPanes();
	m_pLocal->Reload();
	m_bOpen = TRUE;
}

// 0x001219B8 (background in eax, local in ecx)
void CFTPManager::Setup(CXBFontEx *font, IDirect3DTexture8 *background, IFileFolder *local)
{
	if (m_bInitialized)
		return;
	if (m_pBackground != NULL)
		ReleaseTexture(m_pBackground);
	if (background != NULL)
	{
		background->AddRef();
		m_pBackground = background;
	}
	if (local == NULL || font == NULL)
		return;
	if (FAILED(m_siteList.Init(font, 0, 0, NULL, NULL)))
		return;
	m_siteList.Reload();
	m_pLocal = local;
	LayoutWindow();
	m_pLocal->m_bActive = FALSE;
	m_pFont = font;
	m_siteList.m_bActive = TRUE;
	m_fFontScaleX = font->m_fScaleFactorX;
	m_fFontScaleY = font->m_fScaleFactorY;
	m_fLineHeight = font->m_fFontHeight * font->m_fScaleFactorY;
	LayoutPanes();

	m_siteMenu.Create(font, L"Site", 0.0f, 0.0f, 0.0f, 0.0f, 10);
	m_siteMenu.AddOption(L"Connect", 0);
	m_siteMenu.AddOption(L"Quick Connect...", 1);
	m_siteMenu.AddOption(L"", 2);
	m_siteMenu.AddOption(L"New Site...", 3);
	m_siteMenu.AddOption(L"Edit Site...", 4);
	m_siteMenu.AddOption(L"Delete Site", 5);
	m_siteMenu.AddOption(L"Refresh", 6);
	m_siteMenu.AddOption(L"", 7);
	m_siteMenu.AddOption(L"View Log...", 8);
	m_siteMenu.AddOption(L"Switch to Local Browser", 9);
	m_siteMenu.SetOptionEnabled(FTPSITE_EDIT, FALSE);
	m_siteMenu.SetOptionEnabled(FTPSITE_DELETE, FALSE);
	m_pMenu = &m_siteMenu;

	m_remoteMenu.Create(font, L"Site", 0.0f, 0.0f, 0.0f, 0.0f, 10);
	m_remoteMenu.AddOption(L"Transfer", 0);
	m_remoteMenu.AddOption(L"Add to Queue", 1);
	m_remoteMenu.AddOption(L"", 2);
	m_remoteMenu.AddOption(L"Delete...", 3);
	m_remoteMenu.AddOption(L"Rename...", 4);
	m_remoteMenu.AddOption(L"Create Folder...", 5);
	m_remoteMenu.AddOption(L"", 6);
	m_remoteMenu.AddOption(L"Site Raw Command...", 7);
	m_remoteMenu.AddOption(L"Reboot", 8);
	m_remoteMenu.AddOption(L"Shutdown", 9);
	m_remoteMenu.AddOption(L"Power Cycle", 10);
	m_remoteMenu.AddOption(L"Close Tray", 11);
	m_remoteMenu.AddOption(L"Eject Tray", 12);
	m_remoteMenu.AddOption(L"Drive Status", 13);
	m_remoteMenu.AddOption(L"", 14);
	m_remoteMenu.AddOption(L"View Log...", 15);
	m_remoteMenu.AddOption(L"Refresh", 16);
	m_remoteMenu.AddOption(L"Disconnect", 17);

	m_queue.Init(m_pFont);
	m_nState = FTPSTATE_SITES;
	m_bInitialized = TRUE;
	m_bOpen = TRUE;
	m_bRightPane = TRUE;
	g_ftpLog.Init(font);
}

// 0x00121871 (this in edi)
void CFTPManager::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
		}
		else
		{
			m_pMessageBox->SetButtonText(MSGBOX_OK, GetString(STR_OK));
			m_pMessageBox->SetButtonText(MSGBOX_CANCEL, GetString(STR_CANCEL));
			m_pMessageBox->SetButtonText(MSGBOX_ABORT, GetString(STR_ABORT));
			m_pMessageBox->SetButtonText(MSGBOX_RETRY, GetString(STR_RETRY));
			m_pMessageBox->SetButtonText(MSGBOX_IGNORE, GetString(STR_IGNORE));
			m_pMessageBox->SetButtonText(MSGBOX_YES, GetString(STR_YES));
			m_pMessageBox->SetButtonText(MSGBOX_NO, GetString(STR_NO));
		}
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageShown = TRUE;
	}
}

// 0x00122479
void CFTPManager::Connect()
{
	SFTPSiteInfo site = m_siteList.GetSelectedSite();
	if (!m_bQuickConnect && (site.port == 0 || strlen(site.host) == 0))
	{
		ShowMessage("Invalid FTP Server Information", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return;
	}
	delete m_pRemote;
	m_pRemote = new CFTPFileFolder;
	if (m_pRemote == NULL)
		return;
	if (!m_bQuickConnect || m_pSiteEditor == NULL)
	{
		// (The strings go through _strdup copies that leak.)
		m_pRemote->m_strHost = site.host;
		m_pRemote->m_nPort = site.port;
		m_pRemote->m_strUser = site.user;
		m_pRemote->m_strPassword = site.password;
		m_pRemote->m_bPassive = site.passive;
		m_strRemotePath = site.remotePath;
		if (site.localPath.size() != 0 && DirectoryExists(site.localPath.c_str()))
			m_pLocal->SetPath(site.localPath.c_str());
	}
	else
	{
		m_pRemote->m_strHost = m_pSiteEditor->m_strAddress;
		m_pRemote->m_nPort = m_pSiteEditor->m_nPort;
		m_pRemote->m_strUser = m_pSiteEditor->m_strUser;
		m_pRemote->m_strPassword = m_pSiteEditor->m_strPassword;
		m_pRemote->m_bPassive = m_pSiteEditor->m_bPassive;
		if (m_pSiteEditor->m_strLocalPath.size() != 0 && DirectoryExists(m_pSiteEditor->m_strLocalPath.c_str()))
			m_pLocal->SetPath(m_pSiteEditor->m_strLocalPath.c_str());
		m_strRemotePath = m_pSiteEditor->m_strRemotePath;
	}
	m_pRemote->Init(m_pFont, 0, m_pLocal->m_nMaxNameLength, m_pLocal->m_pFolderIcon, m_pLocal->m_pFileIcon);
	if (m_strRemotePath.size() != 0)
		m_pRemote->m_strPath = m_strRemotePath;
	// Connects on the listing thread.
	m_pRemote->Reload();
	Sleep(50);
	m_nState = FTPSTATE_CONNECTING;
}

// 0x00120E06
void CFTPManager::QueueSelection()
{
	std::string sourceDir, targetDir;
	BOOL withNames = TRUE;   // FALSE: a drive of the local Root goes as a whole
	BOOL upload;
	IFileFolder *source;
	if (!m_bRightPane)
	{
		// Up: the local pane's items into the FTP folder.
		source = m_pLocal;
		upload = TRUE;
		sourceDir = m_pLocal->GetCurrentPath();
		targetDir = m_pRemote->GetCurrentPath();
		if (targetDir.size() == 0 || StrLeft(targetDir, 1).compare("/") != 0)
			targetDir = "/";   // (Quirk: not "/" + it.)
		if (StrRight(targetDir, 1).compare("/") != 0)
			targetDir.append("/");
		if (StrFind(sourceDir, ":") == -1)
		{
			// Root: the highlighted drive.
			SFolderItem *drive = m_pLocal->m_pCurrent;
			if (drive == NULL || !drive->m_bFolder || drive->m_bParent)
				return;
			StrFormat(sourceDir, "%S", drive->m_pszName);
			if (StrFind(sourceDir, ":") == -1)
				sourceDir.append(":\\");
			withNames = FALSE;
		}
		StrAddBackslash(sourceDir);
	}
	else
	{
		// Down: the FTP items into the local folder.
		source = m_pRemote;
		upload = FALSE;
		sourceDir = m_pRemote->GetCurrentPath();
		if (sourceDir.size() == 0 || StrLeft(sourceDir, 1).compare("/") != 0)
			sourceDir = "/" + sourceDir;
		if (StrRight(sourceDir, 1).compare("/") != 0)
			sourceDir.append("/");
		targetDir = m_pLocal->GetCurrentPath();
		if (StrFind(targetDir, ":") == -1)
		{
			SFolderItem *drive = m_pLocal->m_pCurrent;
			if (drive == NULL || !drive->m_bFolder || drive->m_bParent)
				return;
			StrFormat(targetDir, "%S", drive->m_pszName);
			if (StrFind(targetDir, ":") == -1)
				targetDir.append(":\\");
		}
		StrAddBackslash(targetDir);
	}

	// The marked items, or else the highlighted one.
	std::vector<SFolderItem *> items;
	for (std::vector<SFolderItem *>::iterator it = source->m_items.begin(); it != source->m_items.end(); ++it)
	{
		if (*it != NULL && (*it)->m_bMarked && !(*it)->m_bParent)
			items.push_back(*it);
	}
	if (items.size() == 0 && source->m_pCurrent != NULL && !source->m_pCurrent->m_bParent)
		items.push_back(source->m_pCurrent);
	std::string from, to;
	for (std::vector<SFolderItem *>::iterator it = items.begin(); it != items.end(); ++it)
	{
		SFolderItem *item = *it;
		if (withNames)
			StrFormat(from, "%s%S", sourceDir.c_str(), item->m_pszName);
		else
			StrFormat(from, "%s", sourceDir.c_str());
		StrFormat(to, "%s%S", targetDir.c_str(), item->m_pszName);
		if (item->m_bFolder)
		{
			to.append(upload ? "/" : "\\");
			from.append(upload ? "\\" : "/");
		}
		StrReplaceAll(from, "//", "/");
		StrReplaceAll(to, "//", "/");
		m_queue.Add(from.c_str(), to.c_str(), item->m_bFolder, item->m_dwSize, upload);
	}
}

// 0x00121C85
BOOL CFTPManager::Render()
{
	if (!m_bInitialized)
		return E_FAIL;
	if (m_bLogShown)
		return g_ftpLog.Render();
	float scaleX = m_pFont->m_fScaleFactorX;
	float scaleY = m_pFont->m_fScaleFactorY;
	float scale = g_pFtpSettings->m_fFontScale;
	m_pFont->SetScaleFactors(scale, scale);
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	IFileFolder *pane = m_pLocal;
	if (m_bRightPane)
		pane = (m_nState != FTPSTATE_SITES && m_pRemote != NULL) ? (IFileFolder *)m_pRemote : &m_siteList;
	BOOL frames = m_bDrawBorder || m_pBackground == NULL;

	// The header: the active pane's folder and counts (none for the sites).
	float y = m_fY + 15.0f;
	if (frames)
		DrawPaneFrame(m_fX + 15.0f, y, m_fWidth - 30.0f, m_fLineHeight + 8.0f, 1);
	if (!m_bRightPane || m_nState != FTPSTATE_SITES)
	{
		wchar_t header[50];
		if (!m_bQueueActive)
			swprintf(header, 50, L"%ls\t%u Files, %u Folders (%ls)", pane->GetTitle(), pane->m_nFileCount,
			         pane->m_nFolderCount, pane->m_szTotalSize);
		else
			wcscpy(header, L"Transfer Queue");
		m_pFont->DrawText(m_fX + 17.0f, m_fLineHeight * 0.5f + y + 4.0f, g_colorScheme.OPTION_ACTIVETEXTCOLOR, header,
		                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - 34.0f);
	}

	// The footer: the highlighted item.
	float footerHeight = m_fLineHeight + 8.0f;
	float footerY = ((m_fY + m_fHeight) - 15.0f) - footerHeight;
	if (frames)
		DrawPaneFrame(m_fX + 15.0f, footerY, m_fWidth - 30.0f, footerHeight, 1);
	SFolderItem *current = pane->m_pCurrent;
	if (current != NULL)
	{
		wchar_t info[60];
		memset(info, 0, sizeof(info));
		wcscpy(info, current->m_szSize);
		wcscat(info, L"  ");
		wcscat(info, current->m_szDate);
		float infoWidth = m_pFont->GetTextWidth(info) + 4.0f;
		m_pFont->DrawText(m_fX + 17.0f, m_fLineHeight * 0.5f + footerY + 4.0f, g_colorScheme.OPTION_ACTIVETEXTCOLOR,
		                  current->m_pszName, XBFONT_CENTER_Y | XBFONT_TRUNCATED, (m_fWidth - 34.0f) - infoWidth);
		m_pFont->DrawText((m_fWidth + m_fX) - 17.0f, m_fLineHeight * 0.5f + footerY + 4.0f,
		                  g_colorScheme.OPTION_ACTIVETEXTCOLOR, info, XBFONT_CENTER_Y | XBFONT_TRUNCATED | XBFONT_RIGHT,
		                  infoWidth);
	}

	// The panes' frames (over a background, only the queue's).
	if (!frames)
	{
		if (m_nState != FTPSTATE_SITES)
			DrawPaneFrame(m_queue.m_fX, m_queue.m_fY, m_queue.m_fWidth, m_queue.m_fHeight, m_queue.m_bActive ? 2 : 1);
	}
	else if (m_nState == FTPSTATE_SITES)
	{
		DrawPaneFrame(m_pLocal->m_fX, m_pLocal->m_fY, m_pLocal->m_fWidth, m_pLocal->m_fHeight, !m_bRightPane ? 2 : 1);
		DrawPaneFrame(m_siteList.m_fX, m_siteList.m_fY, m_siteList.m_fWidth, m_siteList.m_fHeight,
		              m_bRightPane ? 2 : 1);
	}
	else
	{
		DrawPaneFrame(m_pLocal->m_fX, m_pLocal->m_fY, m_pLocal->m_fWidth, m_pLocal->m_fHeight,
		              m_pLocal->IsActive() ? 2 : 1);
		if (m_nState != FTPSTATE_CONNECTING)
		{
			DrawPaneFrame(m_pRemote->m_fX, m_pRemote->m_fY, m_pRemote->m_fWidth, m_pRemote->m_fHeight,
			              m_pRemote->IsActive() ? 2 : 1);
			DrawPaneFrame(m_queue.m_fX, m_queue.m_fY, m_queue.m_fWidth, m_queue.m_fHeight, m_queue.m_bActive ? 2 : 1);
		}
	}

	m_pLocal->Render();
	if (m_nState == FTPSTATE_SITES)
	{
		m_siteList.Render();
	}
	else
	{
		m_pRemote->Render();
		if (m_nState != FTPSTATE_TRANSFER)
			m_queue.Render();
	}
	if (m_bMessageShown && m_pMessageBox != NULL)
		m_pMessageBox->Render();

	switch (m_nState)
	{
	case FTPSTATE_SITES:
		if (m_bMenuOpen)
			m_pMenu->Render();
		else if (m_bSiteEditorOpen)
			m_pSiteEditor->Render();
		break;
	case FTPSTATE_CONNECTING:
		DrawBusyBox(m_pFont, GetString(STR_HTTP_CONNECTING));
		break;
	case FTPSTATE_BROWSE:
		if (m_bMenuOpen)
			m_pMenu->Render();
		break;
	case FTPSTATE_CANCELLING:
		DrawBusyBox(m_pFont, L"Please Wait. Canceling");
		break;
	case FTPSTATE_RENAME:
	case FTPSTATE_NEW_FOLDER:
	case FTPSTATE_RAW_COMMAND:
		if (m_bKeyboardOpen)
			g_virtualKeyboard.Render();
		break;
	default:
		// The progress, under the message box (kept on the screen).
		if (m_bOperationRunning && !m_bAskingCancel)
		{
			CMessageBox *box = m_pMessageBox;
			float x = m_fX + 20.0f;
			float width = m_fWidth - 40.0f;
			float height = m_fLineHeight * 6.0f + 20.0f;
			float top = box->m_fY + box->m_fHeight + 10.0f;
			if (m_fY + m_fHeight < height + top)
			{
				top = ((m_fY + m_fHeight) - height) - 10.0f;
				box->m_fY = (top - box->m_fHeight) - 10.0f;
			}
			int percent = (int)g_dFilePercent;
			top -= 50.0f;
			if (m_nState == FTPSTATE_TRANSFER)
			{
				height += m_fLineHeight + 10.0f;
				wchar_t from[310], to[310];
				swprintf(from, 310, L"From: %ls", g_szOperationFile);
				swprintf(to, 310, L"To: %ls", g_szOperationTarget);
				DrawTransferBox(m_pFont, x, top, width, height, L"Transfering", from, to, (float)(unsigned)percent, TRUE);
			}
			else if (m_nState == FTPSTATE_DELETE)
			{
				DrawTransferBox(m_pFont, x, top, width, height, L"Deleting", g_szOperationFile, NULL, -1.0f, FALSE);
			}
		}
		break;
	}
	m_pFont->SetScaleFactors(scaleX, scaleY);
	return S_OK;
}
