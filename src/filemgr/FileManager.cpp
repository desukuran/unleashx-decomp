// CFileManager: set-up, layout, drawing, message boxes and the collect
// thread. The input is in FileManagerInput.cpp, the operations in
// FileManagerOps.cpp, opening files in FileManagerOpen.cpp.
#include "FileManager.h"

#include <cstring>
#include <cwchar>

#include "../app/UnleashX.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../media/WMVideoPlayer.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"
#include "FtpManager.h"
#include "Viewers.h"

BOOL g_bFileAudioPlayer = FALSE;
BOOL g_bFileImageViewer = FALSE;
CWMVideoPlayer *g_pFileVideoPlayer = NULL;
BOOL g_bFileZipOpen = FALSE;
BOOL g_bFileRarOpen = FALSE;
BOOL g_bFileXipOpen = FALSE;
BOOL g_bFileVideoPlaying = FALSE;
BOOL g_bFileTextEditor = FALSE;
BOOL g_bFtpManagerOpen = FALSE;
CFTPManager *g_pFtpManager = NULL;
BOOL g_bFileViewerOpen = FALSE;
CAppFrame *g_pFileViewer = NULL;
BOOL g_bFilesCollected = FALSE;
BOOL g_bCopyResume = FALSE;
CAppSettings *g_pFileSettings = NULL;
BOOL g_bDeleteViewerOnClose = TRUE;

// The frame margins, worked out by Layout(). 0x00270F90..0x00270F9C
static float s_fMarginX;        // 15 pixels, scaled (0x00270F90 and 0x00270F98)
static float s_fGap;            // 4 pixels (0x00270F94)
static float s_fMarginY;        // 15 pixels (0x00270F9C)

// A frame of the dialog colors; style 1 is a plain border, 2 the active
// pane's. (0x00126774)
static void DrawFrameBox(float x, float y, float width, float height, DWORD style)
{
	DrawRect(x, y, width, height, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, style);
}

// 0x0012631D
DWORD GetOperationError()
{
	DWORD error = GetLastError();
	if (error == 0)
		error = g_dwOperationError;
	return error;
}

// 0x001297AC (this in edi)
CFileManager::CFileManager()
{
	m_unknown004 = 0;
	m_unknown044 = 0;
	m_nState = FMSTATE_BROWSE;
	m_nPrevState = FMSTATE_BROWSE;
	m_nStep = FMSTEP_COLLECTING;
	m_bInitialized = FALSE;
	m_nActivePane = 0;
	m_pBackground = NULL;
	m_bMenuOpen = FALSE;
	m_bKeyboardOpen = FALSE;
	m_bMessageShown = FALSE;
	m_bSideBySide = TRUE;
	m_fLineHeight = 0.0f;
	m_fFontScaleX = 0.0f;
	m_fFontScaleY = 0.0f;
	m_pMessageBox = NULL;
	m_bOpen = TRUE;
	m_bViewLog = FALSE;
	m_bSafeBorder = TRUE;
	m_bDrawBorder = FALSE;
	m_pSortMenu = NULL;
	m_pFont = NULL;
	m_nMaxNameLength = 0;
	m_fY = m_fX = m_fWidth = m_fHeight = 0.0f;
	g_pFileSettings = CAppSettings::GetInstance();
}

// 0x00127A5C
CFileManager::~CFileManager()
{
	m_pFont = NULL;
	ReleaseTexture(m_pBackground);
	delete m_pMessageBox;
	m_pMessageBox = NULL;
	g_pFolderSelectSound = NULL;
	g_pFolderScrollSound = NULL;
	g_pFolderDoneSound = NULL;
	if (g_pFileVideoPlayer != NULL)
	{
		delete g_pFileVideoPlayer;
		g_pFileVideoPlayer = NULL;
	}
	if (m_pSortMenu != NULL)
	{
		delete m_pSortMenu;
		m_pSortMenu = NULL;
	}
}

// 0x001294E2 (font in ecx, maxNameLength in eax, this in edx)
HRESULT CFileManager::Init(CXBFontEx *font, int maxNameLength, int unused, IDirect3DTexture8 *background,
                           IDirect3DTexture8 *folderIcon, IDirect3DTexture8 *fileIcon)
{
	if (m_bInitialized)
		return S_OK;
	if (background != NULL)
	{
		background->AddRef();
		m_pBackground = background;
	}
	if (font == NULL)
		return E_FAIL;
	if (FAILED(m_panes[0].Init(font, unused, maxNameLength, folderIcon, fileIcon)) ||
	    FAILED(m_panes[1].Init(font, unused, maxNameLength, folderIcon, fileIcon)))
		return E_FAIL;
	m_panes[0].m_pEvents = &m_events;
	m_panes[1].m_pEvents = &m_events;
	SetDefaultRect();
	m_panes[1].m_bActive = FALSE;
	m_panes[0].m_bActive = TRUE;
	m_pFont = font;
	m_nMaxNameLength = maxNameLength + 2;
	m_fFontScaleX = font->m_fScaleFactorX;
	m_fFontScaleY = font->m_fScaleFactorY;
	m_fLineHeight = font->m_fFontHeight * font->m_fScaleFactorY;
	Layout();

	// The context menu (empty texts are separators).
	m_menu.Create(font, GetString(STR_SELECT), 0.0f, 0.0f, 0.0f, 0.0f, 10);
	m_menu.AddOption(GetString(STR_ROOT_FOLDER), FMMENU_ROOT);
	m_menu.AddOption(GetString(STR_SORT), FMMENU_SORT);
	m_menu.AddOption(L"", 0);
	m_menu.AddOption(GetString(STR_SELECT_ALL), FMMENU_SELECT_ALL);
	m_menu.AddOption(GetString(STR_CLEAR_SELECTION), FMMENU_CLEAR_SELECTION);
	m_menu.AddOption(GetString(STR_INVERSE_SELECTION), FMMENU_INVERT_SELECTION);
	m_menu.AddOption(L"", 0);
	m_menu.AddOption(GetString(STR_COPY), FMMENU_COPY);
	m_menu.AddOption(GetString(STR_COPY_RESUME), FMMENU_COPY_RESUME);
	m_menu.AddOption(GetString(STR_MOVE), FMMENU_MOVE);
	m_menu.AddOption(GetString(STR_DELETE), FMMENU_DELETE);
	m_menu.AddOption(GetString(STR_RENAME), FMMENU_RENAME);
	m_menu.AddOption(GetString(STR_NEW_FOLDER), FMMENU_NEW_FOLDER);
	m_menu.AddOption(GetString(STR_CALC_USED_SPACE), FMMENU_CALC_SIZE);
	m_menu.AddOption(L"", 0);
	m_menu.AddOption(GetString(STR_COPY_DISC), FMMENU_COPY_DISC);
	m_menu.AddOption(GetString(STR_QUICK_COPY_DISK), FMMENU_QUICK_COPY_DISC);
	m_menu.AddOption(L"", 0);
	m_menu.AddOption(GetString(STR_ADD_TO_ZIP_FILE), FMMENU_ADD_TO_ZIP);
	m_menu.AddOption(GetString(STR_PATCH_XBE_FILE), FMMENU_PATCH_XBE);
	m_menu.AddOption(GetString(STR_RENAME_XBE_TITLE), FMMENU_RENAME_XBE_TITLE);
	m_menu.AddOption(L"", 0);
	m_menu.AddOption(GetString(STR_NEW_TEXT_DOCUMENT), FMMENU_NEW_TEXT);
	m_menu.AddOption(GetString(STR_OPEN_WITH_TEXT_EDITOR), FMMENU_OPEN_TEXT);
	m_menu.AddOption(L"", 0);
	m_menu.AddOption(GetString(STR_FORMAT_DRIVE), FMMENU_FORMAT);
	m_menu.AddOption(L"", 0);
	m_menu.AddOption(GetString(STR_SWITCH_TO_FTP), FMMENU_FTP);
	m_bInitialized = TRUE;
	m_nState = FMSTATE_BROWSE;
	m_nPrevState = FMSTATE_BROWSE;
	return S_OK;
}

// 0x00014189 (this in eax)
void CFileManager::SetDefaultRect()
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
	m_fWidth = 640.0f - m_fX * 2.0f;
	m_fHeight = 480.0f - m_fY * 2.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);   // TODO: verify the arguments
}

// 0x00126362 (this in ebx)
void CFileManager::Layout()
{
	m_pFont->SetScaleFactors(g_pFileSettings->m_fFontScale, g_pFileSettings->m_fFontScale);
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	float unitX = 1.0f, unitY = 1.0f;
	ScaleToScreen(NULL, &unitX, NULL, &unitY, TRUE);
	s_fGap = unitY * 4.0f;
	s_fMarginY = unitY * 15.0f;
	s_fMarginX = unitX * 15.0f;
	float header = s_fGap * 2.0f + m_fLineHeight;
	float listY = m_fY + s_fGap + s_fMarginY + header;
	float listHeight = m_fHeight - (s_fGap + s_fMarginY + header) * 2.0f;
	if (m_bSideBySide)
	{
		float half = (m_fWidth - s_fMarginX) * 0.5f;
		m_panes[0].SetRect(s_fMarginX + m_fX, listY, half - s_fMarginX, listHeight);
		m_panes[1].SetRect(half + m_fX, listY, half - s_fMarginX, listHeight);
	}
	else
	{
		m_panes[0].SetRect(s_fMarginX + m_fX, listY, m_fWidth - s_fMarginX * 2.0f, listHeight);
		m_panes[1].SetRect(s_fMarginX + m_fX, listY, m_fWidth - s_fMarginX * 2.0f, listHeight);
	}
	// Dates only fit when a pane has the whole width.
	m_panes[0].m_bDetails = !m_bSideBySide;
	m_panes[1].m_bDetails = !m_bSideBySide;
}

// 0x0002B4C9 (this in eax)
void CFileManager::Reset()
{
	m_nState = FMSTATE_BROWSE;
	m_nPrevState = FMSTATE_BROWSE;
	m_bMenuOpen = FALSE;
	m_bKeyboardOpen = FALSE;
	m_bMessageShown = FALSE;
	SetDefaultRect();
	Layout();
	if (m_bSideBySide)
	{
		m_panes[0].Reload();
		m_panes[1].Reload();
	}
	else
	{
		ActivePane()->Reload();
	}
	m_bOpen = TRUE;
}

// 0x0002B4A1 (this in esi, texture in edi)
void CFileManager::SetBackground(IDirect3DTexture8 *texture)
{
	ReleaseTexture(m_pBackground);
	if (texture != NULL)
	{
		texture->AddRef();
		m_pBackground = texture;
	}
}

// 0x00128C01 (path in edi, this in esi)
void CFileManager::OpenPath(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return;
	std::string dir;
	ResolvePath(path, dir);
	if (m_bInitialized && DirectoryExists(dir.c_str()))
		ActivePane()->SetPath(dir.c_str());
}

// 0x00126839 (this in esi)
void CFileManager::DrawBackground()
{
	if (m_pBackground != NULL)
	{
		DrawTextureFullScreen(m_pBackground, 1.0f);
		return;
	}
	DimScreen(0.3f);
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 2);
}

// 0x0012939B (this in edi)
void CFileManager::ShowMessage(const char *text, const wchar_t *title, int buttons)
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

// 0x00129345 (this on the stack)
BOOL CFileManager::CreateTextEditor()
{
	if (g_pFileViewer == NULL)
	{
		CNotePad *editor = new CNotePad;
		g_pFileViewer = editor;
		if (editor != NULL)
			editor->Init(m_pFont, NULL, NULL, NULL);
	}
	return g_pFileViewer != NULL;
}

// 0x0012A2C4 (this in ebx)
void CFileManager::ShowResult(std::string text, const wchar_t *title)
{
	if (g_strOperationLog.size() != 0 && CreateTextEditor())
	{
		// Something was logged: offer it.
		text.append("\nWould you like to view the log file?");
		ShowMessage(text.c_str(), title, MSGBOX_BUTTONS_YESNO);
		if (m_bMessageShown)
		{
			m_pMessageBox->SetSelection(MSGBOX_OK);   // (the first button: Yes)
			m_bViewLog = ((CNotePad *)g_pFileViewer)->Load(g_strOperationLog.c_str());
		}
		return;
	}
	ShowMessage(text.c_str(), title, MSGBOX_BUTTONS_OK);
}

// 0x00128855 (pane in edi)
BOOL CFileManager::StartCollect(IFileFolder *pane)
{
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, CollectThread, pane, CREATE_SUSPENDED, NULL);
	if (thread == NULL || thread == INVALID_HANDLE_VALUE)
		return FALSE;
	pane->m_bCollected = FALSE;
	SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
	ResumeThread(thread);
	CloseHandle(thread);
	return TRUE;
}

// 0x0012824E
unsigned __stdcall CFileManager::CollectThread(void *param)
{
	IFileFolder *pane = (IFileFolder *)param;
	while (g_bFileOperationActive)
		Sleep(50);

	// Nothing changed since the last time: the list is still good.
	if (!g_bFolderListChanged && (g_nOperationFolders != 0 || g_nOperationFiles != 0) && g_fileList.size() != 0 &&
	    pane->HasMarks())
	{
		g_bFileOperationPaused = TRUE;
		g_bFilesCollected = TRUE;
		g_bCancelOperation = FALSE;
		pane->m_bCollected = TRUE;
		return 0;
	}

	g_fileList.clear();
	g_nOperationFolders = 0;
	g_nOperationFiles = 0;
	g_bFileOperationActive = TRUE;
	g_ullOperationTotal = 0;
	g_ullOperationBytes = 0;
	g_bFileOperationPaused = FALSE;
	g_bFilesCollected = FALSE;
	g_bCancelOperation = FALSE;

	// The folder with its separator ('/' for FTP panes).
	std::string dir = pane->GetCurrentPath();
	char separator = StrFind(dir, "/") != -1 ? '/' : '\\';
	if (StrRight(dir, 1)[0] != separator)
	{
		dir.append(1, separator);
		if (separator == '/' && StrLeft(dir, 1)[0] != '/')
			dir = "/";
	}
	StrReplaceAll(dir, "Root\\", "");
	StrReplaceAll(dir, "Root/", "/");

	// The marked items; with none, the highlighted one.
	for (int pass = 0; pass < 2; pass++)
	{
		std::vector<SFolderItem *> items;
		if (pass == 0)
		{
			items = pane->m_items;
		}
		else
		{
			if (g_bCancelOperation || g_nOperationFiles != 0 || g_nOperationFolders != 0 || pane->m_pCurrent == NULL ||
			    pane->m_pCurrent->m_bParent)
				break;
			items.push_back(pane->m_pCurrent);
		}
		for (std::vector<SFolderItem *>::iterator it = items.begin(); it != items.end(); ++it)
		{
			SFolderItem *item = *it;
			if (item != NULL && (pass == 1 || (item->m_bMarked && !item->m_bParent)))
			{
				char path[0x134];
				strcpy(path, dir.c_str());
				strcat(path, item->GetName().c_str());
				if (!item->m_bFolder)
				{
					g_nOperationFiles++;
					g_fileList.push_back(SFileListItem(std::string(path), FALSE, item->m_dwSize));
					g_ullOperationTotal += item->m_dwSize;
					g_ullOperationBytes += item->m_dwSize;
				}
				else
				{
					// A drive in Root: "C:\".
					if (separator == '\\' && StrFind(dir, ":") == -1)
						strcat(path, ":\\");
					g_nOperationFolders++;
					memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
					wcscpy(g_szOperationFile, item->m_pszName);
					g_fileList.push_back(SFileListItem(std::string(path), TRUE, 0));
					pane->CollectFiles(path);
				}
			}
			if (g_bCancelOperation)
				break;
		}
		if (g_bCancelOperation)
			break;
	}

	// (A cancelled list is collected again next time.)
	g_bFolderListChanged = g_bCancelOperation;
	g_bFileOperationActive = FALSE;
	g_bFilesCollected = !g_bCancelOperation;
	pane->m_bCollected = TRUE;
	g_bCancelOperation = FALSE;
	return 0;
}

// 0x00127052 (this in eax)
HRESULT CFileManager::Render()
{
	if (!m_bInitialized)
		return E_FAIL;
	float scaleX = m_pFont->m_fScaleFactorX;
	float scaleY = m_pFont->m_fScaleFactorY;
	m_pFont->SetScaleFactors(g_pFileSettings->m_fFontScale, g_pFileSettings->m_fFontScale);
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	DrawBackground();

	// The FTP manager covers everything. (Quirk: the font's scale isn't
	// restored.)
	if (g_bFtpManagerOpen)
	{
		g_pFtpManager->Render();
		if (g_bFileViewerOpen)
			g_pFileViewer->Render();
		return S_OK;
	}

	IFileFolder *pane = ActivePane();
	BOOL frames = m_bDrawBorder || m_pBackground == NULL;
	float innerX = m_fX + s_fMarginX;
	float innerWidth = m_fWidth - s_fMarginX * 2.0f;

	// The header: the folder, its counts and size; "A"/"B" when one pane
	// shows.
	float y = s_fMarginY + m_fY;
	if (frames)
		DrawFrameBox(innerX, y, innerWidth, m_fLineHeight + 8.0f, 1);
	wchar_t text[360];
	swprintf(text, 360, L"%ls\t%u Files, %u Folders (%ls)", pane->GetTitle(), pane->m_nFileCount,
	         pane->m_nFolderCount, pane->m_szTotalSize);
	float textY = m_fLineHeight * 0.5f + y + 4.0f;
	m_pFont->DrawText(innerX + 2.0f, textY, g_colorScheme.OPTION_ACTIVETEXTCOLOR, text,
	                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - (s_fMarginX + 2.0f) * 2.0f);
	if (!m_bSideBySide)
		m_pFont->DrawText(((m_fX + m_fWidth) - s_fMarginX) - 2.0f, textY, g_colorScheme.OPTION_ACTIVETEXTCOLOR,
		                  m_nActivePane != 0 ? L"B" : L"A", XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);

	// The footer: the highlighted item's name, size and date.
	float footerHeight = m_fLineHeight + 8.0f;
	y = ((m_fHeight + m_fY) - s_fMarginY) - footerHeight;
	if (frames)
		DrawFrameBox(innerX, y, innerWidth, footerHeight, 1);
	SFolderItem *current = pane->m_pCurrent;
	if (current != NULL)
	{
		wchar_t details[60];
		memset(details, 0, sizeof(details));
		wcscpy(details, current->m_szSize);
		wcscat(details, L"  ");
		wcscat(details, current->m_szDate);
		float detailsWidth = m_pFont->GetTextWidth(details) + 4.0f;
		textY = m_fLineHeight * 0.5f + y + 4.0f;
		m_pFont->DrawText(innerX + 2.0f, textY, g_colorScheme.OPTION_ACTIVETEXTCOLOR, current->m_pszName,
		                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, (m_fWidth - (s_fMarginX + 2.0f) * 2.0f) - detailsWidth);
		m_pFont->DrawText((m_fX + m_fWidth) - (s_fMarginX + 2.0f), textY, g_colorScheme.OPTION_ACTIVETEXTCOLOR, details,
		                  XBFONT_RIGHT | XBFONT_CENTER_Y | XBFONT_TRUNCATED, detailsWidth);
	}

	// The panes (frame style 2 marks the active one), or the viewer.
	if (!g_bFileViewerOpen)
	{
		if (!m_bSideBySide)
		{
			CFileFolder *shown = m_nActivePane != 0 ? &m_panes[1] : &m_panes[0];
			if (frames)
				DrawFrameBox(shown->m_fX, shown->m_fY, shown->m_fWidth, shown->m_fHeight, 1);
			shown->Render();
		}
		else
		{
			if (frames)
			{
				DrawFrameBox(m_panes[0].m_fX, m_panes[0].m_fY, m_panes[0].m_fWidth, m_panes[0].m_fHeight,
				             m_nActivePane == 0 ? 2 : 1);
				DrawFrameBox(m_panes[1].m_fX, m_panes[1].m_fY, m_panes[1].m_fWidth, m_panes[1].m_fHeight,
				             m_nActivePane != 0 ? 2 : 1);
			}
			m_panes[0].Render();
			m_panes[1].Render();
		}
	}
	else
	{
		g_pFileViewer->Render();
	}
	if (g_bFileAudioPlayer)
	{
		g_pAudioGUI->Render();
		return S_OK;
	}
	if (m_bMessageShown && m_pMessageBox != NULL)
		m_pMessageBox->Render();

	BOOL busy = TRUE;
	switch (m_nState)
	{
	case FMSTATE_BROWSE:
		if (m_pSortMenu != NULL)
		{
			m_menu.Render();
			m_pSortMenu->Render();
			busy = FALSE;
		}
		else if (m_bMenuOpen)
		{
			m_menu.Render();
			busy = FALSE;
		}
		break;

	case FMSTATE_COPY:
	case FMSTATE_MOVE:
	case FMSTATE_DELETE:
	case FMSTATE_CALC_SIZE:
	case FMSTATE_COPY_DVD:
	case FMSTATE_ZIP:
	{
		if (m_nStep != FMSTEP_RUNNING)
		{
			busy = FALSE;
			break;
		}
		// The progress box below the message box (which moves up if
		// there's no room).
		float boxX = m_fX + 20.0f;
		float boxWidth = m_fWidth - 40.0f;
		float boxHeight = m_fLineHeight * 6.0f + 20.0f;
		float boxY = m_pMessageBox->m_fY + m_pMessageBox->m_fHeight + 10.0f;
		float screenBottom = m_fHeight + m_fY;
		if (boxHeight + boxY > screenBottom)
		{
			boxY = (screenBottom - boxHeight) - 10.0f;
			m_pMessageBox->m_fY = (boxY - m_pMessageBox->m_fHeight) - 10.0f;
		}
		unsigned filePercent = (unsigned)(int)g_dFilePercent;
		unsigned totalPercent = (unsigned)(int)g_dTotalPercent;
		if (totalPercent == 0)
		{
			if ((LONGLONG)g_ullOperationTotal > 0)
				totalPercent = (unsigned)(g_ullOperationDone * 100 / g_ullOperationTotal);
			if (totalPercent == 0 && g_nOperationFolders + g_nOperationFiles > 1)
				totalPercent = (g_nOperationProgress * 100) / (g_nOperationFolders + g_nOperationFiles);
		}
		if (totalPercent > 100)
			totalPercent = 100;
		if (filePercent > 100)
			filePercent = 100;
		boxY -= 50.0f;
		wchar_t from[310], to[310];
		if (m_nState == FMSTATE_DELETE || m_nState == FMSTATE_ZIP)
		{
			// One file line and the overall bar.
			DrawProgressBox(m_pFont, boxX, boxY, boxWidth, boxHeight,
			                m_nState == FMSTATE_DELETE ? L"Deleting" : L"Compressing", g_szOperationFile, NULL,
			                (float)totalPercent, -1.0f, FALSE);
		}
		else
		{
			swprintf(from, 310, L"From: %ls", g_szOperationFile);
			swprintf(to, 310, L"To: %ls", g_szOperationTarget);
			DrawProgressBox(m_pFont, boxX, boxY, boxWidth, boxHeight + m_fLineHeight,
			                m_nState == FMSTATE_MOVE ? L"Moving" : L"Copying", from, to, (float)filePercent,
			                (float)totalPercent, TRUE);
		}
		break;
	}

	case FMSTATE_RENAME:
	case FMSTATE_NEW_FOLDER:
	case FMSTATE_COPY_DISC:
	case FMSTATE_ZIP_NAME:
	case FMSTATE_XBE_TITLE:
		if (m_bKeyboardOpen)
			g_virtualKeyboard.Render();
		busy = FALSE;
		break;

	default:
		busy = FALSE;
		break;
	}
	// A cancel that hasn't been answered yet.
	if (busy && g_bCancelOperation && !g_bFileOperationPaused)
		DrawBusyBox(m_pFont, L"Please Wait");
	m_pFont->SetScaleFactors(scaleX, scaleY);
	return S_OK;
}
