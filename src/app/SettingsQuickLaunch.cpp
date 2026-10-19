// Control Panel: AutoRun Settings (class CQuickLaunch). One XBE per gamepad
// button and remote control digit, stored as <Settings><Shortcut><GP_A>...
#include "SettingsPages.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"
#include "Screens.h"

// The <Shortcut> child for each line, from ITEM_GP_A.
static const char *const s_shortcutNames[] = {
	"GP_A", "GP_B", "GP_X", "GP_Y", "GP_START", "GP_WHITE", "GP_BLACK", NULL,
	"IR_0", "IR_1", "IR_2", "IR_3", "IR_4", "IR_5", "IR_6", "IR_7", "IR_8", "IR_9",
};

static BOOL IsShortcutItem(int type)
{
	return (type >= CQuickLaunch::ITEM_GP_A && type <= CQuickLaunch::ITEM_GP_BLACK) ||
	       (type >= CQuickLaunch::ITEM_IR_0 && type <= CQuickLaunch::ITEM_IR_9);
}

// 0x000C1D0B
CQuickLaunch::CQuickLaunch()
{
	m_bFileBrowserOpen = FALSE;
}

// 0x000C1CF7
CQuickLaunch::~CQuickLaunch()
{
	FreePopups();
}

// 0x000C45AB
HRESULT CQuickLaunch::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"AutoRun Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	static const wchar_t *const labels[] = {
		L" A", L" B", L" X", L" Y", L" Start", L" White", L" Black", NULL,
		L" 0", L" 1", L" 2", L" 3", L" 4", L" 5", L" 6", L" 7", L" 8", L" 9",
	};
	AddItem(GetString(STR_GAMEPAD), L"", ITEM_GAMEPAD_HEADER, TRUE, TRUE);
	for (int type = ITEM_GP_A; type <= ITEM_GP_BLACK; type++)
		AddItem(labels[type - ITEM_GP_A], L"", type, FALSE, TRUE);
	AddItem(GetString(STR_INFRARED_RCONTROL), L"", ITEM_IR_HEADER, TRUE, TRUE);
	for (int type = ITEM_IR_0; type <= ITEM_IR_9; type++)
		AddItem(labels[type - ITEM_GP_A], L"", type, FALSE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);

	// Fill in the paths from <Settings><Shortcut> (at most 127 children).
	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
		return S_OK;
	CXmlElement *shortcuts = g_config.FindElement(settings, "Shortcut");
	if (shortcuts == NULL)
		return S_OK;
	std::string path;
	int count = 0;
	for (CXmlElement *child = shortcuts->GetFirstChild(); child != NULL && count < 127;
	     child = shortcuts->GetNextChild())
	{
		count++;
		path = child->m_strValue;
		if (path.size() == 0)
			continue;
		wchar_t text[260];
		MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size(), text, 260);
		for (int type = ITEM_GP_A; type <= ITEM_IR_9; type++)
		{
			const char *name = s_shortcutNames[type - ITEM_GP_A];
			if (name != NULL && StrCmpNoCase(child->m_strName, name) == 0)
			{
				// Quirk: the text isn't terminated by the conversion (the
				// length passed excludes the '\0').
				SetItemValue(&m_items[type], text, FALSE);
				break;
			}
		}
	}
	return S_OK;
}

// 0x000D5734
HRESULT CQuickLaunch::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	if (m_bFileBrowserOpen)
	{
		m_pFileBrowser->ProcessInput(action, pad);
		if (m_pFileBrowser->IsOpen())
			return S_OK;
		m_bFileBrowserOpen = FALSE;
		if (!m_pFileBrowser->m_bCancelled)
		{
			std::string path = m_pFileBrowser->GetSelectedPath();
			if (path.size() != 0)
			{
				wchar_t text[260];
				MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, text, 260);
				SetItemValue(&m_items[m_nSelected], text, TRUE);
			}
		}
		return S_OK;
	}

	if (FAILED(CSettingsBase::HandleInput(action, pad)))
		return S_OK;
	if (m_bModified && m_bAskSave && Save())
	{
		m_bClosed = TRUE;
		m_bAskSave = FALSE;
		return S_OK;
	}

	CSettingItem item = m_items[m_nSelected];
	std::string path;
	static DWORD s_dwTimer = GetTickCount() + 1000;   // 0x0027A5A8, not used for anything

	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		if (IsSelectedDisabled())
			return S_OK;
		if (IsShortcutItem(item.m_nType))
		{
			path = StrFromWide(item.m_strValue.c_str());
			if (path.size() != 0)
				path = GetPathDirectory(path.c_str());
			ShowFileBrowser(L"Browse XBE", 0, TRUE, 0, path.c_str(), "xbe");
		}
		else if (item.m_nType == ITEM_SAVE)
		{
			if (m_bModified)
				Save();
			else
				m_bClosed = TRUE;
		}
		else if (item.m_nType == ITEM_RESET)
		{
			RevertChanges();
		}
		else if (item.m_nType == ITEM_EXIT)
		{
			AskToSave();
		}
		PlaySound(m_pSoundSelect);
		break;

	case INPUT_X:
		// Clears the line.
		if (IsSelectedDisabled())
			return S_OK;
		if (IsShortcutItem(item.m_nType))
			SetItemValue(&m_items[m_nSelected], L"", TRUE);
		break;

	default:
		// (B/BACK and up/down are handled here too, the same as in the base
		// class, which has always used them.)
		if (s_dwTimer < GetTickCount())
			s_dwTimer = GetTickCount() + 2000;
		break;
	}
	return S_OK;
}

// 0x000CA63B
BOOL CQuickLaunch::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
	char message[100];
	if (!m_bModified)
		return TRUE;

	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	CXmlElement *shortcuts = NULL;
	if (settings != NULL)
	{
		shortcuts = settings->FindFirstChild("Shortcut");
		GetOrAddChildElement("Shortcut", settings, &shortcuts);
	}
	if (shortcuts == NULL)
	{
		sprintf(message, GetStringA(STR_MSG_SETTING_NOCONFIG), STR_MSG_SETTING_NOCONFIG);
		ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return FALSE;
	}

	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
	{
		if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
			continue;
		std::string text;
		if (IsShortcutItem(it->m_nType))
			text = s_shortcutNames[it->m_nType - ITEM_GP_A];
		if (text.size() == 0)
			continue;

		CXmlElement *element = shortcuts->FindFirstChild(text.c_str());
		if (!GetOrAddChildElement(text.c_str(), shortcuts, &element))
		{
			sprintf(message, "Unable to set QuickLaunch item. Error Code: %d", GetLastError());
			ok = FALSE;
			continue;
		}
		// An empty path removes the element.
		text = StrFromWide(it->m_strValue.c_str());
		if (text.size() != 0)
			element->SetValue(text.c_str());
		else
			shortcuts->RemoveChild(element);
		changed = TRUE;
		m_items[it->m_nType].m_bChanged = FALSE;   // (indexed by type, which is the index)
	}
	return FinishSave(ok, message, changed, changed);
}
