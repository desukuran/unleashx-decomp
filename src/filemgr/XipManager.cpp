// CXipManager: the file manager's .xip browser. See Viewers.h.
//
// The zip browser's code with a CXipArchive: A lists the menu (extract the
// marked files, or all, next to the archive or into a chosen folder), X
// marks, B/BACK closes. The archive stays open from the listing on.
#include <algorithm>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../ui/FileBrowser.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Viewers.h"

// The header's height, worked out on the first Render(). 0x0027A3E4
static float s_fXipHeaderHeight;
static BOOL s_bXipHeaderHeight = FALSE;   // 0x0027A3E8 bit 0

// The list's order: names without case. (0x0001AAAE)
static bool XipEntryLess(const SZipEntry &a, const SZipEntry &b)
{
	return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
}

// 0x00016274 (this in esi)
CXipManager::CXipManager()
{
	memset(&m_archive, 0, sizeof(m_archive));
	m_bRunning = FALSE;
	m_bDone = FALSE;
	m_bFailed = FALSE;
	m_nOperation = OP_NONE;
	m_bMessageShown = FALSE;
	m_unknown03C = 0;
	m_bMenuOpen = FALSE;
	m_bOpen = TRUE;
	m_pBrowser = NULL;
	m_pMenu = NULL;
	m_pMessageBox = NULL;
	m_bBrowserOpen = FALSE;
	m_pFont = NULL;
	m_pCaptionFont = NULL;
	m_bListed = TRUE;
	m_pScrollSound = NULL;
	m_nTop = 0;
	m_nBottom = 0;
	m_nVisible = 0;
	m_fLineHeight = 0.0f;
	m_bInitialized = FALSE;
	// (m_nSelected, m_nExtracted, m_szCurrentFile and the rectangle are
	// left as they are.)
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00016325 (this in ebx)
CXipManager::~CXipManager()
{
	if (m_archive.m_pFile != NULL)
		m_archive.Close();
	delete m_pBrowser;
	m_pBrowser = NULL;
	delete m_pMenu;
	m_pMenu = NULL;
	delete m_pMessageBox;
	m_pMessageBox = NULL;
	m_entries.clear();
}

// 0x0001640F
BOOL CXipManager::Init(CXBFontEx *font)
{
	if (m_bInitialized)
		return S_OK;
	if (font == NULL)
		return E_FAIL;
	m_pFont = font;
	m_fLineHeight = font->m_fScaleFactorY * font->m_fFontHeight;
	m_pCaptionFont = m_pSettings->m_pDialogFont;
	if (m_pCaptionFont == NULL)
		m_pCaptionFont = font;
	m_fY = 5.0f;
	m_fX = 10.0f;
	m_fWidth = 620.0f;
	m_fHeight = 470.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
	m_pScrollSound = g_soundEffects.Find("Scroll");
	m_bInitialized = TRUE;
	return S_OK;
}

// 0x000163AD (this in eax)
BOOL CXipManager::Open(const char *path)
{
	while (!m_bListed)
		Sleep(5);
	m_bListed = FALSE;
	m_bOpen = TRUE;
	m_strXipFile = path;
	m_nSelected = 0;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, ListThread, this, 0, NULL);
	if (thread != NULL && thread != INVALID_HANDLE_VALUE)
	{
		Sleep(5);
		CloseHandle(thread);
	}
	return TRUE;
}

// 0x00017030 (this in edi)
void CXipManager::ShowMessage(const char *text, const wchar_t *title, int buttons)
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

// 0x000155AC (this in edi)
void CXipManager::ShowBrowser(BOOL selectFiles)
{
	if (m_pBrowser == NULL)
	{
		m_pBrowser = new CFileBrowser;
		if (m_pBrowser != NULL)
			m_pBrowser->Create(m_pFont, NULL);
	}
	if (m_pBrowser == NULL)
	{
		m_bBrowserOpen = FALSE;
		return;
	}
	m_pBrowser->SetSelectFiles(selectFiles);
	m_pBrowser->m_bNewFolderItem = !selectFiles;
	m_pBrowser->SetStartPath(GetPathDirectory(m_strXipFile.c_str()).c_str());
	m_pBrowser->Show(FALSE);
	m_bBrowserOpen = TRUE;
}

// 0x00017150
unsigned __stdcall CXipManager::ListThread(void *param)
{
	CXipManager *xip = (CXipManager *)param;
	if (xip == NULL)
		return 0;
	std::string message;
	xip->m_entries.clear();
	// (Open closes the archive first.)
	if (FAILED(xip->m_archive.Open(xip->m_strXipFile.c_str())))
	{
		StrFormat(message, "Failed to open '%s'. File may be corrupt or damaged.", xip->m_strXipFile.c_str());
		xip->ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
		xip->m_bListed = TRUE;
		return 1;
	}
	int count = xip->m_archive.m_wFileCount;
	std::string name;
	for (int i = 0; i < count; i++)
	{
		SXipItem item;
		memset(&item, 0, sizeof(item));
		if (FAILED(xip->m_archive.GetItem(i, &item)))
		{
			StrFormat(message, "Error occured while opening '%s'. File may be corrupt or damaged.",
			          xip->m_strXipFile.c_str());
			xip->ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
			xip->m_bListed = TRUE;
			return 2;
		}
		// The name without its folders. (Folders are listed too.)
		name = item.name;
		int slash = StrReverseFind(name, "/");
		if (slash != -1)
			name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
		xip->m_entries.push_back(SZipEntry(name.c_str(), 0, item.entry.size, i));
	}
	std::sort(xip->m_entries.begin(), xip->m_entries.end(), XipEntryLess);
	xip->m_bListed = TRUE;
	return 0;
}

// 0x00017397
unsigned __stdcall CXipManager::ExtractThread(void *param)
{
	CXipManager *xip = (CXipManager *)param;
	unsigned result = 0;
	if (xip == NULL)
		return result;
	xip->m_bDone = FALSE;
	xip->m_bFailed = FALSE;
	std::string dir = xip->m_strTarget;
	std::string message;
	if (xip->m_archive.m_wFileCount == 0)
	{
		StrFormat(message, "Failed to open '%s'. File may be corrupt or damaged.", xip->m_strXipFile.c_str());
		xip->ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
		xip->m_bDone = TRUE;
		xip->m_bFailed = FALSE;
		return 1;
	}
	if (xip->m_nOperation == OP_EXTRACT_MARKED || xip->m_nOperation == OP_EXTRACT_ALL)
	{
		BOOL all = xip->m_nOperation == OP_EXTRACT_ALL;
		if (StrRight(dir, 1).compare("\\") != 0)
			dir.append("\\");
		if (!CreateDirectoryTree(dir.c_str()))
		{
			xip->m_bFailed = TRUE;
			result = 2;
		}
		else
		{
			// Each file straight under dir by its full name. Quirk: the
			// archive's subfolders aren't created, so their files fail.
			xip->m_nExtracted = 0;
			SXipItem item;
			for (std::vector<SZipEntry>::iterator it = xip->m_entries.begin(); it != xip->m_entries.end(); ++it)
			{
				if (!all && !it->marked)
					continue;
				memset(&item, 0, sizeof(item));
				if (xip->m_archive.GetItem(it->index, &item) != S_OK || item.entry.type == XIP_TYPE_DIRECTORY)
					continue;
				std::string path = dir;
				path.append(item.name);
				StrReplaceAll(path, "\\\\", "\\");
				DeleteFileA(path.c_str());
				memset(xip->m_szCurrentFile, 0, sizeof(xip->m_szCurrentFile));
				MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, xip->m_szCurrentFile, 0x104);
				if (xip->m_archive.Extract(it->index, path.c_str()) != S_OK)
				{
					xip->m_bFailed = TRUE;
					result = 2;
					break;
				}
				SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
				xip->m_nExtracted++;
			}
			if (xip->m_nExtracted == 0 && !xip->m_bFailed)
			{
				// Nothing marked: the highlighted entry (even a folder).
				SZipEntry current = xip->m_entries[xip->m_nSelected];
				memset(&item, 0, sizeof(item));
				if (xip->m_archive.GetItem(current.index, &item) == S_OK)
				{
					std::string path = dir;
					path.append(item.name);
					StrReplaceAll(path, "\\\\", "\\");
					DeleteFileA(path.c_str());
					memset(xip->m_szCurrentFile, 0, sizeof(xip->m_szCurrentFile));
					MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, xip->m_szCurrentFile, 0x104);
					if (xip->m_archive.Extract(current.index, path.c_str()) == S_OK)
					{
						SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
						xip->m_nExtracted++;
					}
					else
					{
						xip->m_bFailed = TRUE;
						result = 2;
					}
				}
			}
		}
	}
	xip->m_bDone = TRUE;
	return result;
}

// (inlined in ProcessInput)
void CXipManager::ScrollToSelection()
{
	int selected = m_nSelected;
	if (selected < m_nTop)
	{
		m_nTop = selected;
		m_nBottom = m_nVisible - 1 + selected;
		int last = (int)m_entries.size() - 1;
		if (last < m_nBottom)
		{
			m_nBottom = last;
			m_nTop = (last - m_nVisible) + 1;
		}
	}
	if (m_nBottom < selected)
	{
		m_nBottom = selected;
		m_nTop = (selected - m_nVisible) + 1;
		if (m_nTop < 0)
		{
			m_nTop = 0;
			m_nBottom = m_nVisible - 1;
		}
	}
}

// 0x000164D1
HRESULT CXipManager::ProcessInput(int *action, SGamepadState **pad)
{
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
		return S_OK;
	}
	if (m_bBrowserOpen)
	{
		m_pBrowser->ProcessInput(action, pad);
		if (m_pBrowser->IsOpen())
			return S_OK;
		m_bBrowserOpen = FALSE;
		if (m_pBrowser->m_bCancelled)
		{
			m_nOperation = OP_NONE;
			return S_OK;
		}
		if (m_nOperation == OP_ADD)
			m_strAddPath = m_pBrowser->GetSelectedPath();
		else
			m_strTarget = m_pBrowser->GetSelectedPath();
		return S_OK;
	}
	if (m_bMenuOpen)
	{
		m_pMenu->ProcessInput(action, pad);
		m_bMenuOpen = m_pMenu->IsOpen();
		if (m_bMenuOpen || !m_pMenu->m_bChosen)
			return S_OK;
		int choice = m_pMenu->GetSelection();
		if (choice != 0 && choice != 1 && choice != 3 && choice != 4)
			return S_OK;
		// 1 and 4 ask for the folder (the browser opens over the
		// default); 0 and 3 extract next to the archive, into a folder
		// named after it.
		if (choice == 1 || choice == 4)
			ShowBrowser(FALSE);
		m_bRunning = FALSE;
		m_nOperation = (choice == 0 || choice == 1) ? OP_EXTRACT_MARKED : OP_EXTRACT_ALL;
		int dot = StrReverseFind(m_strXipFile, ".");
		if (dot == -1)
			m_strTarget = GetPathDirectory(m_strXipFile.c_str());
		else
			m_strTarget = StrLeft(m_strXipFile, dot);
		return S_OK;
	}

	if (m_nOperation != OP_NONE)
	{
		if (!m_bRunning)
		{
			HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, ExtractThread, this, 0, NULL);
			if (thread == NULL || thread == INVALID_HANDLE_VALUE)
				return S_OK;
			SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
			CloseHandle(thread);
			m_bRunning = TRUE;
			return S_OK;
		}
		if (!m_bDone)
			return S_OK;
		m_bRunning = FALSE;
		std::string message;
		m_nOperation = OP_NONE;
		if (!m_bFailed)
		{
			StrFormat(message, "Successfully finished Requested operation.\n%d file(s) extracted successfully.",
			          m_nExtracted);
			ShowMessage(message.c_str(), L"Success", MSGBOX_BUTTONS_OK);
		}
		else
		{
			StrFormat(message,
			          "An error occured while performing requested operation.\nError Code: %d.\n%d file(s) extracted "
			          "successfully.",
			          (int)GetLastError(), m_nExtracted);
			ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
		}
		return S_OK;
	}

	if (*action == INPUT_LTHUMB && pad != NULL && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}
	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
	case INPUT_WHITE:
		if (m_pMenu == NULL)
		{
			m_pMenu = new COptionMenu;
			if (m_pMenu != NULL)
			{
				// (The zip browser's title.)
				m_pMenu->Create(m_pFont, L"Zip Explorer", 0.0f, 0.0f, 0.0f, 0.0f, 10);
				m_pMenu->AddOption(L"Extract selected File(s)", 0);
				m_pMenu->AddOption(L"Extract selected File(s) to...", 0);
				m_pMenu->AddOption(L"", 0);
				m_pMenu->AddOption(L"Extract all Files", 0);
				m_pMenu->AddOption(L"Extract all Files to...", 0);
			}
		}
		if (m_pMenu != NULL)
		{
			m_bMenuOpen = TRUE;
			m_pMenu->Show(0);
		}
		break;

	case INPUT_B:
	case INPUT_BACK:
		// (No cancel, and the action isn't consumed.)
		m_bOpen = FALSE;
		break;

	case INPUT_X:
	{
		SZipEntry &entry = m_entries[m_nSelected];
		entry.marked = !entry.marked;
		if ((unsigned)m_nSelected >= m_entries.size() - 1)
			return S_OK;
		m_nSelected++;
		ScrollToSelection();
		break;
	}

	case INPUT_LTRIGGER:
	case INPUT_UP:
		if (m_entries.size() == 0 || m_nSelected == 0)
			return S_OK;
		m_nSelected--;
		PlaySound(m_pScrollSound);
		ScrollToSelection();
		break;

	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		if (m_entries.size() == 0 || (unsigned)m_nSelected >= m_entries.size() - 1)
			return S_OK;
		m_nSelected++;
		PlaySound(m_pScrollSound);
		ScrollToSelection();
		break;
	}
	return S_OK;
}

// 0x000169F0
BOOL CXipManager::Render()
{
	if (!m_bInitialized)
		return S_OK;
	if (m_bListed && !m_bMessageShown && m_entries.size() == 0)
	{
		m_bOpen = FALSE;
		return E_FAIL;
	}
	DimScreen(0.3f);
	m_pCaptionFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	if (!s_bXipHeaderHeight)
	{
		s_fXipHeaderHeight = m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + 12.0f;
		s_bXipHeaderHeight = TRUE;
	}
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 1);
	DrawRect(m_fX + 10.0f, m_fY + 5.0f, m_fWidth - 20.0f, s_fXipHeaderHeight - 10.0f, g_colorScheme.DIALOG_BORDERCOLOR,
	         g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	DrawRect(m_fX + 10.0f, s_fXipHeaderHeight + m_fY, m_fWidth - 20.0f, (m_fHeight - s_fXipHeaderHeight) - 10.0f,
	         g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	wchar_t title[0x104];
	MultiByteToWideChar(CP_ACP, 0, m_strXipFile.c_str(), (int)m_strXipFile.size(), title, 0x104);
	m_pCaptionFont->DrawText(m_fX + 12.0f,
	                         (m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + 10.0f) * 0.5f + m_fY,
	                         g_colorScheme.CAPTION_TEXTCOLOR, title, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                         (m_fWidth - 20.0f) - 4.0f);

	if (!m_bListed)
	{
		DrawBusyBox(m_pFont, L"Opening Xip File");
		return S_OK;
	}
	if (m_nVisible == 0)
	{
		m_nVisible = (int)((m_fHeight - (m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight * 2.0f + 20.0f)) /
		                   (m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight + 3.0f));
		m_nTop = 0;
		m_nBottom = m_nVisible - 1;
	}

	int index = m_nTop;
	float width = m_fWidth - 26.0f;
	float x = m_fX + 13.0f;
	float y = m_fLineHeight + m_fY + 23.0f;
	float bottom = m_fHeight + m_fY;
	float sizeWidth = width * 0.25f;
	for (std::vector<SZipEntry>::iterator it = m_entries.begin() + index; it != m_entries.end(); ++it)
	{
		if (index >= m_nTop)
		{
			DWORD color = g_colorScheme.DIALOG_TEXTCOLOR;
			if (index == m_nSelected)
			{
				DrawGradientRect(x - 1.0f, y - 1.0f, width, m_fLineHeight + 3.0f, g_colorScheme.OPTION_ACTIVEBACKCOLOR,
				                 g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE, g_colorScheme.OPTION_ACTIVEBACKCOLOR);
				color = g_colorScheme.OPTION_ACTIVETEXTCOLOR;
			}
			else if (it->marked)
			{
				color = g_colorScheme.OPTION_ACTIVETEXTCOLOR;
			}
			m_pFont->DrawText(x, y, color, it->name.c_str(), XBFONT_TRUNCATED, width - sizeWidth);
			m_pFont->DrawText((width + x) - 2.0f, y, color, it->size.c_str(), XBFONT_RIGHT | XBFONT_TRUNCATED,
			                  sizeWidth - 2.0f);
			y = y + m_fLineHeight + 3.0f;
			if (bottom < y)
			{
				m_nBottom = index;
				break;
			}
		}
		if (m_nBottom < index)
			break;
		index++;
	}

	if (m_bMenuOpen)
		m_pMenu->Render();
	if (m_bMessageShown)
	{
		m_pMessageBox->Render();
		return S_OK;
	}
	if (m_bBrowserOpen)
	{
		m_pBrowser->Render();
		return S_OK;
	}
	if (m_bDone || !m_bRunning)
		return S_OK;
	// The progress, by files extracted. Unlike the zip browser's, the box
	// sits half the window's height below the last line drawn.
	wchar_t file[310];
	swprintf(file, 310, L"%ls", m_szCurrentFile);
	float percent = (float)m_nExtracted * 100.0f / (float)m_entries.size();
	DrawProgressBox(m_pFont, x, m_fHeight * 0.5f + y, width, 0.0f, L"Extracting", file, NULL, percent, -1.0f, FALSE);
	return S_OK;
}
