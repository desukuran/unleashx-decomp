// Control Panel: AutoLaunch Settings. Writes <Preference><Games>, <DVD>,
// <AudioCD> and <Data> (AutoLaunch="Yes|No", the player XBE as the value).
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

// 0x000C1D32
CAutoLaunch::CAutoLaunch()
{
	m_bFileBrowserOpen = FALSE;
}

// 0x000C1D1E
CAutoLaunch::~CAutoLaunch()
{
	FreePopups();
}

// Adds a line showing a narrow path setting.
static void AddPathItem(CSettingsBase *page, const wchar_t *label, const std::string &path, int type)
{
	wchar_t text[100];
	MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, text, 80);
	page->AddItem(label, text, type, FALSE, TRUE);
}

// 0x000C4EC9
HRESULT CAutoLaunch::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"AutoLaunch Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	SPreferences &prefs = g_pSettings->m_prefs;
	AddItem(GetString(STR_AUTOLAUNCH_GAME), prefs.m_bGamesAutoLaunch ? L"Yes" : L"No", ITEM_GAMES, FALSE, TRUE);
	AddItem(GetString(STR_AUTOLAUNCH_DVD), prefs.m_bDVDAutoLaunch ? L"Yes" : L"No", ITEM_DVD, FALSE, TRUE);
	AddPathItem(this, GetString(STR_DVD_PLAYER), prefs.m_strDVDPlayer, ITEM_DVD_PLAYER);
	AddItem(GetString(STR_AUTOLAUNCH_AUDIO), prefs.m_bAudioCDAutoLaunch ? L"Yes" : L"No", ITEM_AUDIO_CD, FALSE, TRUE);
	AddPathItem(this, GetString(STR_CDAUDIO_PLAYER), prefs.m_strAudioCDPlayer, ITEM_AUDIO_CD_PLAYER);
	AddItem(GetString(STR_AUTOLAUNCH_DATA), prefs.m_bDataAutoLaunch ? L"Yes" : L"No", ITEM_DATA, FALSE, TRUE);
	AddPathItem(this, GetString(STR_FILE_MANAGER), prefs.m_strDataLauncher, ITEM_DATA_LAUNCHER);

	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// 0x000D5AFF
HRESULT CAutoLaunch::HandleInput(int *action, SGamepadState **pad)
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
	static DWORD s_dwTimer = GetTickCount() + 1000;   // 0x0027A5B0, not used for anything

	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		if (IsSelectedDisabled())
			return S_OK;
		switch (item.m_nType)
		{
		case ITEM_GAMES:
		case ITEM_DVD:
		case ITEM_AUDIO_CD:
		case ITEM_DATA:
			SetItemValue(&m_items[m_nSelected], item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
			break;

		case ITEM_DVD_PLAYER:
		case ITEM_AUDIO_CD_PLAYER:
		case ITEM_DATA_LAUNCHER:
			// Browse from the folder of the current XBE.
			path = StrFromWide(item.m_strValue.c_str());
			if (path.size() != 0)
				path = GetPathDirectory(path.c_str());
			ShowFileBrowser(L"Browse XBE", 0, TRUE, 0, path.c_str(), "xbe");
			break;

		case ITEM_SAVE:
			if (m_bModified)
				Save();
			else
				m_bClosed = TRUE;
			break;

		case ITEM_RESET:
			RevertChanges();
			break;

		case ITEM_EXIT:
			AskToSave();
			break;
		}
		PlaySound(m_pSoundSelect);
		break;

	case INPUT_X:
		// Clears a player path.
		if (IsSelectedDisabled())
			return S_OK;
		if (item.m_nType == ITEM_DVD_PLAYER || item.m_nType == ITEM_AUDIO_CD_PLAYER ||
		    item.m_nType == ITEM_DATA_LAUNCHER)
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

// 0x000CAA5D
BOOL CAutoLaunch::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
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
		const char *name;
		const char *error;
		BOOL *flag = NULL;
		std::string *player = NULL;
		switch (it->m_nType)
		{
		case ITEM_GAMES:
			name = "Games";
			flag = &prefs.m_bGamesAutoLaunch;
			error = "Unable to set AutoLaunch for Games. Error Code: %d";
			break;
		case ITEM_DVD:
			name = "DVD";
			flag = &prefs.m_bDVDAutoLaunch;
			error = "Unable to set AutoLaunch for DVD. Error Code: %d";
			break;
		case ITEM_AUDIO_CD:
			name = "AudioCD";
			flag = &prefs.m_bAudioCDAutoLaunch;
			error = "Unable to set AutoLaunch for AudioCD. Error Code: %d";
			break;
		case ITEM_DATA:
			name = "Data";
			flag = &prefs.m_bDataAutoLaunch;
			error = "Unable to set AutoLaunch for Data. Error Code: %d";
			break;
		case ITEM_DVD_PLAYER:
			name = "DVD";
			player = &prefs.m_strDVDPlayer;
			error = "Unable to set '%s' as the default DVD Player.";
			break;
		case ITEM_AUDIO_CD_PLAYER:
			name = "AudioCD";
			player = &prefs.m_strAudioCDPlayer;
			error = "Unable to set '%s' as the default CD Player.";
			break;
		case ITEM_DATA_LAUNCHER:
			name = "Data";
			player = &prefs.m_strDataLauncher;
			error = "Unable to set '%s' as the default File Manager.";
			break;
		default:
			continue;
		}

		CXmlElement *preference = g_config.FindElement(settings, "Preference");
		CXmlElement *element = NULL;
		if (GetOrAddChildElement("Preference", settings, &preference))
		{
			element = preference->FindFirstChild(name);
			if (!GetOrAddChildElement(name, preference, &element))
				element = NULL;
		}

		if (flag != NULL)
		{
			if (element == NULL)
			{
				sprintf(message, error, GetLastError());
				ok = FALSE;
				continue;
			}
			*flag = !IsNoW(it->m_strValue.c_str());
			text = *flag ? "Yes" : "No";
			element->SetAttribute("AutoLaunch", text.c_str());
			if (it->m_nType == ITEM_DVD && element->m_strValue.size() == 0)
				element->SetValue("C:\\xboxdash.xbe");   // the DVD player needs an XBE
			if (it->m_nType != ITEM_DATA)
				m_items[it->m_nType].m_bChanged = FALSE;   // (indexed by type, which is the index)
			changed = TRUE;
		}
		else
		{
			// The player must exist (an empty path is allowed). Quirk: when
			// the element can't be made, the message shows the previous
			// line's text.
			if (element != NULL)
			{
				text = StrFromWide(it->m_strValue.c_str());
				if (text.size() == 0 || FileExists(text.c_str()))
				{
					element->SetValue(text.c_str());
					*player = text;
					changed = TRUE;
					continue;
				}
			}
			sprintf(message, error, text.c_str());
			ok = FALSE;
		}
	}
	return FinishSave(ok, message, changed, FALSE);
}
