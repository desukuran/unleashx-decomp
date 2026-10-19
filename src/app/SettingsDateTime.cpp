// Control Panel: Date/Time Settings. Writes <Preference><Use24HFormat>,
// <AutoSetTime> and <SNTP Synchronize="" Port="" HTTP="">server</SNTP>.
#include "SettingsPages.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/StrUtil.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/Skin.h"
#include "../skin/SkinText.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"
#include "Screens.h"

// 0x000C1C00
CDateTimeSetting::~CDateTimeSetting()
{
	FreePopups();
}

// 0x000D1BBB
HRESULT CDateTimeSetting::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"Date/Time Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	SPreferences &prefs = g_pSettings->m_prefs;
	AddItem(GetString(STR_CLOCK_SETTINGS), GetSkinTextValue(TEXT_DATETIME), ITEM_CLOCK, FALSE, TRUE);
	AddItem(GetString(STR_CLOCK_24HFormat), prefs.m_bUse24HFormat ? L"Yes" : L"No", ITEM_24H_FORMAT, FALSE, TRUE);
	AddItem(GetString(STR_AUTOSET_CLOCK), prefs.m_bAutoSetTime ? L"Yes" : L"No", ITEM_AUTO_SET_CLOCK, FALSE, TRUE);

	wchar_t text[100];
	MultiByteToWideChar(CP_ACP, 0, prefs.m_strSNTPServer.c_str(), (int)prefs.m_strSNTPServer.size() + 1, text, 80);
	AddItem(L"SNTP", L"", ITEM_SNTP_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_ENABLE), prefs.m_bSNTPSync ? L"Yes" : L"No", ITEM_SNTP_ENABLE, FALSE, TRUE);
	BOOL sync = prefs.m_bSNTPSync;
	AddItem(GetString(STR_SNTP_SERVER), text, ITEM_SNTP_SERVER, FALSE, sync);
	swprintf(text, 100, L"%d", prefs.m_nSNTPPort);
	AddItem(GetString(STR_PORT), text, ITEM_SNTP_PORT, FALSE, sync);
	AddItem(GetString(STR_ALLOW_HTTP), prefs.m_bSNTPAllowHTTP ? L"Yes" : L"No", ITEM_SNTP_ALLOW_HTTP, FALSE, sync);

	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// 0x000BEECD
void CDateTimeSetting::SetItemEnabled(int, BOOL enabled)
{
	m_items[ITEM_SNTP_SERVER].m_bEnabled = enabled;
	m_items[ITEM_SNTP_PORT].m_bEnabled = enabled;
	m_items[ITEM_SNTP_ALLOW_HTTP].m_bEnabled = enabled;
}

// 0x000BF191
BOOL ValidateSNTPValue(wchar_t *text, int index, char *message)
{
	if (index != CDateTimeSetting::ITEM_SNTP_PORT)
		return TRUE;
	if (text == NULL || wcslen(text) == 0)
	{
		sprintf(message, "SNTP Port can not be empty.");
		swprintf(text, 100, L"%d", 13);
		return FALSE;
	}
	long port = _wtol(text);
	if (port == 0 || (unsigned long)port > 0xFFFF)
	{
		swprintf(text, 100, L"%d", 13);
		sprintf(message, "%d is an invalid port for SNTP Server.", (int)port);
		return FALSE;
	}
	return TRUE;
}

// 0x000D16A4
HRESULT CDateTimeSetting::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	// The pop-ups take the input while open.
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return S_OK;
		if (g_nKeyboardMode == 0)
		{
			wchar_t text[100];
			char message[100];
			wcscpy(text, g_virtualKeyboard.GetText());
			if (ValidateSNTPValue(text, m_nSelected, message))
			{
				if (m_nSelected == ITEM_SNTP_PORT)
					SetItemValue(&m_items[ITEM_SNTP_PORT], text, TRUE);
			}
			else
			{
				ShowMessage(message, L"Invalid Value", MSGBOX_BUTTONS_OK);
			}
		}
		if (g_nKeyboardOwner != 0)
			g_virtualKeyboard.Open();
		g_nKeyboardOwner = 0;
		return S_OK;
	}
	if (m_bIPEditorOpen)
	{
		m_pIPEditor->ProcessInput(action, pad);
		m_bIPEditorOpen = m_pIPEditor->m_bOpen;
		if (!m_bIPEditorOpen && !m_pIPEditor->m_bCancelled)
		{
			wchar_t address[80];
			m_pIPEditor->GetAddress(address);
			SetItemValue(&m_items[m_nSelected], address, TRUE);
		}
		return S_OK;
	}
	if (m_bClockEditorOpen && m_pClockEditor != NULL)
	{
		m_pClockEditor->ProcessInput(action, pad);
		m_bClockEditorOpen = m_pClockEditor->m_bOpen;
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
	static DWORD s_dwNextUpdate = GetTickCount() + 1000;   // 0x0027A43C
	static BOOL s_bTick;                                   // 0x00278D68, not read anywhere

	switch (*action)
	{
	case INPUT_NONE:
		// Keep the clock line current.
		if (s_dwNextUpdate < GetTickCount())
		{
			s_dwNextUpdate = GetTickCount() + 1000;
			BOOL modified = m_bModified;
			SetItemValue(&m_items[ITEM_CLOCK], GetSkinTextValue(TEXT_DATETIME), FALSE);
			m_bModified = modified;
			if (!s_bTick)
			{
				s_bTick = TRUE;
			}
			else
			{
				s_bTick = FALSE;
				m_bModified = modified;
			}
		}
		return S_OK;

	case INPUT_A:
	case INPUT_START:
		break;

	default:
		// Any other input holds the clock line for 2 seconds. (The page's own
		// B/BACK and up/down handling here is the same as the base class's,
		// which has always used those actions.)
		if (s_dwNextUpdate < GetTickCount())
			s_dwNextUpdate = GetTickCount() + 2000;
		return S_OK;
	}

	if (IsSelectedDisabled())
		return S_OK;
	CSettingItem *selected = &m_items[m_nSelected];
	switch (item.m_nType)
	{
	case ITEM_CLOCK:
		ShowClockEditor();
		break;

	case ITEM_SNTP_SERVER:
		ShowIPEditor(item.m_strValue.c_str(), item.m_strLabel.c_str());
		break;

	case ITEM_SNTP_PORT:
		OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), FALSE, 3, 1);
		break;

	case ITEM_SNTP_ENABLE:
	{
		BOOL enable = item.m_strValue != L"Yes";
		m_items[ITEM_SNTP_SERVER].m_bEnabled = enable;
		m_items[ITEM_SNTP_PORT].m_bEnabled = enable;
		m_items[ITEM_SNTP_ALLOW_HTTP].m_bEnabled = enable;
	}
		// fall through
	case ITEM_24H_FORMAT:
	case ITEM_AUTO_SET_CLOCK:
	case ITEM_SNTP_ALLOW_HTTP:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		break;

	case ITEM_SAVE:
		if (m_bModified)
			Save();
		else
			m_bClosed = TRUE;
		break;

	case ITEM_RESET:
		RevertChanges();
		SetItemEnabled(ITEM_SNTP_ENABLE, g_pSettings->m_prefs.m_bSNTPSync);
		break;

	case ITEM_EXIT:
		AskToSave();
		break;

	default:
		return S_OK;
	}
	PlaySound(m_pSoundSelect);
	return S_OK;
}

// 0x000C9B4B
BOOL CDateTimeSetting::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
	BOOL needRestart = FALSE;
	char message[100];
	if (!m_bModified)
		return TRUE;

	std::string text;
	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
	{
		sprintf(message, GetStringA(STR_MSG_SETTING_NOCONFIG), STR_MSG_SETTING_NOCONFIG);
		ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return FALSE;
	}

	SPreferences &prefs = g_pSettings->m_prefs;
	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
	{
		if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
			continue;
		const wchar_t *value = it->m_strValue.c_str();
		const char *name;
		switch (it->m_nType)
		{
		case ITEM_24H_FORMAT:
			name = "Use24HFormat";
			break;
		case ITEM_AUTO_SET_CLOCK:
			name = "AutoSetTime";
			break;
		case ITEM_SNTP_ENABLE:
		case ITEM_SNTP_SERVER:
		case ITEM_SNTP_PORT:
		case ITEM_SNTP_ALLOW_HTTP:
			name = "SNTP";
			break;
		default:
			continue;
		}

		// <Settings><Preference><name>, added if missing. Quirk: if it can't
		// be added the line is skipped silently, except for the port (with
		// a message about the FTP port).
		CXmlElement *preference = g_config.FindElement(settings, "Preference");
		CXmlElement *element = NULL;
		if (GetOrAddChildElement("Preference", settings, &preference))
		{
			element = preference->FindFirstChild(name);
			if (!GetOrAddChildElement(name, preference, &element))
				element = NULL;
		}
		if (element == NULL)
		{
			if (it->m_nType == ITEM_SNTP_PORT)
			{
				sprintf(message, "Unable to set FTP Port: %d", GetLastError());
				ok = FALSE;
			}
			continue;
		}

		switch (it->m_nType)
		{
		case ITEM_24H_FORMAT:
			prefs.m_bUse24HFormat = !IsNoW(value);
			element->SetValue(prefs.m_bUse24HFormat ? "Yes" : "No");
			changed = TRUE;
			break;

		case ITEM_AUTO_SET_CLOCK:
			prefs.m_bAutoSetTime = !IsNoW(value);
			element->SetValue(prefs.m_bAutoSetTime ? "Yes" : "No");
			changed = TRUE;
			break;

		case ITEM_SNTP_ENABLE:
			prefs.m_bSNTPSync = !IsNoW(value);
			element->SetAttribute("Synchronize", prefs.m_bSNTPSync ? "Yes" : "No");
			changed = TRUE;
			needRestart = TRUE;
			break;

		case ITEM_SNTP_SERVER:
			text = StrFromWide(value);
			element->SetValue(text.c_str());
			changed = TRUE;
			needRestart = TRUE;
			prefs.m_strSNTPServer = text;
			break;

		case ITEM_SNTP_PORT:
			text = StrFromWide(value);
			element->SetAttribute("Port", text.c_str());
			changed = TRUE;
			needRestart = TRUE;
			prefs.m_nSNTPPort = _wtol(value);
			break;

		case ITEM_SNTP_ALLOW_HTTP:
			prefs.m_bSNTPAllowHTTP = !IsNoW(value);
			element->SetAttribute("HTTP", prefs.m_bSNTPAllowHTTP ? "Yes" : "No");
			changed = TRUE;
			needRestart = TRUE;
			break;
		}
	}
	return FinishSave(ok, message, changed, needRestart);
}
