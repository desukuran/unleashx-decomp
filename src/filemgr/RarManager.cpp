// CRarManager: the file manager's RAR browser. See Viewers.h.
//
// The zip browser's screen over unrarlib: A lists the menu (extract the
// marked files, or all, next to the archive or into a chosen folder), X
// marks, B/BACK closes. Extracting everything is
// one urarlib_get call, with this object's UnrarSink counting the files.
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
#include "../system/FileOps.h"
#include "../system/SystemInfo.h"
#include "../ui/FileBrowser.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Viewers.h"

// The header's height, worked out on the first Render(). 0x0027A3DC
static float s_fRarHeaderHeight;
static BOOL s_bRarHeaderHeight = FALSE;   // 0x0027A3E0 bit 0

// "1.23 MB", "45.67 KB", else "123" plus unit. (inlined)
static void FormatEntrySize(wchar_t *out, DWORD size, const wchar_t *bytesUnit)
{
	double value = (double)size;
	if (value * (1.0 / 1048576.0) >= 10.0)
	{
		FormatDoubleW(out, 2, TRUE, value * (1.0 / 1048576.0));
		wcscat(out, L" MB");
	}
	else if (value * (1.0 / 1024.0) >= 10.0)
	{
		FormatDoubleW(out, 2, TRUE, value * (1.0 / 1024.0));
		wcscat(out, L" KB");
	}
	else
	{
		FormatIntW(out, (int)size);
		wcscat(out, bytesUnit);
	}
}

// 0x00014579 (this in esi)
SRarEntry::SRarEntry(const char *entryName, const char *entryPath, DWORD unpackedSize, DWORD packedSize)
{
	marked = FALSE;
	wchar_t text[0x104];
	memset(text, 0, sizeof(text));
	MultiByteToWideChar(CP_ACP, 0, entryName, (int)strlen(entryName), text, 0x104);
	// Quirk: the path goes through a _strdup copy that is never freed.
	path = _strdup(entryPath);
	name = text;
	// "packed/unpacked" (the unpacked size without " B").
	memset(text, 0, sizeof(text));
	FormatEntrySize(text, packedSize, L" B");
	size = text;
	if (unpackedSize == 0)
		return;
	memset(text, 0, sizeof(text));
	FormatEntrySize(text, unpackedSize, L"");
	size.append(L"/");
	size.append(text);
}

// The list's order: names without case. (0x0001AACB)
static bool RarEntryLess(const SRarEntry &a, const SRarEntry &b)
{
	return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
}

// 0x00017783 (this in esi)
CRarManager::CRarManager()
{
	m_bCancel = FALSE;
	m_bRunning = FALSE;
	m_bDone = FALSE;
	m_bFailed = FALSE;
	m_nOperation = OP_NONE;
	m_bMessageShown = FALSE;
	m_unknown040 = 0;
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
	m_strBaseFolder = "";
	m_bInitialized = FALSE;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00017854 (this in ebx)
CRarManager::~CRarManager()
{
	delete m_pBrowser;
	m_pBrowser = NULL;
	delete m_pMenu;
	m_pMenu = NULL;
	delete m_pMessageBox;
	m_pMessageBox = NULL;
	m_entries.clear();
}

// 0x0001793F
BOOL CRarManager::Init(CXBFontEx *font)
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

// 0x000178DD (this in eax)
BOOL CRarManager::Open(const char *path)
{
	while (!m_bListed)
		Sleep(5);
	m_bListed = FALSE;
	m_bOpen = TRUE;
	m_strRarFile = path;
	m_nSelected = 0;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, ListThread, this, 0, NULL);
	if (thread != NULL && thread != INVALID_HANDLE_VALUE)
	{
		Sleep(5);
		CloseHandle(thread);
	}
	return TRUE;
}

// 0x0001865A (this in edi)
void CRarManager::ShowMessage(const char *text, const wchar_t *title, int buttons)
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

// 0x000185AC (this in edi)
void CRarManager::ShowBrowser(BOOL selectFiles)
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
	m_pBrowser->SetStartPath(GetPathDirectory(m_strRarFile.c_str()).c_str());
	m_pBrowser->Show(FALSE);
	m_bBrowserOpen = TRUE;
}

// 0x0001877A
unsigned __stdcall CRarManager::ListThread(void *param)
{
	CRarManager *rar = (CRarManager *)param;
	if (rar == NULL)
		return 0;
	std::string message;
	rar->m_entries.clear();
	ArchiveList_struct *list = NULL;
	int count = urarlib_list(rar->m_strRarFile.c_str(), &list, NULL);
	if (list == NULL || count == 0)
	{
		urarlib_freelist(list);
		StrFormat(message, "Failed to open '%s'. File may be corrupt or damaged.", rar->m_strRarFile.c_str());
		rar->ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
		rar->m_bListed = TRUE;
		return 1;
	}
	// When the archive starts with a folder, that folder is the base:
	// extracting drops it from the paths (see ExtractThread). Folders
	// aren't listed.
	std::string name;
	rar->m_strBaseFolder.clear();
	ArchiveList_struct *item = list;
	if (item->attributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		rar->m_strBaseFolder = "\\";
		rar->m_strBaseFolder.append(item->name);
		StrAddBackslash(rar->m_strBaseFolder);
	}
	for (; item != NULL; item = item->next)
	{
		if (item->attributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		// (RAR paths use '\\', so the name normally keeps its folders.)
		name = item->name;
		int slash = StrReverseFind(name, "/");
		if (slash != -1)
			name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
		rar->m_entries.push_back(SRarEntry(name.c_str(), item->name, item->unpackedSize, item->packedSize));
	}
	// Quirk: this frees the list from where the walk ended, NULL, so the
	// list leaks.
	urarlib_freelist(item);
	std::sort(rar->m_entries.begin(), rar->m_entries.end(), RarEntryLess);
	rar->m_bListed = TRUE;
	return 0;
}

// 0x00018975 (thunk: this - 4)
void CRarManager::OnFile(const char *name, bool ok, bool *proceed)
{
	if (!ok)
		m_bFailed = TRUE;
	else
		m_nExtracted++;
	memset(m_szCurrentFile, 0, sizeof(m_szCurrentFile));
	if (name != NULL)
		MultiByteToWideChar(CP_ACP, 0, name, (int)strlen(name), m_szCurrentFile, 0x104);
	if (!m_bCancel)
	{
		if (!g_bCancelOperation)
			return;
		m_bCancel = TRUE;
	}
	*proceed = false;
}

// 0x000189F7
unsigned __stdcall CRarManager::ExtractThread(void *param)
{
	CRarManager *rar = (CRarManager *)param;
	unsigned result = 0;
	if (rar == NULL)
		return result;
	rar->m_bDone = FALSE;
	rar->m_bFailed = FALSE;
	std::string dir = rar->m_strTarget;
	std::string message;
	BOOL all = FALSE;
	if (rar->m_nOperation == OP_EXTRACT_ALL)
		all = TRUE;
	else if (rar->m_nOperation != OP_EXTRACT_MARKED)
		goto done;

	StrAddBackslash(dir);
	// Quirk: this drops the base folder from the target rather than from
	// the entries.
	StrReplaceAll(dir, rar->m_strBaseFolder.c_str(), "\\");
	if (!CreateDirectoryTree(dir.c_str()))
	{
		rar->m_bFailed = TRUE;
		result = 2;
		goto done;
	}
	rar->m_nExtracted = 0;
	memset(rar->m_szCurrentFile, 0, sizeof(rar->m_szCurrentFile));

	if (all)
	{
		// Everything in one go; OnFile counts and cancels.
		if (!urarlib_get(rar->m_strRarFile.c_str(), dir.c_str(), NULL, NULL, rar))
			rar->m_bFailed = TRUE;
		if (rar->m_bFailed)
		{
			SetLastError(GetLastError());
			result = 2;
		}
		goto done;
	}

	for (std::vector<SRarEntry>::iterator it = rar->m_entries.begin();
	     it != rar->m_entries.end() && !rar->m_bCancel; ++it)
	{
		if (g_bCancelOperation)
		{
			rar->m_bCancel = TRUE;
			break;
		}
		if (!it->marked)
			continue;
		// The entry's path under dir (':' made '_', the base folder
		// dropped).
		std::string path = it->path;
		StrReplaceAll(path, ":", "_");
		if (rar->m_strBaseFolder.size() != 0)
			StrReplaceAll(path, rar->m_strBaseFolder.c_str(), "");
		std::string name = path;
		StrFormat(path, "%s%s", dir.c_str(), name.c_str());
		StrReplaceAll(path, "\\\\", "\\");
		DeleteFileA(path.c_str());
		std::string folder = path;
		int slash = StrReverseFind(folder, "\\");
		if (slash > 3 && slash > (int)dir.size())
		{
			folder = StrLeft(folder, slash);
			CreateDirectoryTree(folder.c_str());
		}
		memset(rar->m_szCurrentFile, 0, sizeof(rar->m_szCurrentFile));
		MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, rar->m_szCurrentFile, 0x104);
		// urarlib_get takes the folder; the entry lands there under its
		// own name, folders included.
		path = GetPathDirectory(path.c_str());
		if (!urarlib_get(rar->m_strRarFile.c_str(), path.c_str(), it->path.c_str(), NULL, NULL))
		{
			rar->m_bFailed = TRUE;
			SetLastError(GetLastError());
			result = 2;
			break;
		}
		// Quirk: on the folder, not the file.
		SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
		rar->m_nExtracted++;
	}

	if (rar->m_nExtracted == 0 && !rar->m_bFailed)
	{
		// Nothing marked: the highlighted entry, under dir.
		SRarEntry current = rar->m_entries[rar->m_nSelected];
		std::string path = dir;
		path.append(current.path);
		StrReplaceAll(path, "\\\\", "\\");
		BOOL ok = TRUE;
		int slash;
		if (!DeleteFileA(path.c_str()) && (slash = StrReverseFind(path, "\\")) > 3)
		{
			// Quirk: creates a folder named after the file part, relative
			// to the current directory.
			if (!CreateDirectoryTree(StrMid(path, slash + 1, (int)path.size() - (slash + 1)).c_str()))
			{
				rar->m_bFailed = TRUE;
				result = 2;
				SetLastError(GetLastError());
				ok = FALSE;
			}
		}
		if (ok)
		{
			memset(rar->m_szCurrentFile, 0, sizeof(rar->m_szCurrentFile));
			MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, rar->m_szCurrentFile, 0x104);
			if (!urarlib_get(rar->m_strRarFile.c_str(), dir.c_str(), current.path.c_str(), NULL, NULL))
			{
				rar->m_bFailed = TRUE;
				result = 2;
				SetLastError(GetLastError());
			}
			else
			{
				SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
				rar->m_nExtracted++;
			}
		}
	}

done:
	rar->m_bDone = TRUE;
	rar->m_bCancel = FALSE;
	return result;
}

// (inlined in ProcessInput)
void CRarManager::ScrollToSelection()
{
	int selected = m_nSelected;
	if (selected < m_nTop)
	{
		m_nBottom = m_nVisible - 1 + selected;
		m_nTop = selected;
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

// 0x00017A01
HRESULT CRarManager::ProcessInput(int *action, SGamepadState **pad)
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
		if (choice == 1 || choice == 4)
			ShowBrowser(FALSE);
		m_bRunning = FALSE;
		m_nOperation = (choice == 0 || choice == 1) ? OP_EXTRACT_MARKED : OP_EXTRACT_ALL;
		int dot = StrReverseFind(m_strRarFile, ".");
		if (dot == -1)
			m_strTarget = GetPathDirectory(m_strRarFile.c_str());
		else
			m_strTarget = StrLeft(m_strRarFile, dot);
		return S_OK;
	}

	if (m_nOperation != OP_NONE)
	{
		if (!m_bRunning)
		{
			memset(m_szCurrentFile, 0, sizeof(m_szCurrentFile));
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
			StrFormat(message, "Successfully finished Requested operation.\n%d file(s) unRarred successfully.",
			          m_nExtracted);
			ShowMessage(message.c_str(), L"Success", MSGBOX_BUTTONS_OK);
		}
		else
		{
			StrFormat(message,
			          "An error occured while performing requested operation.\nError Code: %d.\n%d file(s) unRarred "
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
				m_pMenu->Create(m_pFont, L"Rar Explorer", 0.0f, 0.0f, 0.0f, 0.0f, 10);
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
		// (m_bRunning is never set here: input is ignored while an
		// extract runs, so this can't cancel.)
		*action = INPUT_NONE;
		if (!m_bRunning)
			m_bOpen = FALSE;
		else
			m_bCancel = TRUE;
		break;

	case INPUT_X:
	{
		SRarEntry &entry = m_entries[m_nSelected];
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

// 0x00017F4C
BOOL CRarManager::Render()
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
	if (!s_bRarHeaderHeight)
	{
		s_fRarHeaderHeight = m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + 12.0f;
		s_bRarHeaderHeight = TRUE;
	}
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 1);
	DrawRect(m_fX + 10.0f, m_fY + 5.0f, m_fWidth - 20.0f, s_fRarHeaderHeight - 10.0f, g_colorScheme.DIALOG_BORDERCOLOR,
	         g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	DrawRect(m_fX + 10.0f, s_fRarHeaderHeight + m_fY, m_fWidth - 20.0f, (m_fHeight - s_fRarHeaderHeight) - 10.0f,
	         g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	wchar_t title[0x104];
	MultiByteToWideChar(CP_ACP, 0, m_strRarFile.c_str(), (int)m_strRarFile.size(), title, 0x104);
	m_pCaptionFont->DrawText(m_fX + 12.0f,
	                         (m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + 10.0f) * 0.5f + m_fY,
	                         g_colorScheme.CAPTION_TEXTCOLOR, title, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                         (m_fWidth - 20.0f) - 4.0f);

	if (!m_bListed)
	{
		DrawBusyBox(m_pFont, L"Opening Rar File");
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
	for (std::vector<SRarEntry>::iterator it = m_entries.begin() + index; it != m_entries.end(); ++it)
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
	if (m_bDone)
		return S_OK;
	if (m_bCancel)
	{
		DrawBusyBox(m_pFont, L"Please Wait");
		return S_OK;
	}
	if (!m_bRunning || index < 1)
		return S_OK;
	// The progress, by files extracted.
	wchar_t file[310];
	swprintf(file, 310, L"%ls", m_szCurrentFile);
	float percent = (float)m_nExtracted * 100.0f / (float)m_entries.size();
	DrawProgressBox(m_pFont, x, m_fHeight * 0.5f, width, 0.0f, L"Extracting", file, NULL, percent, -1.0f, FALSE);
	return S_OK;
}
