// CFileBrowser: the file/folder pop-up. See FileBrowser.h.
#include "FileBrowser.h"

#include <algorithm>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../system/Drives.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "VirtualKeyboard.h"

CAppSettings *g_pFileSettings = NULL;

#define NEW_FOLDER_ITEM "[New Folder]"

// 0x00013C39 (name in edx, this in esi)
SFileItem::SFileItem(const char *itemName, BOOL parent, BOOL folder, int unknown)
{
	isParent = parent;
	isFolder = folder;
	unknown128 = unknown;
	// (name is left as it was when itemName is empty.)
	if (itemName != NULL && strlen(itemName) != 0)
	{
		memset(name, 0, sizeof(name));
		strcpy(name, itemName);
		wchar_t wide[0x104];
		StrUtf8ToWide(itemName, wide, 0x104);
		if (!folder || parent)
		{
			label = wide;
		}
		else
		{
			label = L"<";
			label.append(wide);
			label.append(L">");
		}
	}
}

// 0x001266FD (a in ecx, b in eax)
bool operator<(const SFileItem &a, const SFileItem &b)
{
	if (a.isParent > b.isParent)
		return true;
	if (a.isParent < b.isParent)
		return false;
	if (a.isFolder > b.isFolder)
		return true;
	if (a.isFolder < b.isFolder)
		return false;
	// Among files, names starting with '[' (the "[New Folder]" item) go
	// last.
	if (!a.isFolder && b.name[0] == '[')
		return true;
	if (!b.isFolder && a.name[0] == '[')
		return false;
	return _wcsicmp(a.label.c_str(), b.label.c_str()) < 0;
}

// 0x00127A32 (this in eax)
CFileBrowser::CFileBrowser()
{
	Reset();
}

// 0x001279FC (this in eax)
// The music player's (flag 1): the flag is kept at +0x17C and not read.
CFileBrowser::CFileBrowser(BOOL flag)
{
	Reset();
	m_unknown17C = flag;
}

// 0x00127013 (this in edi)
CFileBrowser::~CFileBrowser()
{
	m_pFont = NULL;
	m_pCaptionFont = NULL;
	m_pSoundMove = NULL;
	m_pSymbolFont = NULL;
	m_items.clear();
}

// 0x001267B6 (this in esi)
void CFileBrowser::Reset()
{
	m_bNewFolderItem = TRUE;
	m_bSelectFiles = FALSE;
	m_strPath.erase();
	m_pFont = NULL;
	m_pCaptionFont = NULL;
	m_pSymbolFont = NULL;
	m_pSoundMove = NULL;
	m_bOpen = TRUE;
	m_bCancelled = FALSE;
	m_bKeyboardOpen = FALSE;
	m_nVisible = 0;
	m_nTop = 0;
	m_nSelected = 0;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	memset(m_selectedStack, 0, sizeof(m_selectedStack));
	memset(m_topStack, 0, sizeof(m_topStack));
	m_bFolderResult = TRUE;
	m_nDepth = 0;
	m_szTitle[0] = 0;
	m_unknown17C = 0;
}

// 0x0012C378 (this in eax)
HRESULT CFileBrowser::Create(CXBFont *font, const wchar_t *title)
{
	if (m_pFont != NULL)
		return S_OK;
	if (font == NULL)
		return E_FAIL;

	memset(m_szTitle, 0, sizeof(m_szTitle));
	if (title == NULL || wcslen(title) == 0)
		wcscpy(m_szTitle, L"Browse Folder");
	else
		wcsncpy(m_szTitle, title, wcslen(title) > 40 ? 40 : wcslen(title));
	m_pFont = font;
	g_pFileSettings = CAppSettings::GetInstance();
	m_pCaptionFont = g_pFileSettings->m_pDialogFont;
	if (m_pCaptionFont == NULL)
		m_pCaptionFont = font;
	m_fY = 30.0f;
	m_fX = 80.0f;
	m_fWidth = 480.0f;
	m_fHeight = 420.0f;
	m_pSoundMove = g_soundEffects.Find("Scroll");
	g_fonts.Find("XboxDings", &m_pSymbolFont);
	Refresh();
	Show(FALSE);
	return S_OK;
}

// 0x000113D9 (this in eax, title in esi)
void CFileBrowser::SetTitle(const wchar_t *title)
{
	if (title == NULL || *title == 0)
	{
		wcscpy(m_szTitle, L"Browse Folder");
		return;
	}
	memset(m_szTitle, 0, sizeof(m_szTitle));
	wcsncpy(m_szTitle, title, wcslen(title) > 40 ? 40 : wcslen(title));
}

// 0x000112BA (this in esi, value in ecx)
void CFileBrowser::SetSelectFiles(BOOL selectFiles)
{
	if (m_bSelectFiles == selectFiles)
		return;
	m_bSelectFiles = selectFiles;
	if (m_items.size() != 0)
		Refresh();
}

// 0x001281F6 (this in eax)
void CFileBrowser::AddFilter(const char *extension)
{
	if (extension == NULL || strlen(extension) == 0)
	{
		m_filters.clear();
		return;
	}
	// (The _strdup copy is never freed.)
	m_filters.push_back(std::string(_strdup(extension)));
}

// 0x0001142F (this in ebx, path in esi)
void CFileBrowser::SetStartPath(const char *path)
{
	if (path == NULL || strlen(path) == 0 || !DirectoryExists(path))
		return;
	m_strPath = path;
	if (m_items.size() != 0)
		Refresh();
}

// 0x0001124E (this in ebx)
void CFileBrowser::Show(BOOL refresh)
{
	m_bOpen = TRUE;
	m_bCancelled = FALSE;
	if (refresh && m_pFont != NULL)
		Refresh();
	m_fY = 30.0f;
	m_fX = 80.0f;
	m_fWidth = 480.0f;
	m_fHeight = 420.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
}

// 0x000112D9 (this in eax)
std::string CFileBrowser::GetSelectedPath()
{
	if (m_bCancelled)
		return "";
	const SFileItem &item = m_items[m_nSelected];
	if (item.isParent)
		return m_strPath;
	std::string path = m_strPath;
	// (Quirk: a drive picked from the drive list comes back as "\C".)
	if ((m_bSelectFiles && !item.isFolder) || _stricmp(item.name, NEW_FOLDER_ITEM) != 0)
	{
		if (StrRight(path, 1).compare("\\") != 0)
			path.append("\\");
		path.append(item.name);
	}
	return path;
}

// 0x0012ABE3 (this on the stack)
HRESULT CFileBrowser::Refresh()
{
	// Back where this depth was last left.
	m_nTop = m_topStack[m_nDepth];
	m_nSelected = m_selectedStack[m_nDepth];
	m_nVisible = 0;
	m_items.clear();

	if ((int)m_strPath.size() < 1)
	{
		// The drive list.
		RefreshDrives(TRUE);
		std::string letter;
		for (int i = 0; i < DRIVE_INFO_COUNT; i++)
		{
			if (!g_drives[i].mounted)
				continue;
			StrFormat(letter, "%c", g_drives[i].letter);
			m_items.push_back(SFileItem(letter.c_str(), FALSE, TRUE, 0));
		}
	}
	else
	{
		m_items.push_back(SFileItem("..", TRUE, TRUE, 0));
		if (m_strPath.size() == 1)
			m_strPath.append(":\\");

		CFileFinder finder;
		finder.m_nFilter = m_bSelectFiles ? 0 : 2;   // folders only unless choosing files
		const char *name = finder.FindFirst(m_strPath.c_str(), "*");
		if (name != NULL)
		{
			while (strlen(finder.GetName()) != 0)
			{
				DWORD attributes = finder.GetAttributes();
				BOOL folder = (attributes >> 4) & 1;   // FILE_ATTRIBUTE_DIRECTORY
				if (!m_bSelectFiles || folder || m_filters.size() == 0)
				{
					m_items.push_back(SFileItem(finder.GetName(), FALSE, folder, 0));
				}
				else
				{
					// An empty filter matches everything.
					std::string found;
					for (std::vector<std::string>::iterator it = m_filters.begin(); it != m_filters.end(); ++it)
					{
						found = finder.GetName();
						if (it->size() == 0 || StrCmpNoCase(StrRight(found, (int)it->size()), it->c_str()) == 0)
						{
							m_items.push_back(SFileItem(finder.GetName(), FALSE, folder, 0));
							break;
						}
					}
				}
				finder.FindNext();
			}
		}
		finder.Close();
		if (m_bNewFolderItem)
			m_items.push_back(SFileItem(NEW_FOLDER_ITEM, FALSE, FALSE, 0));
		if (m_items.size() != 0)
			std::sort(m_items.begin(), m_items.end());
	}

	if ((int)m_items.size() - 1 < m_nSelected)
	{
		m_nSelected = 0;
		m_nTop = 0;
	}
	if ((int)m_items.size() - 1 < m_nTop)
		m_nTop = 0;
	return S_OK;
}

// 0x00129B10 (this in esi)
HRESULT CFileBrowser::Render()
{
	if (m_pFont == NULL)
		return S_OK;
	DimScreen(0.3f);
	float captionHeight = m_pCaptionFont->m_fFontHeight * m_pCaptionFont->m_fScaleFactorY;
	static float s_fUnused = captionHeight + 12.0f;   // 0x0027A3D4 (never read)
	(void)s_fUnused;

	DrawWindowFrame(m_fX, m_fY, m_fWidth, m_fHeight, 10.0f, captionHeight, 40.0f, g_colorScheme.DIALOG_BORDERCOLOR,
	                g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_INNER_BORDERCOLOR,
	                g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 2);
	DrawGradientRect(m_fX + 10.0f, m_fY + 5.0f, m_fWidth * 0.6f - 20.0f, captionHeight + 10.0f,
	                 g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BACKCOLOR, FALSE, 0);
	m_pCaptionFont->DrawText(m_fX + 13.0f, captionHeight * 0.5f + m_fY + 10.0f, g_colorScheme.CAPTION_TEXTCOLOR,
	                         m_szTitle, XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth * 0.75f - 20.0f - 3.0f);

	if (m_items.size() == 0)
		return E_FAIL;

	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	if (m_nVisible == 0)
	{
		m_nVisible = (int)((m_fHeight - (lineHeight + captionHeight + 60.0f)) / (lineHeight + 3.0f));
		m_nTop = 0;
		m_nBottom = m_nVisible - 1;
	}

	float x = m_fX + 13.0f;
	float y = captionHeight + m_fY + 23.0f;
	float width = m_fWidth - 26.0f;
	if ((UINT)m_nTop > m_items.size())
		m_nTop = 0;
	int i = m_nTop;
	for (std::vector<SFileItem>::iterator it = m_items.begin() + i; it != m_items.end(); ++it, i++)
	{
		if (m_nBottom < i)
			break;
		DWORD color;
		if (i == m_nSelected)
		{
			DrawGradientRect(x - 1.0f, y - 1.0f, width, lineHeight + 3.0f, g_colorScheme.OPTION_ACTIVEBACKCOLOR,
			                 g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE, g_colorScheme.OPTION_ACTIVEBACKCOLOR);
			color = g_colorScheme.OPTION_ACTIVETEXTCOLOR;
		}
		else
		{
			color = g_colorScheme.DIALOG_TEXTCOLOR;
		}
		m_pFont->DrawText(x, y, color, it->label.c_str(), XBFONT_TRUNCATED, width);
		y += lineHeight + 3.0f;
		if (m_fHeight + m_fY - 40.0f - 20.0f < y)
		{
			// The box is full: this is how many fit.
			int shown = i - m_nTop;
			m_nBottom = i;
			m_nVisible = shown + 1;
			if (i < m_nSelected)
			{
				m_nBottom = m_nSelected;
				m_nTop = m_nSelected - m_nVisible + 1;
				if (m_nTop < 0)
				{
					m_nTop = 0;
					m_nBottom = shown;
				}
			}
			break;
		}
	}

	// The A/B legend.
	if (m_pSymbolFont != NULL)
	{
		float legendX = width * 0.2f + m_fX + 10.0f;
		float legendY = m_fHeight + m_fY - 25.0f;
		m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"G", XBFONT_CENTER_Y, 0.0f);
		m_pFont->DrawText(legendX + 50.0f, legendY, g_colorScheme.DIALOG_TEXTCOLOR, GetString(STR_ACCEPT),
		                  XBFONT_CENTER_Y, 0.0f);
		legendX = width * 0.4f + legendX;
		m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"H", XBFONT_CENTER_Y, 0.0f);
		m_pFont->DrawText(legendX + 50.0f, legendY, g_colorScheme.DIALOG_TEXTCOLOR, GetString(STR_CANCEL),
		                  XBFONT_CENTER_Y, 0.0f);
	}
	if (m_bKeyboardOpen)
		g_virtualKeyboard.Render();
	return S_OK;
}

// (inlined in ProcessInput)
void CFileBrowser::ScrollToSelection()
{
	int selected = m_nSelected;
	if (selected < m_nTop)
	{
		int bottom = m_nVisible - 1 + selected;
		m_nTop = selected;
		m_nBottom = bottom;
		if ((int)m_items.size() - 1 < bottom)
		{
			m_nBottom = (int)m_items.size() - 1;
			m_nTop = m_nBottom - m_nVisible + 1;
		}
	}
	if (m_nBottom < selected)
	{
		m_nBottom = selected;
		m_nTop = selected - m_nVisible + 1;
		if (m_nTop < 0)
		{
			m_nTop = 0;
			m_nBottom = m_nVisible - 1;
		}
	}
}

// 0x0012AF3F (this in ebx, action in ecx, pad in eax)
HRESULT CFileBrowser::ProcessInput(int *action, SGamepadState **pad)
{
	// Naming a new folder.
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen || g_virtualKeyboard.m_bCancelled || wcslen(g_virtualKeyboard.GetText()) == 0)
			return S_OK;
		std::string path = m_strPath;
		if (StrRight(path, 1).compare("\\") != 0)
			path.append("\\");
		path.append(g_virtualKeyboard.GetResult());
		if (CreateDirectoryTree(path.c_str()))
			Refresh();
		return S_OK;
	}

	// Quirk: the stick only counts when pushed all the way.
	if (*action == INPUT_LTHUMB && pad != NULL && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}

	SFileItem *item;
	int slash;
	switch (*action)
	{
	case INPUT_LTRIGGER:
	case INPUT_UP:
		if (m_items.size() == 0 || m_nSelected < 1)
			return S_OK;
		m_nSelected--;
		PlaySound(m_pSoundMove);
		ScrollToSelection();
		return S_OK;

	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		if (m_items.size() == 0 || (int)m_items.size() - 1 <= m_nSelected)
			return S_OK;
		m_nSelected++;
		PlaySound(m_pSoundMove);
		ScrollToSelection();
		return S_OK;

	case INPUT_A:
		if (m_items.empty())
			return S_OK;
		item = &m_items[m_nSelected];
		if (_wcsicmp(item->label.c_str(), L"[New Folder]") == 0)
		{
			m_bKeyboardOpen = TRUE;
			g_virtualKeyboard.SetTitle(L"Create Folder");
			g_virtualKeyboard.m_nMaxLength = 0x40;
			g_virtualKeyboard.Open();
			g_virtualKeyboard.m_bPassword = FALSE;
			return S_OK;
		}
		if (!item->isParent)
		{
			if (m_bSelectFiles && !item->isFolder)
			{
				// Picks the file.
				m_bCancelled = FALSE;
				m_bOpen = FALSE;
				return S_OK;
			}
			// Into the folder (or drive).
			if (m_strPath.size() > 1 && StrRight(m_strPath, 1).compare("\\") != 0)
				m_strPath.append("\\");
			StrAppend(m_strPath, item->name);
			m_topStack[m_nDepth] = m_nTop;
			m_selectedStack[m_nDepth] = m_nSelected;
			m_nDepth++;
			m_nSelected = 0;   // (Refresh restores this depth's old one)
			Refresh();
			return S_OK;
		}
		// "..": like B, and back where it was.
		if (m_strPath.size() < 2)
			return S_OK;
		m_nDepth--;
		m_nSelected = m_selectedStack[m_nDepth];
		m_nTop = m_topStack[m_nDepth];
		if (m_strPath.size() == 3)
		{
			m_strPath.erase();   // "C:\" goes back to the drive list
		}
		else
		{
			slash = StrReverseFind(m_strPath, "\\", -1);
			if (slash < 1)
				m_strPath.erase();
			else
				m_strPath = StrLeft(m_strPath, slash);
		}
		Refresh();
		ScrollToSelection();
		return S_OK;

	case INPUT_START:
		item = m_items.empty() ? NULL : &m_items[m_nSelected];
		if (!m_bFolderResult && item != NULL && item->isFolder)
			return S_OK;
		m_bCancelled = FALSE;
		m_bOpen = FALSE;
		return S_OK;

	case INPUT_B:
		if (m_strPath.size() < 2)
			return S_OK;
		m_nDepth--;
		m_nSelected = m_selectedStack[m_nDepth];
		slash = -1;
		if (m_strPath.size() > 3)
			slash = StrReverseFind(m_strPath, "\\", -1);
		if (slash > 0)
			m_strPath = StrLeft(m_strPath, slash);
		else
			m_strPath.erase();
		Refresh();
		ScrollToSelection();
		return S_OK;

	case INPUT_BACK:
	case INPUT_BLACK:
		*action = INPUT_NONE;
		m_bCancelled = TRUE;
		m_bOpen = FALSE;
		return S_OK;
	}
	return S_OK;
}
