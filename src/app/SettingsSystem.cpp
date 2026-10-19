// Control Panel: System Settings. Writes mostly <Preference> (plus
// <Settings><LED> and <Settings><Newsfeed>); the nickname goes to the Xbox
// and the font scale to <data>\videoset.dat.
#include "SettingsPages.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/Skin.h"
#include "../skin/SkinText.h"
#include "../system/SystemControl.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../xml/XmlUtil.h"
#include "Screens.h"

// The page's own copy of the LedColor names (the config loader has another
// at 0x0026A4B0). 0x0026A6A0
static const wchar_t s_ledColorNames[LED_COLOR_COUNT][16] = {
	L"Default", L"Red", L"Orange", L"Off", L"Green", L"Flashing Red", L"Flashing Orange",
	L"Flashing Green", L"Green-Orange", L"Orange-Red", L"Red-Green",
};

// The LED lines: label, <LED> child and setting.
struct SLedLine
{
	const wchar_t *label;
	const char *element;
	int SPreferences::*color;
};

static const SLedLine s_ledLines[] = {
	{ L"Default", "Default", &SPreferences::m_nLEDDefault },
	{ L"Screen Saver", "ScreenSaver", &SPreferences::m_nLEDScreenSaver },
	{ L"Copying", "FileCopy", &SPreferences::m_nLEDFileCopy },
	{ L"FTP Send", "FTPSend", &SPreferences::m_nLEDFTPSend },
	{ L"FTP Receive", "FTPReceive", &SPreferences::m_nLEDFTPReceive },
	{ L"FTP Boost", "FTPBoost", &SPreferences::m_nLEDFTPBoost },
	{ L"Busy", "Busy", &SPreferences::m_nLEDBusy },
};

// 0x000C1AE0
CSystemSetting::CSystemSetting()
{
	m_pXboxInfo = NULL;
	m_bXboxInfoOpen = FALSE;
}

// 0x000C1ACC
CSystemSetting::~CSystemSetting()
{
	FreePopups();
	// (m_pXboxInfo isn't freed.)
}

// 0x000CCC88
HRESULT CSystemSetting::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"System Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	CAppSettings *settings = g_pSettings;
	SPreferences &prefs = settings->m_prefs;
	wchar_t text[160];

	AddItem(GetString(STR_XBOX_INFO), L"", ITEM_XBOX_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_DASH_VERSION), GetSkinTextValue(TEXT_APPVERSIONBUILD), ITEM_DASH_VERSION, FALSE, FALSE);
	AddItem(GetString(STR_XBOX_INFO), L"(More Information)", ITEM_XBOX_INFO, FALSE, TRUE);
	AddItem(GetString(STR_XBOX_NAME), GetSkinTextValue(TEXT_NICKNAME), ITEM_NICKNAME, FALSE, TRUE);

	AddItem(GetString(STR_FILE_OPERATION), L"", ITEM_FILE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_AUTO_RENAME), prefs.m_bAutoRename ? L"Yes" : L"No", ITEM_AUTO_RENAME, FALSE, TRUE);
	AddItem(GetString(STR_AUTO_PATCH_XBE), prefs.m_bAutoPatch ? L"Yes" : L"No", ITEM_AUTO_PATCH, FALSE, TRUE);
	AddItem(GetString(STR_ABORT_ON_ERROR), prefs.m_bAbortOnError ? L"Yes" : L"No", ITEM_ABORT_ON_ERROR, FALSE, TRUE);
	// Quirk: converted without the '\0'.
	MultiByteToWideChar(CP_ACP, 0, prefs.m_strDefaultFolder.c_str(), (int)prefs.m_strDefaultFolder.size(), text, 100);
	AddItem(GetString(STR_DEF_BACKUP_FOLDER), text, ITEM_DEFAULT_FOLDER, FALSE, TRUE);

	AddItem(GetString(STR_CONTEXT_MENU), L"", ITEM_CONTEXT_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_ALLOW_UNINSTALL), prefs.m_bAllowUninstall ? L"Yes" : L"No", ITEM_ALLOW_UNINSTALL, FALSE, TRUE);
	AddItem(GetString(STR_ALLOW_MENU_EDIT), prefs.m_bAllowItemEdit ? L"Yes" : L"No", ITEM_ALLOW_ITEM_EDIT, FALSE, TRUE);
	AddItem(GetString(STR_ALLOW_RECENT_LIST), prefs.m_bAllowRecentList ? L"Yes" : L"No", ITEM_ALLOW_RECENT_LIST, FALSE,
	        TRUE);
	AddItem(GetString(STR_OPEN_GAME_FOLDER), prefs.m_bLinkToGame ? L"Yes" : L"No", ITEM_LINK_TO_GAME, FALSE, TRUE);
	AddItem(GetString(STR_OPEN_SAVES_FOLDER), prefs.m_bLinkToSave ? L"Yes" : L"No", ITEM_LINK_TO_SAVE, FALSE, TRUE);

	AddItem(GetString(STR_FONT), L"", ITEM_FONT_HEADER, TRUE, TRUE);
	float scale = settings->m_fFontScale > 1.0f ? settings->m_fFontScale : 1.0f;
	swprintf(text, 160, L"%0.01fx", (double)scale);
	AddItem(GetString(STR_FONT_SCALE), text, ITEM_FONT_SCALE, FALSE, TRUE);
	// (Quirk: anything below 11 shows as 10.)
	swprintf(text, 160, L"%u", (unsigned)settings->m_nScrollSpeed < 11 ? 10u : (unsigned)settings->m_nScrollSpeed);
	AddItem(GetString(STR_SCROLL_SPEED), text, ITEM_SCROLL_SPEED, FALSE, TRUE);

	AddItem(GetString(STR_TEMPERATURE), L"", ITEM_TEMPERATURE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_MB_TEMP), GetSkinTextValue(TEXT_MBTEMP), ITEM_MB_TEMP, FALSE, FALSE);
	AddItem(GetString(STR_CPU_TEMP), GetSkinTextValue(TEXT_CPUTEMP), ITEM_CPU_TEMP, FALSE, FALSE);
	AddItem(GetString(STR_USE_FAHRENHEIT), prefs.m_bUseFahrenheit ? L"Yes" : L"No", ITEM_USE_FAHRENHEIT, FALSE, TRUE);
	swprintf(text, 160, L"%d C", (unsigned)prefs.m_nTempThreshold > 71 ? 72 : prefs.m_nTempThreshold);
	AddItem(GetString(STR_TEMP_THRESHOLD), text, ITEM_TEMP_THRESHOLD, FALSE, TRUE);
	float fan = (float)prefs.m_nFanSpeed * 0.1f;
	if (fan <= 1.0f)
		fan = 1.0f;
	swprintf(text, 160, L"%.01fx", (double)fan);
	AddItem(GetString(STR_FAN_SPEED), text, ITEM_FAN_SPEED, FALSE, TRUE);

	AddItem(GetString(STR_MISC), L"", ITEM_MISC_HEADER, TRUE, TRUE);
	StrUtf8ToWide(prefs.m_strLanguageFile.c_str(), text, 100);
	AddItem(GetString(STR_LANGUAGE_FILE), text, ITEM_LANGUAGE_FILE, FALSE, TRUE);
	swprintf(text, 160, L"%d", prefs.m_nAutoTurnOff);
	AddItem(GetString(STR_AUTO_TURN_OFF), text, ITEM_AUTO_TURN_OFF, FALSE, TRUE);

	AddItem(GetString(STR_LED_COLOR), L"", ITEM_LED_HEADER, TRUE, TRUE);
	for (int i = 0; i < 7; i++)
	{
		int color = prefs.*s_ledLines[i].color;
		// Quirk: % 10, so Red-Green (10) shows as Default.
		swprintf(text, 160, L"%s", s_ledColorNames[(unsigned)color % 10]);
		AddItem(s_ledLines[i].label, text, ITEM_LED_DEFAULT + i, FALSE, TRUE, TRUE, color);
	}
	AddItem(L"Test LED", s_ledColorNames[0], ITEM_LED_TEST, FALSE, TRUE);

	AddItem(GetString(STR_SCREEN_SAVER), L"", ITEM_SCREEN_SAVER_HEADER, TRUE, TRUE);
	swprintf(text, 160, L"%d", prefs.m_nScreenSaverWait);
	AddItem(GetString(STR_SS_WAIT), text, ITEM_SS_WAIT, FALSE, TRUE);
	const char *type = prefs.m_nScreenSaverType == 2 ? "Video" : prefs.m_nScreenSaverType == 3 ? "Slideshow" : "Text";
	StrUtf8ToWide(type, text, 80);
	AddItem(GetString(STR_TYPE), text, ITEM_SS_TYPE, FALSE, TRUE, TRUE, prefs.m_nScreenSaverType);
	StrUtf8ToWide(prefs.m_strScreenSaverText.c_str(), text, 100);
	AddItem(GetString(STR_SS_TEXT), text, ITEM_SS_TEXT, FALSE, TRUE);
	StrUtf8ToWide(prefs.m_strScreenSaverVideo.c_str(), text, 100);
	AddItem(GetString(STR_SS_VIDEO), text, ITEM_SS_VIDEO, FALSE, TRUE);
	StrUtf8ToWide(prefs.m_strScreenSaverPath.c_str(), text, 100);
	AddItem(GetString(STR_SS_PATH), text, ITEM_SS_PATH, FALSE, TRUE);
	AddItem(GetString(STR_ENABLE_BGM), prefs.m_bScreenSaverBGM ? L"Yes" : L"No", ITEM_SS_BGM, FALSE, TRUE);

	AddItem(GetString(STR_SKIN), L"", ITEM_SKIN_HEADER, TRUE, TRUE);
	// Relative to UnleashX's folder.
	std::string path = prefs.m_strSkinPath;
	StrReplaceAll(path, settings->m_strHomePath.c_str(), "");
	MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, text, 80);
	AddItem(GetString(STR_SKINS_PATH), text, ITEM_SKINS_PATH, FALSE, TRUE);
	memset(text, 0, 100 * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, g_skin.m_strSkinName.c_str(), (int)g_skin.m_strSkinName.size() + 1, text, 100);
	AddItem(GetString(STR_CURRENT_SKIN), text, ITEM_CURRENT_SKIN, FALSE, FALSE);

	AddItem(GetString(STR_RSS_NEWSFEED), L"", ITEM_NEWSFEED_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_ENABLE), settings->m_bNewsfeedEnable ? L"Yes" : L"No", ITEM_NEWSFEED_ENABLE, FALSE, TRUE);
	swprintf(text, 160, L"%d", settings->m_nNewsfeedInterval);
	AddItem(GetString(STR_INTERVAL), text, ITEM_NEWSFEED_INTERVAL, FALSE, settings->m_bNewsfeedEnable);
	AddItem(GetString(STR_CONTINUOUS), settings->m_bNewsfeedContinuous ? L"Yes" : L"No", ITEM_NEWSFEED_CONTINUOUS,
	        FALSE, settings->m_bNewsfeedEnable);
	AddItem(GetString(STR_SEPARATOR), settings->m_szNewsfeedSeparator, ITEM_NEWSFEED_SEPARATOR, FALSE,
	        settings->m_bNewsfeedEnable);

	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// 0x000C66E1
HRESULT CSystemSetting::Render()
{
	if (m_bXboxInfoOpen)
	{
		m_pXboxInfo->Render();
		return S_OK;
	}
	return CSettingsBase::Render();
}

// 0x000D3E7A
void CSystemSetting::ShowXboxInfo()
{
	if (m_pXboxInfo == NULL)
	{
		m_pXboxInfo = new CXboxInfo;
		if (m_pXboxInfo != NULL && FAILED(m_pXboxInfo->Init(NULL)))
		{
			delete m_pXboxInfo;
			m_pXboxInfo = NULL;
		}
	}
	if (m_pXboxInfo != NULL)
	{
		m_pXboxInfo->Show();
		m_bXboxInfoOpen = TRUE;
	}
}

// 0x000C14A7
BOOL ValidateSystemValue(int index, wchar_t *text, char *message)
{
	switch (index)
	{
	case CSystemSetting::ITEM_NICKNAME:
		if (text != NULL && wcslen(text) > 32)
		{
			sprintf(message, "Xbox Nickname is limited to %d characters.", 31);
			return FALSE;
		}
		return TRUE;

	case CSystemSetting::ITEM_SCROLL_SPEED:
	{
		// Quirk: only an empty entry is checked (and always fails); any
		// number typed is accepted.
		if (text == NULL || wcslen(text) != 0)
			return TRUE;
		unsigned speed = (unsigned)_wtol(text);
		if (speed >= 10 && speed <= 61)
			return TRUE;
		swprintf(text, 100, L"%d", 20);
		sprintf(message, "%d is an invalid value for Scroll Speed.", speed);
		return FALSE;
	}

	case CSystemSetting::ITEM_TEMP_THRESHOLD:
	{
		// Quirk: as above, and an empty entry reads as 0, which is valid.
		if (text == NULL || wcslen(text) != 0)
			return TRUE;
		unsigned threshold = (unsigned)_wtol(text);
		if (threshold <= 99)
			return TRUE;
		swprintf(text, 100, L"%d", threshold);
		sprintf(message, "%d is an invalid value for Temperature Threshold.", threshold);
		return FALSE;
	}

	case CSystemSetting::ITEM_SKINS_PATH:
	{
		// (The skins path is chosen with the file pop-up, so this isn't used.)
		if (wcslen(text) == 0)
			return TRUE;
		std::string path = StrFromWide(text);
		std::string resolved;
		ResolvePath(path.c_str(), resolved);
		if (DirectoryExists(resolved.c_str()))
			return TRUE;
		sprintf(message, "'%s' does not exist.", StrFromWide(text).c_str());
		return FALSE;
	}
	}
	return TRUE;
}

// Path lines open the file pop-up at the current folder.
static std::string GetStartPath(const CSettingItem &item)
{
	std::string path = StrFromWide(item.m_strValue.c_str());
	if (path.size() != 0)
	{
		std::string resolved;
		ResolvePath(path.c_str(), resolved);
		path = resolved;
	}
	return path;
}

// 0x000D3EE3
HRESULT CSystemSetting::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	if (m_bFileBrowserOpen)
	{
		m_pFileBrowser->ProcessInput(action, pad);
		if (m_pFileBrowser->IsOpen())
			return S_OK;
		m_bFileBrowserOpen = FALSE;
		if (m_pFileBrowser->m_bCancelled)
			return S_OK;
		std::string path = m_pFileBrowser->GetSelectedPath();
		if (path.size() != 0)
		{
			wchar_t text[260];
			MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, text, 260);
			SetItemValue(&m_items[m_nSelected], text, TRUE);
		}
		return S_OK;
	}
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return S_OK;
		if (g_virtualKeyboard.m_bCancelled == 0)
		{
			wchar_t text[260];
			char message[260];
			wcscpy(text, g_virtualKeyboard.GetText());
			if (!ValidateSystemValue(m_nSelected, text, message))
			{
				ShowMessage(message, L"Invalid Value", MSGBOX_BUTTONS_OK);
			}
			else if (m_nSelected == ITEM_TEMP_THRESHOLD)
			{
				swprintf(text, 260, L"%d C", _wtol(g_virtualKeyboard.GetText()));
				SetItemValue(&m_items[m_nSelected], text, TRUE);
			}
			else
			{
				SetItemValue(&m_items[m_nSelected], g_virtualKeyboard.GetText(), TRUE);
			}
		}
		if (g_virtualKeyboard.m_bNumeric != 0)
			g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bNumeric = 0;
		return S_OK;
	}
	if (m_bXboxInfoOpen)
	{
		m_pXboxInfo->ProcessInput(pad, action);
		m_bXboxInfoOpen = !m_pXboxInfo->m_bClosed;
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
	CSettingItem *selected = &m_items[m_nSelected];
	static DWORD s_dwNextUpdate = GetTickCount() + 1000;   // 0x0027A598
	wchar_t text[100];
	const wchar_t *label = item.m_strLabel.c_str();
	const wchar_t *value = item.m_strValue.c_str();
	std::string path;
	int type = item.m_nType;

	switch (*action)
	{
	case INPUT_NONE:
		// Keep the temperatures current.
		if (s_dwNextUpdate < GetTickCount())
		{
			s_dwNextUpdate = GetTickCount() + 1000;
			BOOL modified = m_bModified;
			SetItemValue(&m_items[ITEM_MB_TEMP], GetSkinTextValue(TEXT_MBTEMP), FALSE);
			SetItemValue(&m_items[ITEM_CPU_TEMP], GetSkinTextValue(TEXT_CPUTEMP), FALSE);
			m_bModified = modified;
		}
		return S_OK;

	case INPUT_A:
	case INPUT_START:
		break;

	case INPUT_X:
		// Back to the default value.
		if (IsSelectedDisabled())
			return S_OK;
		switch (type)
		{
		case ITEM_AUTO_RENAME:
		case ITEM_ALLOW_UNINSTALL:
		case ITEM_ALLOW_ITEM_EDIT:
		case ITEM_LINK_TO_SAVE:
		case ITEM_USE_FAHRENHEIT:
		case ITEM_NEWSFEED_ENABLE:
			SetItemValue(selected, L"No", TRUE);
			if (type == ITEM_NEWSFEED_ENABLE)
			{
				m_items[ITEM_NEWSFEED_INTERVAL].m_bEnabled = FALSE;
				m_items[ITEM_NEWSFEED_CONTINUOUS].m_bEnabled = FALSE;
				m_items[ITEM_NEWSFEED_SEPARATOR].m_bEnabled = FALSE;
			}
			break;
		case ITEM_AUTO_PATCH:
		case ITEM_ABORT_ON_ERROR:
		case ITEM_ALLOW_RECENT_LIST:
		case ITEM_LINK_TO_GAME:
		case ITEM_SS_BGM:
		case ITEM_NEWSFEED_CONTINUOUS:
			SetItemValue(selected, L"Yes", TRUE);
			break;
		case ITEM_DEFAULT_FOLDER:
		case ITEM_LANGUAGE_FILE:
		case ITEM_SS_TEXT:
		case ITEM_SS_VIDEO:
		case ITEM_SS_PATH:
			SetItemValue(selected, L"", TRUE);
			break;
		case ITEM_FONT_SCALE:
		case ITEM_FAN_SPEED:
			SetItemValue(selected, L"1x", TRUE);
			break;
		case ITEM_SCROLL_SPEED:
			SetItemValue(selected, L"20", TRUE);
			break;
		case ITEM_SS_TYPE:
			SetItemValue(selected, L"Text", TRUE);
			break;
		case ITEM_NEWSFEED_SEPARATOR:
			SetItemValue(selected, L" | ", TRUE);
			break;
		case ITEM_LED_TEST:
			SetItemValue(selected, s_ledColorNames[0], FALSE);
			selected->m_nValue = 0;
			SetLedPattern(LED_DEFAULT, TRUE);
			break;
		default:
			if (type >= ITEM_LED_DEFAULT && type <= ITEM_LED_BUSY)
			{
				SetItemValue(selected, s_ledColorNames[0], TRUE);
				selected->m_nValue = 0;
			}
			break;
		}
		return S_OK;

	case INPUT_LEFT:
	case INPUT_RIGHT:
		// The font scale in steps of 0.1.
		if (type != ITEM_FONT_SCALE)
			return S_OK;
		if (item.m_strValue.size() == 0)
		{
			SetItemValue(selected, L"1x", TRUE);
			return S_OK;
		}
		{
			float scale;
			if (*action == INPUT_LEFT)
			{
				scale = (float)(_wtof(value) - 0.1f);
				if (scale < 1.0f)
					scale = 1.0f;
			}
			else
			{
				scale = (float)(_wtof(value) + 0.1f);
				if (scale > 3.0f)
					scale = 3.0f;
			}
			swprintf(text, 100, L"%.02fx", (double)scale);
			SetItemValue(selected, text, TRUE);
		}
		return S_OK;

	default:
		// (B/BACK and up/down are handled here too, the same as in the base
		// class, which has always used them.)
		if (s_dwNextUpdate < GetTickCount())
			s_dwNextUpdate = GetTickCount() + 2000;
		return S_OK;
	}

	if (IsSelectedDisabled())
		return S_OK;
	switch (type)
	{
	case ITEM_XBOX_INFO:
		ShowXboxInfo();
		break;

	case ITEM_NICKNAME:
		OpenKeyboard(label, value, FALSE, 31, 0);
		break;

	case ITEM_AUTO_RENAME:
	case ITEM_AUTO_PATCH:
	case ITEM_ABORT_ON_ERROR:
	case ITEM_ALLOW_UNINSTALL:
	case ITEM_ALLOW_ITEM_EDIT:
	case ITEM_ALLOW_RECENT_LIST:
	case ITEM_LINK_TO_GAME:
	case ITEM_LINK_TO_SAVE:
	case ITEM_USE_FAHRENHEIT:
	case ITEM_SS_BGM:
	case ITEM_NEWSFEED_ENABLE:
	case ITEM_NEWSFEED_CONTINUOUS:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		if (type == ITEM_NEWSFEED_ENABLE)
		{
			BOOL enable = item.m_strValue == L"No";
			m_items[ITEM_NEWSFEED_INTERVAL].m_bEnabled = enable;
			m_items[ITEM_NEWSFEED_CONTINUOUS].m_bEnabled = enable;
			m_items[ITEM_NEWSFEED_SEPARATOR].m_bEnabled = enable;
		}
		break;

	case ITEM_FONT_SCALE:
		// A steps up by 0.1, wrapping to 1.00x above 3.00x.
		if (item.m_strValue.size() == 0)
		{
			SetItemValue(selected, L"1x", TRUE);
		}
		else
		{
			float scale = (float)(_wtof(value) + 0.1f);
			if (scale < 1.0f || scale > 3.0f)
				scale = 1.0f;
			swprintf(text, 100, L"%.02fx", (double)scale);
			SetItemValue(selected, text, TRUE);
		}
		break;

	case ITEM_FAN_SPEED:
	{
		// 1.0x-5.0x in steps of 0.5, wrapping.
		float speed = (float)(_wtof(value) + 0.5f);
		if (speed < 1.0f)
			speed = 5.0f;
		else if (speed > 5.0f)
			speed = 1.0f;
		swprintf(text, 100, L"%.01fx", (double)speed);
		SetItemValue(selected, text, TRUE);
		break;
	}

	case ITEM_SCROLL_SPEED:
		OpenKeyboard(label, value, FALSE, 2, 1);
		break;

	case ITEM_TEMP_THRESHOLD:
	{
		// Edit the number without the " C".
		std::wstring number = item.m_strValue.size() > 2 ? item.m_strValue.substr(0, item.m_strValue.size() - 2)
		                                                 : std::wstring(L"");
		OpenKeyboard(label, number.c_str(), FALSE, 2, 1);
		break;
	}

	case ITEM_AUTO_TURN_OFF:
	case ITEM_SS_WAIT:
	case ITEM_NEWSFEED_INTERVAL:
		OpenKeyboard(label, value, FALSE, 4, 1);
		break;
	case ITEM_SS_TEXT:
		OpenKeyboard(label, value, FALSE, 80, 0);
		break;
	case ITEM_NEWSFEED_SEPARATOR:
		OpenKeyboard(label, value, FALSE, 4, 0);
		break;

	case ITEM_DEFAULT_FOLDER:
		path = GetStartPath(item);
		ShowFileBrowser(label, 1, FALSE, 1, path.c_str(), NULL);
		break;
	case ITEM_LANGUAGE_FILE:
		path = GetStartPath(item);
		ShowFileBrowser(L"Browse File", 0, TRUE, 0, path.c_str(), "xml");
		break;
	case ITEM_SS_VIDEO:
		path = GetStartPath(item);
		ShowFileBrowser(L"Browse File", 0, TRUE, 0, path.c_str(), "wmv|xmv");
		break;
	case ITEM_SS_PATH:
		path = GetStartPath(item);
		ShowFileBrowser(GetString(STR_BROWSE_FOLDER), 1, FALSE, 0, path.c_str(), NULL);
		break;
	case ITEM_SKINS_PATH:
		path = GetStartPath(item);
		ShowFileBrowser(L"Browse Skins Path", 1, FALSE, 0, path.c_str(), NULL);
		break;

	case ITEM_LED_TEST:
	{
		// Shows the color at once (not saved).
		int color = (item.m_nValue + 1) % LED_COLOR_COUNT;
		SetItemValue(selected, s_ledColorNames[color], FALSE);
		selected->m_nValue = color;
		SetLedPattern(color, TRUE);
		break;
	}

	case ITEM_SS_TYPE:
	{
		// Text -> Video -> Slideshow -> Text
		int ssType = (item.m_nValue + 1) & 3;
		const wchar_t *name;
		if (ssType == 2)
			name = L"Video";
		else if (ssType == 3)
			name = L"Slideshow";
		else
		{
			ssType = 1;
			name = L"Text";
		}
		SetItemValue(selected, name, TRUE);
		selected->m_nValue = ssType;
		break;
	}

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

	default:
		if (type >= ITEM_LED_DEFAULT && type <= ITEM_LED_BUSY)
		{
			int color = (item.m_nValue + 1) % LED_COLOR_COUNT;
			SetItemValue(selected, s_ledColorNames[color], TRUE);
			selected->m_nValue = color;
		}
		break;
	}
	PlaySound(m_pSoundSelect);
	return S_OK;
}

// The <Settings><Preference><name> element, added if missing.
static CXmlElement *GetPreferenceChild(CXmlElement *settings, const char *name)
{
	CXmlElement *preference = g_config.FindElement(settings, "Preference");
	if (!GetOrAddChildElement("Preference", settings, &preference))
		return NULL;
	CXmlElement *element = preference->FindFirstChild(name);
	if (!GetOrAddChildElement(name, preference, &element))
		return NULL;
	return element;
}

// The <Settings><name> element, added if missing.
static CXmlElement *GetSettingsChild(CXmlElement *settings, const char *name)
{
	CXmlElement *element = g_config.FindElement(settings, name);
	if (!GetOrAddChildElement(name, settings, &element))
		return NULL;
	return element;
}

// 0x000CB4A4
BOOL CSystemSetting::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
	BOOL needRestart = FALSE;
	char message[100];
	if (m_bModified)
	{
		std::string text;
		g_config.m_pCurrent = &g_config.m_root;
		CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
		if (settings == NULL)
		{
			sprintf(message, "%s", GetStringA(STR_MSG_SETTING_NOCONFIG));
			ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			return FALSE;
		}

		CAppSettings *appSettings = g_pSettings;
		SPreferences &prefs = appSettings->m_prefs;
		for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
		{
			if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
				continue;
			const wchar_t *value = it->m_strValue.c_str();
			int type = it->m_nType;
			CXmlElement *element;
			BOOL *flag = NULL;
			const char *name = NULL;

			switch (type)
			{
			case ITEM_NICKNAME:
				// The Xbox's own nickname, not Config.xml.
				if (!XSetNicknameW(value, FALSE))
				{
					sprintf(message, "Unable to set Nickname. Error Code: %d", GetLastError());
					ok = FALSE;
					break;
				}
				InvalidateSkinText(TEXT_NICKNAME);
				break;

			case ITEM_AUTO_RENAME:
				name = "AutoRename";
				flag = &prefs.m_bAutoRename;
				break;
			case ITEM_AUTO_PATCH:
				name = "AutoPatch";
				flag = &prefs.m_bAutoPatch;
				break;
			case ITEM_ABORT_ON_ERROR:
				name = "AbortOnError";
				flag = &prefs.m_bAbortOnError;
				break;

			case ITEM_DEFAULT_FOLDER:
				element = GetPreferenceChild(settings, "DefaultFolder");
				if (element == NULL)
					break;
				text = StrFromWide(value);
				element->SetValue(text.c_str());
				changed = TRUE;
				prefs.m_strDefaultFolder = text;
				break;

			case ITEM_ALLOW_UNINSTALL:
			case ITEM_ALLOW_ITEM_EDIT:
			case ITEM_ALLOW_RECENT_LIST:
			case ITEM_LINK_TO_GAME:
			case ITEM_LINK_TO_SAVE:
			{
				static const char *const attributes[] = { "AllowUninstall", "AllowItemEdit", "AllowRecentList",
				                                          "LinkToGame", "LinkToSave" };
				BOOL *flags[] = { &prefs.m_bAllowUninstall, &prefs.m_bAllowItemEdit, &prefs.m_bAllowRecentList,
				                  &prefs.m_bLinkToGame, &prefs.m_bLinkToSave };
				element = GetPreferenceChild(settings, "ContextMenu");
				if (element == NULL)
					break;
				int i = type - ITEM_ALLOW_UNINSTALL;
				*flags[i] = !IsNoW(value);
				element->SetAttribute(attributes[i], *flags[i] ? "Yes" : "No");
				changed = TRUE;
				break;
			}

			case ITEM_FONT_SCALE:
			{
				// Stored with the screen adjustment of the current mode in
				// <data>\videoset.dat (not Config.xml).
				float scale = (float)_wtof(value);
				if (scale < 0.8f || scale > 3.0f)
					break;
				std::string file = appSettings->m_strDataPath;
				file.append("videoset.dat");
				SScreenAdjust *adjust = appSettings->GetScreenAdjust();
				FILE *fp = NULL;
				if (adjust != NULL)
				{
					adjust->scale = scale;
					fp = fopen(file.c_str(), "wb+");
				}
				if (fp == NULL)
				{
					sprintf(message, "Unable to set Font Scale. Error Code: %d", GetLastError());
					ok = FALSE;
					break;
				}
				fwrite(appSettings->m_video.m_screenAdjust, 0x90, 1, fp);
				fclose(fp);
				appSettings->m_fFontScale = scale;
				break;
			}

			case ITEM_SCROLL_SPEED:
			{
				unsigned speed = (unsigned)_wtol(value);
				if (speed < 10 || speed > 60)
					break;
				element = GetPreferenceChild(settings, "ScrollSpeed");
				if (element == NULL)
				{
					sprintf(message, "Unable to set Scroll Speed. Error Code: %d", GetLastError());
					ok = FALSE;
					break;
				}
				StrFormat(text, "%df", speed);   // Quirk: "20f"
				element->SetValue(text.c_str());
				changed = TRUE;
				appSettings->m_nScrollSpeed = speed;
				break;
			}

			case ITEM_USE_FAHRENHEIT:
				element = GetPreferenceChild(settings, "UseFahrenheit");
				if (element == NULL)
					break;
				prefs.m_bUseFahrenheit = !IsNoW(value);
				element->SetValue(prefs.m_bUseFahrenheit ? "Yes" : "No");
				changed = TRUE;
				InvalidateSkinText(TEXT_CPUTEMPC);
				break;

			case ITEM_TEMP_THRESHOLD:
			{
				unsigned threshold = (unsigned)_wtol(value);
				if (threshold >= 100)
					break;
				element = GetPreferenceChild(settings, "TempThreshold");
				if (element == NULL)
				{
					sprintf(message, "Unable to set Temperature Threshold. Error Code: %d", GetLastError());
					ok = FALSE;
					break;
				}
				StrFormat(text, "%d", threshold);
				element->SetValue(text.c_str());
				changed = TRUE;
				prefs.m_nTempThreshold = threshold;
				break;
			}

			case ITEM_FAN_SPEED:
			{
				unsigned speed = (unsigned)(wcstod(value, NULL) * 10.0);
				if (speed < 10 || speed > 50)
					break;
				element = GetPreferenceChild(settings, "FanSpeed");
				if (element == NULL)
				{
					sprintf(message, "Unable to set Fan Speed. Error Code: %d", GetLastError());
					ok = FALSE;
					break;
				}
				StrFormat(text, "%.02f", (double)((float)speed * 0.1f));
				element->SetValue(text.c_str());
				changed = TRUE;
				SetFanSpeed(speed);
				if (speed == 10)
					SmcSetFanAuto();
				InvalidateSkinText(TEXT_FANSPEED);
				prefs.m_nFanSpeed = speed;
				break;
			}

			case ITEM_LANGUAGE_FILE:
				element = GetPreferenceChild(settings, "LanguageFile");
				if (element == NULL)
					break;
				text = StrFromWide(value);
				prefs.m_strLanguageFile = text;
				element->SetValue(text.c_str());
				changed = TRUE;
				LoadLanguageFile(text.c_str());
				break;

			case ITEM_AUTO_TURN_OFF:
			{
				int minutes = _wtol(value);
				element = GetPreferenceChild(settings, "AutoTurnOff");
				if (element == NULL)
					break;
				StrFormat(text, "%d", minutes);
				element->SetValue(text.c_str());
				prefs.m_nAutoTurnOff = minutes;
				changed = TRUE;
				break;
			}

			case ITEM_SS_WAIT:
			case ITEM_SS_TYPE:
				element = GetPreferenceChild(settings, "ScreenSaver");
				if (element == NULL)
					break;
				if (type == ITEM_SS_WAIT)
				{
					int seconds = _wtol(value);
					StrFormat(text, "%d", seconds);
					element->SetAttribute("Wait", text.c_str());
					prefs.m_nScreenSaverWait = seconds;
				}
				else
				{
					if (_wcsicmp(value, L"Video") == 0)
						prefs.m_nScreenSaverType = 2;
					else if (_wcsicmp(value, L"Slideshow") == 0)
						prefs.m_nScreenSaverType = 3;
					else
						prefs.m_nScreenSaverType = 1;
					StrFormat(text, "%d", prefs.m_nScreenSaverType);
					element->SetAttribute("Type", text.c_str());
				}
				changed = TRUE;
				break;

			case ITEM_SS_TEXT:
			case ITEM_SS_VIDEO:
			case ITEM_SS_PATH:
			{
				CXmlElement *screenSaver = GetPreferenceChild(settings, "ScreenSaver");
				if (screenSaver == NULL)
					break;
				const char *child = type == ITEM_SS_VIDEO ? "Video" : type == ITEM_SS_PATH ? "Path" : "Text";
				element = screenSaver->FindFirstChild(child);
				if (!GetOrAddChildElement(child, screenSaver, &element))
					break;
				text = StrFromWide(value);
				element->SetValue(text.c_str());
				needRestart = TRUE;
				changed = TRUE;
				// Quirk: the path is stored in the text setting.
				if (type == ITEM_SS_VIDEO)
					prefs.m_strScreenSaverVideo = text;
				else
					prefs.m_strScreenSaverText = text;
				break;
			}

			case ITEM_SS_BGM:
				element = GetPreferenceChild(settings, "ScreenSaver");
				if (element == NULL)
					break;
				prefs.m_bScreenSaverBGM = !IsNoW(value);
				element->SetAttribute("EnableBGM", prefs.m_bScreenSaverBGM ? "Yes" : "No");
				changed = TRUE;
				break;

			case ITEM_SKINS_PATH:
				element = GetPreferenceChild(settings, "Skin");
				if (element == NULL)
					break;
				text = StrFromWide(value);
				element->SetAttribute("Path", text.c_str());
				needRestart = TRUE;
				changed = TRUE;
				prefs.m_strSkinPath = text;
				break;

			case ITEM_NEWSFEED_ENABLE:
				element = GetSettingsChild(settings, "Newsfeed");
				if (element == NULL)
					break;
				appSettings->m_bNewsfeedEnable = !IsNoW(value);
				element->SetAttribute("Enable", appSettings->m_bNewsfeedEnable ? "Yes" : "No");
				// Turning it on without any feeds adds the two default ones.
				if (appSettings->m_bNewsfeedEnable && element->FindFirstChild("URL") == NULL)
				{
					AddChildElement(element, "URL", "www.xbox-scene.com/xbox1data/xbox-scene.xml", NULL);
					AddChildElement(element, "URL", "www.xbins.org/xbins.xml", NULL);
				}
				changed = TRUE;
				break;

			case ITEM_NEWSFEED_INTERVAL:
			{
				int minutes = _wtol(value);
				element = GetSettingsChild(settings, "Newsfeed");
				if (element == NULL)
					break;
				StrFormat(text, "%d", minutes);
				element->SetAttribute("Interval", text.c_str());
				changed = TRUE;
				appSettings->m_nNewsfeedInterval = minutes;
				break;
			}

			case ITEM_NEWSFEED_CONTINUOUS:
				element = GetSettingsChild(settings, "Newsfeed");
				if (element == NULL)
					break;
				appSettings->m_bNewsfeedContinuous = !IsNoW(value);
				element->SetAttribute("Continuous", appSettings->m_bNewsfeedContinuous ? "Yes" : "No");
				changed = TRUE;
				break;

			case ITEM_NEWSFEED_SEPARATOR:
			{
				text = StrFromWide(value);
				if (text.size() == 0)
					break;
				element = GetSettingsChild(settings, "Newsfeed");
				if (element == NULL)
					break;
				element->SetAttribute("Separator", text.c_str());
				changed = TRUE;
				size_t count = it->m_strValue.size() > 4 ? 4 : it->m_strValue.size();
				wcsncpy(appSettings->m_szNewsfeedSeparator, value, count);
				break;
			}

			default:
				if (type >= ITEM_LED_DEFAULT && type <= ITEM_LED_BUSY)
				{
					// <Settings><LED><FTPSend>Red</FTPSend>
					const SLedLine &line = s_ledLines[type - ITEM_LED_DEFAULT];
					CXmlElement *led = GetSettingsChild(settings, "LED");
					if (led == NULL)
						break;
					element = led->FindFirstChild(line.element);
					if (!GetOrAddChildElement(line.element, led, &element))
						break;
					text.clear();
					for (int i = 0; i < LED_COLOR_COUNT; i++)
					{
						if (_wcsicmp(value, s_ledColorNames[i]) == 0)
						{
							prefs.*line.color = i;
							text = StrFromWide(s_ledColorNames[i]);
							break;
						}
					}
					if (text.size() == 0)
						break;
					element->SetValue(text.c_str());
					changed = TRUE;
				}
				break;
			}

			if (flag != NULL)
			{
				element = GetPreferenceChild(settings, name);
				if (element != NULL)
				{
					*flag = !IsNoW(value);
					element->SetValue(*flag ? "Yes" : "No");
					changed = TRUE;
				}
			}
		}

		if (!ok)
		{
			ShowMessage(message, L"Save Error", MSGBOX_BUTTONS_OK);
			return FALSE;
		}
		if (changed)
		{
			// (FinishSave, plus putting the LED back; m_bModified is cleared
			// below in any case.)
			ok = FinishSave(ok, message, changed, needRestart);
			SetLedPattern(LED_DEFAULT, TRUE);
			if (!ok)
				return FALSE;
		}
	}
	ClearChanged();
	m_bModified = FALSE;
	return ok;
}
