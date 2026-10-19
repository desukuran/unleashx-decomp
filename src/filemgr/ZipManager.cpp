// CZipManager: the file manager's zip browser. See Viewers.h.
//
// A lists the menu (extract the marked files, or all, next to the zip or
// into a chosen folder), X marks, B/BACK closes.
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
#include "../thirdparty/XZip.h"
#include "../ui/FileBrowser.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Viewers.h"

// The header's height, worked out on the first Render(). 0x0027A3EC
static float s_fZipHeaderHeight;
static BOOL s_bZipHeaderHeight = FALSE;   // 0x0027A3F0 bit 0

// "1.23 MB", "45.67 KB", else "123" plus unit ("" or " B"). (inlined)
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

// 0x00014365 (this in esi)
SZipEntry::SZipEntry(const char *entryName, DWORD originalSize, DWORD packedSize, int entryIndex)
{
	index = entryIndex;
	marked = FALSE;
	wchar_t text[0x104];
	memset(text, 0, sizeof(text));
	MultiByteToWideChar(CP_ACP, 0, entryName, (int)strlen(entryName), text, 0x104);
	name = text;
	// "packed/original" (the original without " B").
	memset(text, 0, sizeof(text));
	FormatEntrySize(text, packedSize, L" B");
	size = text;
	if (originalSize == 0)
		return;
	memset(text, 0, sizeof(text));
	FormatEntrySize(text, originalSize, L"");
	size.append(L"/");
	size.append(text);
}

// The list's order: names without case. (0x000BDED1)
static bool ZipEntryLess(const SZipEntry &a, const SZipEntry &b)
{
	return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
}

// 0x000147A3 (this in esi)
CZipManager::CZipManager()
{
	m_nVisible = 0;
	m_nBottom = 0;
	m_nTop = 0;
	m_nOperation = OP_NONE;
	m_unknown03C = 0;
	m_bMessageShown = FALSE;
	m_bMenuOpen = FALSE;
	m_bOpen = TRUE;
	m_pBrowser = NULL;
	m_pMenu = NULL;
	m_pMessageBox = NULL;
	m_bBrowserOpen = FALSE;
	m_bInitialized = FALSE;
	m_fLineHeight = 0.0f;
	m_bListed = TRUE;
	m_pScrollSound = NULL;
	m_pFont = NULL;
	m_pCaptionFont = NULL;
	m_bDone = FALSE;
	m_bRunning = FALSE;
	m_bFailed = FALSE;
	m_bCancel = FALSE;
	m_nExtracted = 0;
	m_nSelected = 0;
	memset(m_szCurrentFile, 0, sizeof(m_szCurrentFile));
	m_fY = m_fX = m_fWidth = m_fHeight = 0.0f;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x0001484D (this in ebx)
CZipManager::~CZipManager()
{
	delete m_pBrowser;
	m_pBrowser = NULL;
	delete m_pMenu;
	m_pMenu = NULL;
	delete m_pMessageBox;
	m_pMessageBox = NULL;
	m_entries.clear();
}

// 0x00014924
BOOL CZipManager::Init(CXBFontEx *font)
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

// 0x000148C2 (this in eax)
BOOL CZipManager::Open(const char *path)
{
	// One listing at a time.
	while (!m_bListed)
		Sleep(5);
	m_bListed = FALSE;
	m_bOpen = TRUE;
	m_strZipFile = path;
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
void CZipManager::ShowMessage(const char *text, const wchar_t *title, int buttons)
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
void CZipManager::ShowBrowser(BOOL selectFiles)
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
	m_pBrowser->SetStartPath(GetPathDirectory(m_strZipFile.c_str()).c_str());
	m_pBrowser->Show(FALSE);
	m_bBrowserOpen = TRUE;
}

// 0x0001565A
unsigned __stdcall CZipManager::ListThread(void *param)
{
	CZipManager *zip = (CZipManager *)param;
	if (zip == NULL)
		return 0;
	std::string message;
	zip->m_entries.clear();
	HZIP handle = OpenZip((void *)zip->m_strZipFile.c_str(), 0, ZIP_FILENAME);
	if (handle == NULL)
	{
		StrFormat(message, "Failed to open '%s'. File may be corrupt or damaged.", zip->m_strZipFile.c_str());
		zip->ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
		zip->m_bListed = TRUE;
		return 1;
	}
	ZIPENTRY entry;
	if (GetZipItem(handle, -1, &entry) == ZR_OK)
	{
		int count = entry.index;
		std::string name;
		for (int i = 0; i < count; i++)
		{
			if (GetZipItem(handle, i, &entry) != ZR_OK)
			{
				// (The zip stays open.)
				StrFormat(message, "Error occured while opening '%s'. File may be corrupt or damaged.",
				          zip->m_strZipFile.c_str());
				zip->ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
				zip->m_bListed = TRUE;
				return 2;
			}
			// The name without its folders; a folder entry keeps its path.
			name = entry.name;
			int slash = StrReverseFind(name, "/");
			if (slash != -1)
			{
				name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
				if (name.size() == 0)
					name = entry.name;
			}
			if (name.size() != 0)
				zip->m_entries.push_back(SZipEntry(name.c_str(), (DWORD)entry.unc_size, (DWORD)entry.comp_size, entry.index));
		}
	}
	CloseAnyZip(handle);
	std::sort(zip->m_entries.begin(), zip->m_entries.end(), ZipEntryLess);
	zip->m_bListed = TRUE;
	return 0;
}

// 0x000158E3
unsigned __stdcall CZipManager::ExtractThread(void *param)
{
	CZipManager *zip = (CZipManager *)param;
	unsigned result = 0;
	if (zip == NULL)
		return result;
	zip->m_bDone = FALSE;
	zip->m_bFailed = FALSE;
	std::string dir = zip->m_strTarget;
	std::string message;
	ZIPENTRY entry;
	memset(&entry, 0, sizeof(entry));
	HZIP handle;
	if (zip->m_nOperation == OP_ADD)
	{
		handle = CreateZip((void *)zip->m_strZipFile.c_str(), 1, ZIP_FILENAME);
	}
	else
	{
		handle = OpenZip((void *)zip->m_strZipFile.c_str(), 0, ZIP_FILENAME);
		if (GetZipItem(handle, -1, &entry) != ZR_OK)
		{
			zip->m_bDone = TRUE;
			zip->m_bFailed = TRUE;
			CloseAnyZip(handle);
			return 1;
		}
	}
	if (handle == NULL)
	{
		StrFormat(message, "Failed to open '%s'. File may be corrupt or damaged.", zip->m_strZipFile.c_str());
		zip->ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
		zip->m_bDone = TRUE;
		zip->m_bFailed = FALSE;
		return 1;
	}

	if (zip->m_nOperation == OP_EXTRACT_MARKED || zip->m_nOperation == OP_EXTRACT_ALL)
	{
		BOOL all = zip->m_nOperation == OP_EXTRACT_ALL;
		if (StrRight(dir, 1).compare("\\") != 0)
			dir.append("\\");
		if (!CreateDirectoryTree(dir.c_str()))
		{
			zip->m_bFailed = TRUE;
			result = 2;
		}
		else
		{
			zip->m_nExtracted = 0;
			std::string path;
			for (std::vector<SZipEntry>::iterator it = zip->m_entries.begin(); it != zip->m_entries.end(); ++it)
			{
				if (!all && !it->marked)
					continue;
				memset(&entry, 0, sizeof(entry));
				if (GetZipItem(handle, it->index, &entry) != ZR_OK)
					continue;
				// The entry's path under dir (':' made '_').
				path = entry.name;
				StrReplaceAll(path, ":", "_");
				std::string name = path;
				StrFormat(path, "%s%s", dir.c_str(), name.c_str());
				StrReplaceAll(path, "\\\\", "\\");
				StrReplaceAll(path, "/", "\\");
				DeleteFileA(path.c_str());
				std::string folder = path;
				int slash = StrReverseFind(folder, "\\");
				if (slash > 3 && slash > (int)dir.size())
				{
					folder = StrLeft(folder, slash);
					CreateDirectoryTree(folder.c_str());
				}
				memset(zip->m_szCurrentFile, 0, sizeof(zip->m_szCurrentFile));
				MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, zip->m_szCurrentFile, 0x104);
				if (UnzipItem(handle, it->index, (void *)path.c_str(), 0, ZIP_FILENAME) != ZR_OK)
				{
					zip->m_bFailed = TRUE;
					result = 2;
					break;
				}
				SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
				zip->m_nExtracted++;
			}
			if (zip->m_nExtracted == 0 && !zip->m_bFailed)
			{
				// Nothing marked: the highlighted entry. (Its name is used
				// as it is: no ':' or folder handling.)
				SZipEntry current = zip->m_entries[zip->m_nSelected];
				memset(&entry, 0, sizeof(entry));
				if (GetZipItem(handle, current.index, &entry) == ZR_OK)
				{
					path = dir;
					path.append(entry.name);
					StrReplaceAll(path, "\\\\", "\\");
					DeleteFileA(path.c_str());
					memset(zip->m_szCurrentFile, 0, sizeof(zip->m_szCurrentFile));
					MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, zip->m_szCurrentFile, 0x104);
					if (UnzipItem(handle, current.index, (void *)path.c_str(), 0, ZIP_FILENAME) == ZR_OK)
					{
						SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
						zip->m_nExtracted++;
					}
					else
					{
						zip->m_bFailed = TRUE;
						result = 2;
					}
				}
			}
		}
	}
	else if (zip->m_nOperation == OP_ADD)
	{
		const char *add = zip->m_strAddPath.c_str();
		if (DirectoryExists(add))
		{
			// A folder: every file under it, named without the drive.
			while (g_bFileOperationActive)
				Sleep(50);
			g_fileList.clear();
			g_bFileOperationActive = TRUE;
			g_ullOperationTotal = 0;
			g_ullOperationBytes = 0;
			g_bFileOperationPaused = FALSE;
			g_bCancelOperation = FALSE;
			g_nOperationFolders = 0;
			g_nOperationFiles = 0;
			CollectFiles(add, NULL);
			Sleep(5);
			std::string name;
			for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end(); ++it)
			{
				if (zip->m_bCancel || g_bCancelOperation)
				{
					zip->m_bCancel = TRUE;
					break;
				}
				if (it->isFolder)
					continue;
				memset(zip->m_szCurrentFile, 0, sizeof(zip->m_szCurrentFile));
				MultiByteToWideChar(CP_ACP, 0, it->path.c_str(), (int)it->path.size(), zip->m_szCurrentFile, 0x104);
				name = it->path;
				if (StrFind(name, ":") != -1)
					name = StrMid(name, 3, (int)name.size() - 3);
				if (ZipAdd(handle, name.c_str(), (void *)it->path.c_str(), 0, ZIP_FILENAME) != ZR_OK)
				{
					zip->m_bFailed = TRUE;
					result = 1;
					break;
				}
				g_nOperationProgress++;
			}
			g_bFileOperationActive = FALSE;
		}
		else if (FileExists(add))
		{
			// (Named after the target folder.)
			if (ZipAdd(handle, dir.c_str(), (void *)add, 0, ZIP_FILENAME) != ZR_OK)
			{
				zip->m_bFailed = TRUE;
				result = 2;
			}
		}
		else
		{
			SetLastError(3);
			zip->m_bFailed = TRUE;
			result = 3;
		}
	}
	if (handle != NULL)
		CloseAnyZip(handle);
	zip->m_bDone = TRUE;
	zip->m_bCancel = FALSE;
	return result;
}

// (inlined in ProcessInput)
void CZipManager::ScrollToSelection()
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

// 0x000149E6
HRESULT CZipManager::ProcessInput(int *action, SGamepadState **pad)
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
		if (choice == 6)
		{
			// (No such entry.)
			ShowBrowser(TRUE);
			m_bRunning = FALSE;
			m_nOperation = OP_ADD;
			return S_OK;
		}
		if (choice != 0 && choice != 1 && choice != 3 && choice != 4)
			return S_OK;
		// 1 and 4: ask for the folder; 0 and 3 extract next to the zip,
		// into a folder named after it.
		if (choice == 1 || choice == 4)
			ShowBrowser(FALSE);
		m_bRunning = FALSE;
		m_nOperation = (choice == 0 || choice == 1) ? OP_EXTRACT_MARKED : OP_EXTRACT_ALL;
		int dot = StrReverseFind(m_strZipFile, ".");
		if (dot == -1)
			m_strTarget = GetPathDirectory(m_strZipFile.c_str());
		else
			m_strTarget = StrLeft(m_strZipFile, dot);
		return S_OK;
	}

	// An operation: start it, then report.
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
			StrFormat(message, "Successfully finished Requested operation.\n%d file(s) unzipped successfully.",
			          m_nExtracted);
			ShowMessage(message.c_str(), L"Success", MSGBOX_BUTTONS_OK);
		}
		else
		{
			StrFormat(message,
			          "An error occured while performing requested operation.\nError Code: %d.\n%d file(s) unzipped "
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

// 0x00014F46
BOOL CZipManager::Render()
{
	if (!m_bInitialized)
		return S_OK;
	// An empty zip closes once listed.
	if (m_bListed && !m_bMessageShown && m_entries.size() == 0)
	{
		m_bOpen = FALSE;
		return E_FAIL;
	}
	DimScreen(0.3f);
	m_pCaptionFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	if (!s_bZipHeaderHeight)
	{
		s_fZipHeaderHeight = m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + 12.0f;
		s_bZipHeaderHeight = TRUE;
	}
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 1);
	DrawRect(m_fX + 10.0f, m_fY + 5.0f, m_fWidth - 20.0f, s_fZipHeaderHeight - 10.0f, g_colorScheme.DIALOG_BORDERCOLOR,
	         g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	DrawRect(m_fX + 10.0f, s_fZipHeaderHeight + m_fY, m_fWidth - 20.0f, (m_fHeight - s_fZipHeaderHeight) - 10.0f,
	         g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	wchar_t title[0x104];
	memset(title, 0, sizeof(title));
	MultiByteToWideChar(CP_ACP, 0, m_strZipFile.c_str(), (int)m_strZipFile.size(), title, 0x104);
	m_pCaptionFont->DrawText(m_fX + 12.0f,
	                         (m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + 10.0f) * 0.5f + m_fY,
	                         g_colorScheme.CAPTION_TEXTCOLOR, title, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                         (m_fWidth - 20.0f) - 4.0f);

	if (!m_bListed)
	{
		DrawBusyBox(m_pFont, L"Opening Zip File");
		return S_OK;
	}
	if (m_nVisible == 0)
	{
		m_nVisible = (int)((m_fHeight - (m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight * 2.0f + 20.0f)) /
		                   (m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight + 3.0f));
		m_nTop = 0;
		m_nBottom = m_nVisible - 1;
	}

	// The entries: name, and the size in the last quarter.
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
	if (m_bDone)
		return S_OK;
	if (m_bCancel)
	{
		DrawBusyBox(m_pFont, L"Please Wait");
		return S_OK;
	}
	if (!m_bRunning || index < 1)
		return S_OK;
	// The progress, by files extracted. TODO: verify the arguments
	wchar_t file[310];
	swprintf(file, 310, L"%ls", m_szCurrentFile);
	float percent = (float)m_nExtracted * 100.0f / (float)m_entries.size();
	DrawProgressBox(m_pFont, x, m_fHeight * 0.5f, width, 0.0f, L"Extracting", file, NULL, percent, -1.0f, FALSE);
	return S_OK;
}
