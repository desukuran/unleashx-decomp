// Control Panel: Sounds and Audio Settings (class CMusicSetting). Writes
// <Settings><Audio>.
#include "SettingsPages.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/MusicManager.h"
#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../system/XSettings.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"

// The sound effect lines, in SoundEffect order.
static const char *const s_effectNames[SOUND_EFFECT_COUNT] = {
	"Scroll", "Select", "Action Complete", "Ambient", "Keyboard Stroke",
	"Back", "Menu Up", "Menu Down", "Game Save Select", "Memory Slot",
};

static const wchar_t *GetSoundTrackModeName(int mode)
{
	switch (mode)
	{
	case SOUNDTRACK_NONE:
		return L"Off";
	case SOUNDTRACK_XBOX:
		return L"Xbox";
	case SOUNDTRACK_CUSTOM:
		return L"Custom";
	case SOUNDTRACK_BOTH:
		return L"Both";
	}
	return NULL;
}

// 0x000C1AFA
CMusicSetting::~CMusicSetting()
{
	FreePopups();
}

// 0x000C2CC1
HRESULT CMusicSetting::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"Sounds and Audio Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	SAudioSettings &audio = g_pSettings->m_audio;
	wchar_t text[260];
	// (Quirk: a mode outside 0-3 leaves the buffer uninitialized.)
	const wchar_t *mode = GetSoundTrackModeName(audio.m_nSoundTrackMode);
	if (mode != NULL)
		swprintf(text, 260, L"%s", mode);
	AddItem(GetString(STR_PLAY_MUSIC), text, ITEM_PLAY_MUSIC, FALSE, TRUE, TRUE, audio.m_nSoundTrackMode);
	AddItem(GetString(STR_RANDOM_MUSIC), audio.m_bRandom ? L"Yes" : L"No", ITEM_RANDOM, FALSE, TRUE);
	AddItem(GetString(STR_GLOBAL), audio.m_bGlobal ? L"Yes" : L"No", ITEM_GLOBAL, FALSE, TRUE, TRUE, audio.m_bRandom);
	MultiByteToWideChar(CP_ACP, 0, audio.m_strMusicPath.c_str(), (int)audio.m_strMusicPath.size() + 1, text, 100);
	AddItem(GetString(STR_MUSIC_PATH), text, ITEM_MUSIC_PATH, FALSE, TRUE);
	AddItem(GetString(STR_SURROUND), audio.m_bEnable51 ? L"Yes" : L"No", ITEM_SURROUND, FALSE, TRUE, TRUE,
	        audio.m_nSoundTrackMode);
	swprintf(text, 260, L"%d", audio.m_nMusicVolume);
	AddItem(GetString(STR_MUSIC_VOLUME), text, ITEM_MUSIC_VOLUME, FALSE, TRUE, TRUE, audio.m_nMusicVolume);

	AddItem(GetString(STR_SOUND), L"", ITEM_SOUND_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_ENABLE), audio.m_bSoundEffects ? L"Yes" : L"No", ITEM_SOUND_ENABLE, FALSE, TRUE, TRUE,
	        audio.m_bSoundEffects);
	swprintf(text, 260, L"%d", audio.m_nSoundVolume);
	AddItem(GetString(STR_SOUND_VOLUME), text, ITEM_SOUND_VOLUME, FALSE, TRUE, TRUE, audio.m_nSoundVolume);

	AddItem(GetString(STR_SOUND_EFFECTS), L"", ITEM_EFFECTS_HEADER, TRUE, TRUE);
	for (int i = 0; i < SOUND_EFFECT_COUNT; i++)
	{
		wchar_t label[50];
		StrUtf8ToWide(s_effectNames[i], label, 50);
		StrUtf8ToWide(audio.m_strSound[i].c_str(), text, 260);
		if (_wcsicmp(text, L"[None]") == 0)
			memset(text, 0, sizeof(text));
		AddItem(label, text, ITEM_EFFECT_FIRST + i, FALSE, audio.m_bSoundEffects);
	}

	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// 0x000C261F
HRESULT CMusicSetting::Render()
{
	// The label width lives in a static here (0x00278D64): it's worked out
	// once for the lifetime of the program, not per page.
	static float s_fLabelWidth = 0.0f;
	if (!m_bClosed)
		RenderPage(s_fLabelWidth, TRUE);
	return S_OK;
}

// 0x000D4A57
HRESULT CMusicSetting::HandleInput(int *action, SGamepadState **pad)
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
	static DWORD s_dwTimer = GetTickCount() + 1000;   // 0x0027A5A0, not used for anything
	(void)s_dwTimer;
	CSettingItem *selected = &m_items[m_nSelected];
	BOOL isFileLine = item.m_nType == ITEM_MUSIC_PATH ||
	                  (item.m_nType >= ITEM_EFFECT_FIRST && item.m_nType <= ITEM_EFFECT_LAST);
	BOOL isVolumeLine = item.m_nType == ITEM_MUSIC_VOLUME || item.m_nType == ITEM_SOUND_VOLUME;
	wchar_t text[100];

	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		break;

	case INPUT_X:
		// Clears a path.
		if (!IsSelectedDisabled() && isFileLine)
			SetItemValue(selected, L"", TRUE);
		return S_OK;

	case INPUT_LEFT:
		if (IsSelectedDisabled() || item.m_nValue == 0 || !isVolumeLine)
			return S_OK;
		selected->m_nValue = item.m_nValue - 1;
		swprintf(text, 100, L"%d", selected->m_nValue);
		SetItemValue(selected, text, TRUE);
		return S_OK;

	case INPUT_RIGHT:
		if (IsSelectedDisabled() || !isVolumeLine)
			return S_OK;
		selected->m_nValue = (unsigned)(item.m_nValue + 1) > 100 ? 100 : item.m_nValue + 1;
		swprintf(text, 100, L"%d", selected->m_nValue);
		SetItemValue(selected, text, TRUE);
		return S_OK;

	default:
		// (B/BACK is handled here too, the same as in the base class, which
		// has always used it.)
		return S_OK;
	}

	if (IsSelectedDisabled())
		return S_OK;
	switch (item.m_nType)
	{
	case ITEM_PLAY_MUSIC:
	{
		// Off -> Xbox -> Custom -> Both -> Off; Off disables the music lines.
		int mode = (item.m_nValue + 1) & 3;
		SetItemValue(selected, GetSoundTrackModeName(mode), TRUE);
		selected->m_nValue = mode;
		for (int i = ITEM_RANDOM; i <= ITEM_MUSIC_VOLUME; i++)
			SetItemEnabled(i, mode != SOUNDTRACK_NONE);
		break;
	}

	case ITEM_RANDOM:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		SetItemEnabled(ITEM_GLOBAL, item.m_strValue == L"No");
		break;

	case ITEM_SOUND_ENABLE:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		for (int i = ITEM_SOUND_VOLUME; i <= ITEM_EFFECT_LAST; i++)
			SetItemEnabled(i, item.m_strValue == L"No");
		break;

	case ITEM_GLOBAL:
	case ITEM_SURROUND:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		break;

	case ITEM_MUSIC_PATH:
		path = StrFromWide(item.m_strValue.c_str());
		if (path.size() != 0)
			path = GetPathDirectory(path.c_str());
		ShowFileBrowser(L"Custom Music Path", 1, FALSE, 0, path.c_str(), NULL);
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

	default:
		if (item.m_nType >= ITEM_EFFECT_FIRST && item.m_nType <= ITEM_EFFECT_LAST)
		{
			path = StrFromWide(item.m_strValue.c_str());
			if (path.size() != 0)
				path = GetPathDirectory(path.c_str());
			ShowFileBrowser(L"Wave File", 0, TRUE, 0, path.c_str(), "WAV");
		}
		break;
	}
	PlaySound(m_pSoundSelect);
	return S_OK;
}

// 0x000CE1B9
BOOL CMusicSetting::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
	BOOL needRestart = FALSE;
	char message[100];   // Quirk: some failures show it uninitialized
	if (!m_bModified)
		return TRUE;

	std::string text;
	// Here <Settings> is created too if missing.
	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	CXmlElement *audioElement = NULL;
	g_config.m_pCurrent = &g_config.m_root;
	if (GetOrAddChildElement("Settings", &g_config.m_root, &settings))
	{
		audioElement = settings->FindFirstChild("Audio");
		if (!GetOrAddChildElement("Audio", settings, &audioElement))
			settings = NULL;
	}
	if (settings == NULL || audioElement == NULL)
	{
		sprintf(message, GetStringA(STR_MSG_SETTING_NOCONFIG), STR_MSG_SETTING_NOCONFIG);
		ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return FALSE;
	}

	SAudioSettings &audio = g_pSettings->m_audio;
	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
	{
		if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
			continue;
		const wchar_t *value = it->m_strValue.c_str();
		int type = it->m_nType;
		CXmlElement *element;

		if (type >= ITEM_PLAY_MUSIC && type <= ITEM_SURROUND)
		{
			element = audioElement->FindFirstChild("EnableSoundTrack");
			if (!GetOrAddChildElement("EnableSoundTrack", audioElement, &element))
			{
				// Quirk: Play Music, Random and Global are skipped silently;
				// Surround fails with an unset message.
				if (type == ITEM_MUSIC_PATH)
					sprintf(message, "Unable to set '%s' for custom soundtrack path.", text.c_str());
				if (type == ITEM_MUSIC_PATH || type == ITEM_SURROUND)
					ok = FALSE;
				continue;
			}
			switch (type)
			{
			case ITEM_PLAY_MUSIC:
				if (_wcsicmp(value, L"Xbox") == 0)
				{
					text = "Xbox";
					audio.m_nSoundTrackMode = SOUNDTRACK_XBOX;
				}
				else if (_wcsicmp(value, L"Custom") == 0)
				{
					text = "Custom";
					audio.m_nSoundTrackMode = SOUNDTRACK_CUSTOM;
				}
				else if (_wcsicmp(value, L"Both") == 0)
				{
					text = "Both";
					audio.m_nSoundTrackMode = SOUNDTRACK_BOTH;
				}
				else
				{
					text = "Off";
					audio.m_nSoundTrackMode = SOUNDTRACK_NONE;
				}
				element->SetValue(text.c_str());
				changed = needRestart = TRUE;
				break;

			case ITEM_RANDOM:
				audio.m_bRandom = !IsNoW(value);
				if (g_pMusicManager != NULL && g_pMusicManager->IsPlaying())
					g_pMusicManager->SetRandom(audio.m_bRandom);
				element->SetAttribute("Random", audio.m_bRandom ? "Yes" : "No");
				changed = TRUE;
				break;

			case ITEM_GLOBAL:
				audio.m_bGlobal = !IsNoW(value);
				if (g_pMusicManager != NULL && g_pMusicManager->IsPlaying())
					g_pMusicManager->SetGlobal(audio.m_bGlobal);
				element->SetAttribute("Global", audio.m_bGlobal ? "Yes" : "No");
				changed = TRUE;
				break;

			case ITEM_MUSIC_PATH:
				// The folder must exist (an empty path is allowed).
				text = StrFromWide(value);
				if (text.size() != 0)
				{
					std::string resolved;
					ResolvePath(text.c_str(), resolved);
					text = resolved;
					if (text.size() != 0 && !DirectoryExists(text.c_str()))
					{
						sprintf(message, "Unable to set '%s' for custom soundtrack path.", text.c_str());
						ok = FALSE;
						continue;
					}
				}
				element->SetAttribute("MusicPath", text.c_str());
				changed = needRestart = TRUE;
				audio.m_strMusicPath = text;
				break;

			case ITEM_SURROUND:
			{
				BOOL enable = !IsNoW(value);
				audio.SetEnable51(enable);
				if (g_pMusicManager != NULL && g_pMusicManager->IsPlaying())
					g_pMusicManager->SetSpeakerConfig(audio.m_bEnable51 ? g_pSettings->m_speakerMixBins : NULL);
				SetAudioAC3(enable);   // the Xbox's own Dolby Digital setting
				element->SetAttribute("Enable_5_1", enable ? "Yes" : "No");
				changed = TRUE;
				break;
			}
			}
			continue;
		}

		switch (type)
		{
		case ITEM_MUSIC_VOLUME:
		case ITEM_SOUND_VOLUME:
		{
			unsigned volume = (unsigned)_wtol(value);
			if (volume > 100)
				continue;
			const char *name = type == ITEM_MUSIC_VOLUME ? "MusicVolume" : "SoundVolume";
			element = audioElement->FindFirstChild(name);
			if (!GetOrAddChildElement(name, audioElement, &element))
			{
				sprintf(message,
				        type == ITEM_MUSIC_VOLUME ? "Unable to set Music Volume. Error Code: %d"
				                                  : "Unable to set Sound Volume. Error Code: %d",
				        GetLastError());
				ok = FALSE;
				continue;
			}
			StrFormat(text, "%d", volume);
			element->SetValue(text.c_str());
			if (type == ITEM_MUSIC_VOLUME)
			{
				audio.m_nMusicVolume = volume;
				if (g_pMusicManager != NULL && g_pMusicManager->IsPlaying())
					g_pMusicManager->SetVolume((float)g_pSettings->GetMusicVolume());
			}
			else
			{
				audio.m_nSoundVolume = volume;
				g_soundEffects.SetQuiet(volume < 5);
			}
			changed = TRUE;
			break;
		}

		case ITEM_SOUND_ENABLE:
			element = audioElement->FindFirstChild("EnableSoundEffect");
			if (!GetOrAddChildElement("EnableSoundEffect", audioElement, &element))
				continue;
			audio.m_bSoundEffects = !IsNoW(value);
			text = audio.m_bSoundEffects ? "Yes" : "No";
			g_soundEffects.SetQuiet(!audio.m_bSoundEffects);
			g_soundEffects.m_bDisabled = !audio.m_bSoundEffects;
			element->SetValue(text.c_str());
			changed = needRestart = TRUE;
			break;

		default:
			if (type >= ITEM_EFFECT_FIRST && type <= ITEM_EFFECT_LAST)
			{
				// <Audio><Action_Complete>file.wav</Action_Complete>; reloaded at once.
				text = StrFromWide(it->m_strLabel.c_str());
				StrReplaceAll(text, " ", "_");
				element = audioElement->FindFirstChild(text.c_str());
				if (!GetOrAddChildElement(text.c_str(), audioElement, &element))
				{
					ok = FALSE;
					break;
				}
				text = StrFromWide(value);
				if (text.size() == 0)
					text = "[None]";
				element->SetValue(text.c_str());
				changed = needRestart = TRUE;
				std::string name = StrFromWide(it->m_strLabel.c_str());
				CSound *sound;
				g_soundEffects.Load(name.c_str(), text.c_str(), &sound);
			}
			break;
		}
	}
	return FinishSave(ok, message, changed, needRestart);
}
