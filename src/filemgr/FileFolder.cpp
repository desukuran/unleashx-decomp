// IFileFolder and CFileFolder: the file manager's panes. See FileFolder.h.
#include "FileFolder.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/D3D.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/SystemControl.h"
#include "../system/SystemInfo.h"
#include "../ui/Menu.h"

BOOL g_bFolderListChanged = FALSE;

// The panes' sounds (looked up by Init). 0x002761C8..0x002761D4
CSound *g_pFolderSelectSound = NULL;     // "Select"
CSound *g_pFolderScrollSound = NULL;     // "Scroll"
CSound *g_pFolderDoneSound = NULL;       // "Action Complete"
BOOL g_bFolderSounds = FALSE;            // "Select" exists and has a buffer

static void PlayFolderSound(CSound *sound)
{
	if (g_bFolderSounds && sound != NULL)
		sound->Play(0);
}

// A size for the size column: "123", or with inKB "12 KB" / "34 MB" from
// 11 KB and 11 MB up. (inlined)
static void FormatItemSize(wchar_t *out, ULONGLONG size, BOOL inKB)
{
	if (!inKB)
	{
		swprintf(out, 0x14, L"%I64u", size);
		return;
	}
	if ((size >> 20) > 10)
	{
		FormatNumberW(out, size >> 20);
		wcscat(out, L" MB");
	}
	else if ((size >> 10) > 10)
	{
		FormatNumberW(out, size >> 10);
		wcscat(out, L" KB");
	}
	else
	{
		FormatNumberW(out, size);
	}
}

// The name compare of the sort, case-insensitive. (a in ecx, b in eax)
static int CompareNamesNoCase(const wchar_t *a, const wchar_t *b)   // 0x0009E5F5
{
	unsigned ca, cb;
	do
	{
		ca = (unsigned short)towlower(*a++);
		cb = (unsigned short)towlower(*b++);
	} while (ca != 0 && ca == cb);
	return (int)ca - (int)cb;
}

// --- SFolderItem ---

// 0x00013622 (this in esi)
SFolderItem::SFolderItem()
{
	m_pszName = NULL;
	m_pszExtension = NULL;
	memset(m_szSize, 0, sizeof(m_szSize));
	m_dwSize = 0;
	m_bFolder = FALSE;
	m_nType = ITEM_OTHER;
	m_bMarked = FALSE;
	m_bParent = FALSE;
	m_date.dwLowDateTime = 0;
	m_date.dwHighDateTime = 0;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00013658 (this in eax)
SFolderItem::~SFolderItem()
{
	delete[] m_pszName;
	delete[] m_pszExtension;
}

// 0x00013679 (isFolder in eax)
BOOL SFolderItem::Init(const char *name, DWORD size, FILETIME date, ULONGLONG displaySize, ULONGLONG dateNumber,
                       BOOL inKB, BOOL isFolder)
{
	m_nType = ITEM_OTHER;
	std::string text = name != NULL ? name : "";
	m_dwSize = size;
	m_bFolder = isFolder;
	if (name == NULL || (int)text.size() < 1)
	{
		delete[] m_pszName;
		m_pszName = NULL;
		return FALSE;
	}
	int length = (int)strlen(name);
	m_pszName = new wchar_t[length + 1];
	if (m_pszName == NULL)
		return FALSE;
	StrUtf8ToWide(name, m_pszName, length);
	memset(m_szSize, 0, sizeof(m_szSize));
	memset(m_szDate, 0, sizeof(m_szDate));
	m_date = date;

	// The type from the extension.
	int dot = StrReverseFind(text, ".");
	if (isFolder)
	{
		m_nType = ITEM_FOLDER;
	}
	else if (dot != -1)
	{
		m_bParent = text.compare("..") == 0;
		std::string extension = StrMid(text, dot + 1, (int)text.size() - (dot + 1));
		m_pszExtension = new char[extension.size() + 1];
		strcpy(m_pszExtension, extension.c_str());
		static const struct
		{
			const char *extension;
			int type;
		} s_types[] = {
			{"xbe", ITEM_XBE},     {"wav", ITEM_AUDIO},   {"wma", ITEM_AUDIO},   {"mp3", ITEM_AUDIO},
			{"wmv", ITEM_VIDEO},   {"xmv", ITEM_VIDEO},   {"bmp", ITEM_IMAGE},   {"jpg", ITEM_IMAGE},
			{"png", ITEM_IMAGE},   {"xbx", ITEM_IMAGE},   {"xpr", ITEM_IMAGE},   {"txt", ITEM_TEXT},
			{"xml", ITEM_TEXT},    {"ini", ITEM_TEXT},    {"nfo", ITEM_TEXT},    {"cfg", ITEM_TEXT},
			{"lst", ITEM_TEXT},    {"zip", ITEM_ARCHIVE}, {"xip", ITEM_ARCHIVE}, {"rar", ITEM_ARCHIVE},
			{"xbg", ITEM_MESH},  {"xm", ITEM_MESH},
		};
		for (size_t i = 0; i < sizeof(s_types) / sizeof(s_types[0]); i++)
		{
			if (StrCmpNoCase(extension.c_str(), s_types[i].extension) == 0)
			{
				m_nType = s_types[i].type;
				break;
			}
		}
	}
	if (m_pszExtension == NULL)
	{
		m_pszExtension = new char[1];
		m_pszExtension[0] = 0;
	}

	// The size column.
	if (displaySize == 0)
	{
		if (!inKB)
		{
			// (The original passes the 32-bit size to "%I64u".)
			swprintf(m_szSize, 0x14, L"%u", size);
		}
		else if ((size >> 20) > 10)
		{
			FormatIntW(m_szSize, (int)(size >> 20));
			wcscat(m_szSize, L" MB");
		}
		else if ((size >> 10) > 10)
		{
			FormatIntW(m_szSize, (int)(size >> 10));
			wcscat(m_szSize, L" KB");
		}
		else
		{
			FormatIntW(m_szSize, (int)size);
		}
	}
	else
	{
		FormatItemSize(m_szSize, displaySize, inKB);
	}

	// The date column: "date time", or the number.
	if (date.dwLowDateTime != 0)
	{
		wchar_t time[10];
		memset(time, 0, sizeof(time));
		FormatFileTime(&date, m_szDate, time, FALSE, m_pSettings->m_prefs.m_bUse24HFormat);
		wcscat(m_szDate, L" ");
		wcscat(m_szDate, time);
	}
	else if (dateNumber != 0)
	{
		FormatItemSize(m_szDate, dateNumber, inKB);
	}
	return TRUE;
}

// 0x00013597
std::string SFolderItem::GetName() const
{
	return m_pszName != NULL ? StrFromWide(m_pszName) : std::string();
}

// --- IFileFolder ---

// 0x00013EEF (this in esi)
IFileFolder::IFileFolder()
{
	m_unknown04 = 0;
	m_unknown3CC = 0;
	m_fY = 50.0f;
	m_fX = 50.0f;
	m_fWidth = 540.0f;
	m_fHeight = 380.0f;
	m_nMaxNameLength = 0;
	m_pFont = NULL;
	m_nVisible = 0;
	m_nTop = 0;
	m_nBottom = 0;
	m_nCount = 0;
	m_strPath = "/";
	m_pCurrent = NULL;
	m_bDetails = FALSE;
	m_pFolderIcon = NULL;
	m_pFileIcon = NULL;
	m_bActive = TRUE;
	m_fDateWidth = 0.0f;
	memset(m_szTitle, 0, sizeof(m_szTitle));
	m_nLevel = 0;
	memset(m_selectionStack, 0, sizeof(m_selectionStack));
	memset(m_topStack, 0, sizeof(m_topStack));
	m_bOnDisc = FALSE;
	m_nFilter = FILTER_ALL;
	m_dwRefreshTick = 0;
	m_nSelected = 0;
	m_nFolderCount = 0;
	m_nFileCount = 0;
	memset(m_szTotalSize, 0, sizeof(m_szTotalSize));
	m_nSort = SORT_NAME;
	m_fIconWidth = 0.0f;
	m_fIconHeight = 0.0f;
	m_bListed = FALSE;
	m_bCollected = TRUE;
	m_bOperationDone = TRUE;
	m_bSortDescending = TRUE;   // (it means ascending, see AddEntry)
	m_bNotDrawing = TRUE;
	m_ullTotalSize = 0;
	m_unknown33C = 0;
	m_unknown348 = 0;
	memset(m_rect, 0, sizeof(m_rect));
	m_fFontScaleX = 1.0f;
	m_fFontScaleY = 1.0f;
}

// 0x00014040
IFileFolder::~IFileFolder()
{
	m_pFont = NULL;
	ClearItems();
	m_items.clear();
	ReleaseTexture(m_pFileIcon);
	ReleaseTexture(m_pFolderIcon);
}

// (inlined: the two loops that delete the items)
void IFileFolder::ClearItems()
{
	for (std::vector<SFolderItem *>::iterator it = m_files.begin(); it != m_files.end(); ++it)
		delete *it;
	m_files.clear();
	for (std::vector<SFolderItem *>::iterator it = m_folders.begin(); it != m_folders.end(); ++it)
		delete *it;
	m_folders.clear();
}

// (inlined: the walks to the index'th shown item)
SFolderItem *IFileFolder::GetItem(int index)
{
	if (index < 0 || index >= (int)m_items.size())
		return NULL;
	return m_items[index];
}

// The end of each LoadList(): the highlighted item, the line height, and
// the scroll position back where this level was. (inlined)
void IFileFolder::FitList()
{
	if (m_items.size() != 0)
		m_pCurrent = m_items[0];
	m_nLineHeight = (int)(m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight) + 2;
	int count = (int)m_items.size();
	m_nCount = count;
	m_nVisible = (int)(m_fHeight / (float)m_nLineHeight);
	if (count < m_nVisible)
		m_nVisible = count;
	int top = m_topStack[m_nLevel];
	int selected = m_nSelected;
	m_nTop = top;
	if (selected < top)
		m_nTop = selected;
	if (count - 1 < m_nTop)
		m_nTop = 0;
	m_nBottom = m_nVisible - 1 + m_nTop;
	if (count - 1 < selected)
	{
		m_nSelected = 0;
		m_nTop = 0;
	}
	else if (m_nBottom < selected)
	{
		m_nTop = (selected - m_nBottom) - 1 + m_nVisible;
		m_nBottom = m_nTop - 1 + m_nVisible;
	}
}

// 0x0012AA4E
void IFileFolder::AddEntry(const WIN32_FIND_DATAA *data, ULONGLONG displaySize, ULONGLONG dateNumber, BOOL inKB)
{
	BOOL folder = (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (folder ? m_nFilter == FILTER_FILES : m_nFilter == FILTER_FOLDERS)
		return;
	SFolderItem *item = new SFolderItem;
	if (item == NULL)
		return;
	if (!item->Init(data->cFileName, data->nFileSizeLow, data->ftLastWriteTime, displaySize, dateNumber, inKB,
	                folder))
	{
		delete item;
		return;
	}
	m_ullTotalSize += data->nFileSizeLow;
	std::vector<SFolderItem *> *list;
	if (!folder)
	{
		m_nFileCount++;
		list = &m_files;
	}
	else
	{
		m_nFolderCount++;
		list = &m_folders;
	}

	// Insertion sort. m_bSortDescending (TRUE by default) actually means
	// ascending: with it clear, sizes and dates come largest first and
	// names Z to A. Dates compare only their low 32 bits. A name already
	// there isn't added.
	BOOL ascending = m_bSortDescending != 0;
	std::vector<SFolderItem *>::iterator it = list->begin();
	for (; it != list->end(); ++it)
	{
		SFolderItem *other = *it;
		if (other == NULL)
			continue;
		if (m_nSort == SORT_DATE || m_nSort == SORT_SIZE)
		{
			DWORD a = m_nSort == SORT_DATE ? other->m_date.dwLowDateTime : other->m_dwSize;
			DWORD b = m_nSort == SORT_DATE ? item->m_date.dwLowDateTime : item->m_dwSize;
			if (ascending ? a > b : a <= b)
				break;
		}
		else if (m_nSort == SORT_TYPE)
		{
			int r = _stricmp(other->m_pszExtension, item->m_pszExtension);
			if (ascending ? r > 0 : r < 0)
				break;
		}
		else
		{
			int r = CompareNamesNoCase(other->m_pszName, item->m_pszName);
			if (ascending ? r > 0 : r < 0)
				break;
			if (r == 0)
			{
				delete item;
				return;
			}
		}
	}
	list->insert(it, item);
}

// 0x0012A2B4
void IFileFolder::CollectFiles(const char *path)
{
	::CollectFiles(path, NULL);
}

// 0x0012A006
BOOL IFileFolder::DeleteListed()
{
	m_bOperationDone = FALSE;
	BOOL result = FALSE;
	BOOL anyFailed = FALSE;
	BOOL failedButContinued = FALSE;
	std::string message = "BEGIN Delete Operation";
	LogOperation(message.c_str(), TRUE);
	CAppSettings *settings = CAppSettings::GetInstance();
	BOOL led = SetLedPattern(settings->m_prefs.m_nLEDBusy, FALSE);

	// The files...
	for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end() && !g_bCancelOperation;
	     ++it)
	{
		if (it->isFolder)
		{
			Sleep(1);
			continue;
		}
		StrUtf8ToWide(it->path.c_str(), g_szOperationFile, 0x12F);
		SetFileAttributesA(it->path.c_str(), FILE_ATTRIBUTE_NORMAL);
		if (DeleteFileA(it->path.c_str()))
		{
			g_ullOperationDone += it->size;
			g_nOperationProgress++;
			continue;
		}
		g_dwOperationError = GetLastError();
		StrFormat(message, "Delete FAILED \"%s\". Error Code: %d", it->path.c_str(), (int)g_dwOperationError);
		LogOperation(message.c_str(), FALSE);
		anyFailed = TRUE;
		result = TRUE;
		if (settings->m_prefs.m_bAbortOnError)
		{
			g_bOperationFailed = TRUE;
			break;
		}
		failedButContinued = TRUE;
	}

	// ...then the folders, last listed (deepest) first.
	Sleep(5);
	for (int i = (int)g_fileList.size() - 1; i >= 0 && !g_bCancelOperation; i--)
	{
		SFileListItem &item = g_fileList[i];
		if (item.path.size() <= 1 || !item.isFolder)
			continue;
		StrUtf8ToWide(item.path.c_str(), g_szOperationFile, 0x12F);
		SetFileAttributesA(item.path.c_str(), FILE_ATTRIBUTE_NORMAL);
		Sleep(1);
		if (!RemoveDirectoryA(item.path.c_str()))
		{
			g_bOperationFailed = TRUE;
			result = TRUE;
			break;
		}
		g_nOperationProgress++;
	}

	if (!g_bOperationFailed && !(g_bOperationFailed = failedButContinued) && !anyFailed)
		g_strOperationLog.erase();
	else
		LogOperation("END Delete Operation", FALSE);
	if (led)
		SetLedPattern(LED_DEFAULT, FALSE);
	m_bOperationDone = TRUE;
	g_bFileOperationPaused = TRUE;
	return result;
}

// The current folder with a trailing backslash ("C" -> "C:\"). (inlined in
// RenameCurrent, CreateFolder and the file manager)
static std::string FolderWithBackslash(IFileFolder *pane)
{
	std::string dir = pane->GetCurrentPath();
	if (dir.size() == 1)
		dir.append(":\\");
	else if (StrRight(dir, 1).compare("\\") != 0)
		dir.append("\\");
	return dir;
}

// 0x00127FA0
BOOL IFileFolder::RenameCurrent(const char *name)
{
	std::string dir = FolderWithBackslash(this);
	std::string target = dir + std::string(name);
	dir.append(m_pCurrent->GetName());
	return MoveFileA(dir.c_str(), target.c_str());
}

// 0x00127E8E
BOOL IFileFolder::CreateFolder(const char *name)
{
	std::string dir = FolderWithBackslash(this);
	dir.append(name);
	while (StrRight(dir, 1).compare("\\") == 0)
		dir = StrLeft(dir, (int)dir.size() - 1);
	return CreateDirectoryTree(dir.c_str());
}

// 0x00128D9D
HRESULT IFileFolder::ProcessInput(int *action, SGamepadState **pad)
{
	if (!m_bListed)
		return S_OK;
	// The stick only counts all the way up or down.
	if (*action == INPUT_LTHUMB && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}
	if (*action == INPUT_NONE)
	{
		if (m_nSelected < m_nTop)
			m_nTop = m_nSelected;
		return S_OK;
	}

	if (*action == INPUT_POINTER_CLICK)
	{
		// The line under the pointer; clicking the highlighted one marks it.
		if (!g_bPointerClicked || !PointInRect(m_rect, g_nPointerX, g_nPointerY))
			return S_OK;
		float lineHeight = (float)floor(m_pFont->m_fScaleFactorY * m_pFont->m_fCellHeight + 2.0f);
		if (m_fIconHeight > lineHeight)
			lineHeight = m_fIconHeight;
		int line = (int)floor((double)(unsigned)(g_nPointerY - m_rect[1]) / lineHeight);
		int index = m_nTop + line;
		if ((unsigned)index >= m_items.size())
			return S_OK;
		if (g_bPointerDoubleClick)
		{
			m_nSelected = index;
			*action = INPUT_A;
			return S_OK;
		}
		if (m_nSelected != index)
		{
			m_nSelected = index;
			return S_OK;
		}
		*action = INPUT_X;
	}

	switch (*action)
	{
	case INPUT_BLACK:
		if (m_strPath.size() > 1)
			InvertMarks();
		return S_OK;

	case INPUT_UP:
	case INPUT_LTRIGGER:
		if (m_nSelected > 0)
		{
			m_nSelected--;
			if (m_nSelected < m_nTop)
			{
				m_nTop--;
				m_nBottom = m_nVisible - 1 + m_nTop;
			}
			PlayFolderSound(g_pFolderScrollSound);
			return S_OK;
		}
		return S_OK;

	case INPUT_DOWN:
	case INPUT_RTRIGGER:
		if (m_nSelected < m_nCount - 1)
		{
			m_nSelected++;
			if (m_nSelected > m_nBottom)
			{
				m_nTop++;
				m_nBottom++;
			}
			PlayFolderSound(g_pFolderScrollSound);
		}
		return S_OK;

	case INPUT_X:
	{
		// Mark (not in Root, not ".."), then move on.
		if (m_nSelected < 1 || m_strPath.size() < 2)
			return S_OK;
		g_bFolderListChanged = TRUE;
		SFolderItem *item = GetItem(m_nSelected);
		if (item == NULL)
			return S_OK;
		item->m_bMarked = !item->m_bMarked;
		PlayFolderSound(g_pFolderDoneSound);
		if (!item->m_bMarked || m_nSelected >= m_nCount - 1)
			return S_OK;
		m_nSelected++;
		if (m_nSelected > m_nBottom)
		{
			m_nTop++;
			m_nBottom++;
		}
		return S_OK;
	}

	case INPUT_B:
	{
		// Up a level.
		if (m_strPath.size() < 2)
			return S_OK;
		g_bFolderListChanged = TRUE;
		m_nLevel--;
		m_nSelected = m_selectionStack[m_nLevel];
		int slash = StrReverseFind(m_strPath, "/");
		if (slash < 1)
			m_strPath = "/";
		else
			m_strPath = StrLeft(m_strPath, slash);
		Refresh();
		PlayFolderSound(g_pFolderDoneSound);
		return S_OK;
	}
	}
	return S_OK;
}

// 0x001268A3
HRESULT IFileFolder::Render()
{
	if (!m_bListed)
	{
		if (GetTickCount() - m_dwRefreshTick > 250)
			DrawBusyBox(m_pFont, L"Checking");
		return S_OK;
	}
	if (m_nCount <= 0)
		return S_OK;

	float bottom = m_fY + m_fHeight;
	float lineHeight = (float)floor(m_pFont->m_fScaleFactorY * m_pFont->m_fCellHeight + 2.0f);
	m_pFont->SetTextHeight((int)lineHeight);
	if (m_fIconHeight > lineHeight)
		lineHeight = m_fIconHeight;
	float halfLine = lineHeight * 0.5f;
	float y = halfLine + m_fY + 3.0f;
	float x = m_fX + 3.0f;
	// Folder levels show the icon column.
	float iconColumn = m_nLevel != 0 ? m_fIconWidth : 0.0f;
	float right = (m_fWidth + m_fX) - 6.0f;
	if (m_bDetails)
		right -= m_fDateWidth;
	m_bNotDrawing = FALSE;
	if ((unsigned)m_nTop > m_items.size())
		m_nTop = 0;
	int index = m_nTop;
	wchar_t text[40];
	memset(text, 0, sizeof(text));
	SetScissorRect(TRUE, m_fX + 1.0f, m_fY + 1.0f, m_fWidth - 1.0f, m_fHeight - 1.0f);

	for (std::vector<SFolderItem *>::iterator it = m_items.begin() + m_nTop; it != m_items.end(); ++it)
	{
		SFolderItem *item = *it;
		if (index >= m_nTop && item != NULL)
		{
			BOOL selected = index == m_nSelected;
			DWORD color = g_colorScheme.DIALOG_TEXTCOLOR;
			DWORD flags = XBFONT_CENTER_Y | XBFONT_TRUNCATED;
			if (selected || item->m_bMarked)
			{
				if (selected)
				{
					m_pCurrent = item->m_bParent ? NULL : item;
					// The highlight, fainter in the inactive pane.
					DWORD fade = 0;
					if (!m_bActive)
						fade = ((g_colorScheme.OPTION_ACTIVEBACKCOLOR >> 24) * 2) / 3;
					float highlightRight = m_bDetails ? m_fDateWidth + right : right;
					DrawSelectionBox(x - 1.0f, y - halfLine - 1.0f, highlightRight + 1.0f, halfLine + y + 1.0f,
					                g_colorScheme.DIALOG_BORDERCOLOR - (fade << 24),
					                g_colorScheme.OPTION_ACTIVEBACKCOLOR - (fade << 24));
				}
				color = item->m_bMarked ? g_colorScheme.OPTION_ACTIVETEXTCOLOR : g_colorScheme.DIALOG_TEXTCOLOR;
				flags = (selected ? 0 : XBFONT_TRUNCATED) | XBFONT_CENTER_Y;
			}

			if (item->m_bParent)
			{
				m_pFont->DrawText(x, y, color, item->m_pszName, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
				                  m_fWidth * 0.8f);
			}
			else
			{
				m_pFont->DrawText(iconColumn + x, y, color, item->m_pszName, flags, (float)(m_fWidth * 0.75));
				BOOL sizeColumn = TRUE;
				if (!item->m_bFolder)
				{
					if (m_pFileIcon != NULL)
						DrawTexture(m_pFileIcon, x, y - m_fIconHeight * 0.5f, m_fIconWidth - 2.0f, m_fIconHeight,
						            1.0f, NULL, 0.0f, NULL);
				}
				else if (m_nLevel != 0)
				{
					// Folders: their icon, or "<Folder>" in the size column.
					sizeColumn = FALSE;
					if (m_pFolderIcon != NULL)
						DrawTexture(m_pFolderIcon, x, y - m_fIconHeight * 0.5f, m_fIconWidth - 2.0f, m_fIconHeight,
						            1.0f, NULL, 0.0f, NULL);
					else
						m_pFont->DrawText(right, y, color, L"<Folder>", XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);
				}
				if (sizeColumn && !m_bDetails)
					m_pFont->DrawText(right, y, color, item->m_szSize, XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);
				if (m_bDetails)
				{
					// "size date time"; in Root the second drive (D:) shows
					// the tray state instead (whatever drive it is).
					const wchar_t *details;
					if (m_nLevel == 0 && index == 1)
					{
						details = GetTrayStateName();
					}
					else
					{
						swprintf(text, 40, L"%ls %ls", item->m_szSize, item->m_szDate);
						details = text;
					}
					m_pFont->DrawText(m_fDateWidth + right, y, color, details, XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);
				}
			}
			y += lineHeight;
			if (bottom < y)
			{
				m_nBottom = index;
				break;
			}
		}
		index++;
		if (index > m_nBottom)
			break;
	}
	m_bNotDrawing = TRUE;
	SetScissorRect(FALSE, 0.0f, 0.0f, 0.0f, 0.0f);
	return S_OK;
}

// 0x00126691
void IFileFolder::Reload()
{
	m_topStack[m_nLevel] = m_nTop;
	m_fDateWidth = m_pFont->GetTextWidth(L"12/39/9999 12:59 AM");
	Refresh();
}

// 0x00129128 (LTCG copy 0x00129136)
HRESULT IFileFolder::Init(CXBFont *font, int /*unused*/, int maxNameLength, IDirect3DTexture8 *folderIcon,
                          IDirect3DTexture8 *fileIcon)
{
	if (font == NULL)
		return E_FAIL;
	m_nMaxNameLength = maxNameLength + 2;
	m_pFont = font;
	m_fFontScaleX = font->m_fScaleFactorX;
	m_fFontScaleY = font->m_fScaleFactorY;
	g_pFolderSelectSound = g_soundEffects.Find("Select");
	if (g_pFolderSelectSound != NULL)
	{
		g_pFolderScrollSound = g_soundEffects.Find("Scroll");
		g_pFolderDoneSound = g_soundEffects.Find("Action Complete");
		g_bFolderSounds = g_pFolderSelectSound != NULL && g_pFolderSelectSound->m_pBuffer != NULL;
	}
	m_pFont->SetScaleFactors(1.0f, 1.0f);
	m_fDateWidth = m_pFont->GetTextWidth(L"12/39/9999 12:59 AM");
	m_pFont->SetScaleFactors(m_fFontScaleX, m_fFontScaleY);   // TODO: verify

	// The icon column fits the larger icon.
	D3DSURFACE_DESC desc;
	if (fileIcon != NULL)
	{
		fileIcon->AddRef();
		m_pFileIcon = fileIcon;
		fileIcon->GetLevelDesc(0, &desc);
		m_fIconWidth = (float)desc.Width + 2.0f;
		m_fIconHeight = (float)desc.Height;
	}
	if (folderIcon != NULL)
	{
		folderIcon->AddRef();
		m_pFolderIcon = folderIcon;
		folderIcon->GetLevelDesc(0, &desc);
		if (m_fIconWidth < (float)desc.Width + 2.0f)
			m_fIconWidth = (float)desc.Width + 2.0f;
		if (m_fIconHeight < (float)desc.Height)
			m_fIconHeight = (float)desc.Height;
	}
	return S_OK;
}

// 0x00013D28
void IFileFolder::SetRect(float x, float y, float width, float height)
{
	m_fY = y;
	m_fX = x;
	if (width > 0.0f)
		m_fWidth = width;
	if (height > 0.0f)
		m_fHeight = height;
	m_rect[0] = (int)(x + 3.0f);
	m_rect[1] = (int)(y + 3.0f);
	m_rect[2] = (int)(width - 6.0f);
	m_rect[3] = (int)(height - 6.0f);
	m_nVisible = (int)(m_fHeight / (float)m_nLineHeight);
	m_nBottom = m_nTop - 1 + (int)(m_fHeight / (float)m_nLineHeight);
}

// 0x001280B7
void IFileFolder::SetPath(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return;
	std::string dir = "/";
	dir.append(path);
	StrReplaceAll(dir, "\\", "/");
	StrReplaceAll(dir, "//", "/");
	StrRemoveChar(dir, ':');
	m_strPath = dir;
	// One level per '/'.
	m_nLevel = 0;
	if (m_strPath.size() != 0 && StrLeft(m_strPath, 1).compare("/") == 0)
		m_nLevel = 1;
	for (int slash = StrFind(m_strPath, "/", 1); slash != -1; slash = StrFind(m_strPath, "/", slash + 1))
		m_nLevel++;
	Refresh();
	PlayFolderSound(g_pFolderDoneSound);
}

// 0x00126222
void IFileFolder::MarkAll(BOOL mark)
{
	while (!m_bListed || !m_bNotDrawing)
		Sleep(1);
	for (std::vector<SFolderItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		if (*it != NULL && !(*it)->m_bParent)
			(*it)->m_bMarked = mark;
	}
	g_bFolderListChanged = TRUE;
}

// 0x001261CE
void IFileFolder::InvertMarks()
{
	while (!m_bListed || !m_bNotDrawing)
		Sleep(1);
	for (std::vector<SFolderItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		if (*it != NULL && !(*it)->m_bParent)
			(*it)->m_bMarked = !(*it)->m_bMarked;
	}
	g_bFolderListChanged = TRUE;
}

// 0x001261A6
BOOL IFileFolder::HasMarks()
{
	for (std::vector<SFolderItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		if (*it != NULL && (*it)->m_bMarked)
			return TRUE;
	}
	return FALSE;
}

// 0x00013DEA
std::string IFileFolder::GetCurrentPath()
{
	std::string path = "";
	if (_wcsicmp(m_szTitle, L"Root") == 0)
	{
		SFolderItem *item = GetItem(m_nSelected);
		if (item != NULL && !item->m_bParent)
			path = item->GetName();
	}
	else
	{
		path = StrFromWide(m_szTitle);
	}
	return path;
}

// 0x001262F5 (this in ecx, sort in edx)
void IFileFolder::SetSort(int sort)
{
	if (sort == m_nSort)
	{
		m_bSortDescending = !m_bSortDescending;
	}
	else
	{
		m_bSortDescending = TRUE;
		m_nSort = sort;
	}
	Reload();
}

// --- CFileFolder ---

// 0x00128C7E (this in eax)
CFileFolder::CFileFolder()
{
	m_pDir = NULL;
	m_pEvents = NULL;
}

// 0x00014145
CFileFolder::~CFileFolder()
{
	if (m_pDir != NULL)
	{
		delete m_pDir;
		m_pDir = NULL;
	}
}

// 0x0012612A
unsigned __stdcall CFileFolder::LoadThread(void *param)
{
	((IFileFolder *)param)->LoadList();
	return 0;
}

// 0x00126647
void CFileFolder::Refresh()
{
	m_bListed = FALSE;
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

// 0x0012A732
void CFileFolder::LoadList()
{
	m_bListed = FALSE;
	while (!m_bNotDrawing)
		Sleep(1);
	Sleep(5);
	m_pCurrent = NULL;
	m_nFolderCount = 0;
	m_nFileCount = 0;
	g_bFolderListChanged = TRUE;
	ClearItems();
	m_ullTotalSize = 0;
	memset(m_szTotalSize, 0, sizeof(m_szTotalSize));
	ListEntries();
	m_items.clear();

	// "..", then the folders, then the files.
	if (m_strPath.size() > 1)
	{
		SFolderItem *parent = new SFolderItem;
		if (parent != NULL)
		{
			FILETIME none = {0, 0};
			if (!parent->Init("..", 0, none, 0, 0, TRUE, FALSE))
				delete parent;
			else
				m_items.push_back(parent);
		}
	}
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
	// (The ".." item is never deleted: it's in neither list.)

	// The total: "123", "12 KB", "3 MB" or "2 GB", from 11 of a unit.
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
	m_bListed = TRUE;
}

// 0x00127B17
void CFileFolder::BuildPath(std::string *out, const char *base, const char *name)
{
	std::string dir = base;
	std::string file = name;
	if (file.size() == 0)
	{
		*out = base;
		return;
	}
	StrReplaceAll(file, "\\", "/");
	while (StrReplaceAll(file, "//", "/") != 0)
		;
	StrTrimRightChars(file, "/");
	if (StrLeft(file, 1).compare("/") != 0)
	{
		// Relative: under base (or the current folder).
		if (dir.size() == 0)
			dir = m_strPath;
		std::string relative = file;
		if (StrRight(dir, 1).compare("/") == 0)
			StrFormat(file, "%s%s", base, relative.c_str());
		else
			StrFormat(file, "%s/%s", base, relative.c_str());
	}
	*out = file;
}

// 0x00127C60
void CFileFolder::ToXboxPath(std::string *out, const char *path)
{
	// "/c/apps/x" -> "C:\apps\x"; "/c" -> "c:\". (Only a drive followed
	// by a folder is upper-cased.) A doubled colon is then made single.
	char buffer[260];
	memset(buffer, 0, sizeof(buffer));
	int skip = 0;
	while (path[skip] == '/')
		skip++;
	const char *p = path + skip;
	const char *slash = strchr(p, '/');
	int n = 0;
	if (slash == NULL)
	{
		for (; *p != 0; p++)
			buffer[n++] = *p;
		buffer[n] = ':';
		buffer[n + 1] = '\\';
		n += 2;
	}
	else
	{
		for (; p < slash; p++)
			buffer[n++] = (char)toupper(*p);
		buffer[n] = ':';
		buffer[n + 1] = '\\';
		// The rest from the slash (which lands on the backslash).
		for (p = slash; *p != 0; p++, n++)
			buffer[n + 1] = *p == '/' ? '\\' : *p;
		n++;
	}
	buffer[n] = 0;
	*out = buffer;
	StrReplaceAll(*out, "::", ":");
}

// 0x0012A36B
void CFileFolder::ListEntries()
{
	std::string unused1, ftpPath;
	memset(m_szTitle, 0, sizeof(m_szTitle));
	m_nFolderCount = 0;
	m_nFileCount = 0;
	while (m_strPath.size() >= 2)
	{
		std::string dir;
		BuildPath(&ftpPath, m_strPath.c_str(), "");
		ToXboxPath(&dir, ftpPath.c_str());
		m_bOnDisc = StrCmpNoCase(StrLeft(dir, 2), "D:") == 0;
		if (m_pDir == NULL)
		{
			m_pDir = CreateDirObject(dir.c_str());
			if (m_pDir == NULL)
			{
				// The drive is gone: back to Root.
				m_strPath = "/";
				continue;
			}
		}
		// A folder: its entries.
		StrUtf8ToWide(dir.c_str(), m_szTitle, 0x104);
		if (StrRight(dir, 1).compare("\\") == 0)
			dir.append("*");
		else if (StrRight(dir, 1).compare("*") != 0)
			dir.append("\\*");
		WIN32_FIND_DATAA data;
		HANDLE find = m_pDir->FindFirst(dir.c_str(), &data);
		if (find != INVALID_HANDLE_VALUE)
		{
			do
			{
				AddEntry(&data, 0, 0, TRUE);
			} while (m_pDir->FindNext(find, &data));
			if (find != INVALID_HANDLE_VALUE)
				m_pDir->FindClose(find);
		}
		return;
	}

	// Root: the drives.
	if (m_pDir != NULL)
	{
		delete m_pDir;
		m_pDir = NULL;
	}
	m_bOnDisc = FALSE;
	m_nLevel = 0;
	wcscpy(m_szTitle, L"Root");
	RefreshDrives(FALSE);
	static WIN32_FIND_DATAA s_entries[DRIVE_INFO_COUNT];   // (on the stack in the original)
	for (int i = 0; i < DRIVE_INFO_COUNT; i++)
	{
		SDriveInfo &drive = g_drives[i];
		WIN32_FIND_DATAA &data = s_entries[i];
		if (!drive.mounted || drive.letter == 'U' || drive.letter == 'T')
		{
			// An unformatted memory unit or USB drive.
			if (drive.muOrUsb && drive.type == 2)
			{
				memset(&data, 0, sizeof(data));
				data.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				strcpy(data.cFileName, &drive.letter);
				strcat(data.cFileName, " [Not formatted]");
				AddEntry(&data, 0, 0, TRUE);
			}
			data.cFileName[0] = 0;
			continue;
		}
		// The size column: free space (used space for the DVD); the date
		// column: the total size. (The time is converted from an
		// uninitialized SYSTEMTIME; it normally fails and stays 0.)
		sprintf(data.cFileName, "%c", drive.letter);
		data.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		ULONGLONG shown = drive.type == 1 ? drive.usedBytes : drive.freeBytes;
		data.nFileSizeLow = (DWORD)shown;
		data.ftLastWriteTime.dwLowDateTime = 0;
		data.ftLastWriteTime.dwHighDateTime = 0;
		AddEntry(&data, shown, drive.totalBytes, TRUE);
	}
}

// 0x00129874
HRESULT CFileFolder::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_NONE || !m_bListed)
		return S_OK;
	if (*action != INPUT_A)
	{
		IFileFolder::ProcessInput(action, pad);
		if (*action != INPUT_A)
			return S_OK;
	}
	if (m_nSelected < 0 || m_nSelected >= m_nCount)
		return S_OK;
	g_bFolderListChanged = TRUE;
	SFolderItem *item = GetItem(m_nSelected);
	if (item == NULL)
		return S_OK;

	if (!item->m_bFolder)
	{
		if (!item->m_bParent)
		{
			// A file: opened by the file manager.
			std::string path = m_strPath;
			if (StrRight(path, 1).compare("/") != 0)
				path.append("/");
			path.append(StrFromWide(item->m_pszName));
			ToXboxPath(&path, path.c_str());
			if (m_pEvents != NULL)
				m_pEvents->OnOpen(path, item->m_nType, (CXBFontEx *)m_pFont);
			return S_OK;
		}
		// "..": up a level.
		int slash = StrReverseFind(m_strPath, "/");
		if (slash < 1)
			m_strPath = "/";
		else
			m_strPath = StrLeft(m_strPath, slash);
		m_nLevel--;
		m_nSelected = m_selectionStack[m_nLevel];
	}
	else
	{
		// Into the folder; D: only with a disc that has files.
		if (m_strPath.compare("/") == 0 && _wcsnicmp(item->m_pszName, L"D", 1) == 0)
		{
			if (!IsDiscPresent() || GetDiscType() == DISC_AUDIO || GetDiscType() == DISC_UNKNOWN)
				return S_OK;
			m_bOnDisc = TRUE;
		}
		m_topStack[m_nLevel] = m_nTop;
		m_selectionStack[m_nLevel] = m_nSelected;
		m_nLevel++;
		m_nSelected = 0;
		if (StrRight(m_strPath, 1).compare("/") != 0)
			m_strPath.append("/");
		m_strPath.append(StrFromWide(item->m_pszName));
	}
	Refresh();
	PlayFolderSound(g_pFolderDoneSound);
	return S_OK;
}
