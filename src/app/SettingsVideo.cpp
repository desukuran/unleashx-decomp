// Control Panel: Video Settings. The video standard and the allowed modes
// are the Xbox's own EEPROM settings (they need a restart); the rest goes to
// <Preference><VideoSettings> and <Preference><AutoVideoMode> in Config.xml.
#include "SettingsPages.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/SkinColors.h"
#include "../system/XSettings.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"
#include "Screens.h"
#include "XBApp.h"

// 0x000C1BED
CVideoSetting::CVideoSetting()
{
	m_unknownB8 = 0;
}

// 0x000C1BD9
CVideoSetting::~CVideoSetting()
{
	FreePopups();
}

// 0x000C3A26
HRESULT CVideoSetting::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"Video Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	// The HDTV modes can only be changed with the HDTV (or SCART) AV pack.
	BOOL hdtvPack = XGetAVPack() == XC_AV_PACK_HDTV || XGetAVPack() == XC_AV_PACK_SCART;
	SVideoSettings &video = g_pSettings->m_video;
	SPreferences &prefs = g_pSettings->m_prefs;
	wchar_t text[100];
	memset(text, 0, sizeof(text));

	switch (XGetVideoStandard())
	{
	case XC_VIDEO_STANDARD_NTSC_J:
		wcscpy(text, L"NTSC J");
		break;
	case XC_VIDEO_STANDARD_PAL_I:
		wcscpy(text, L"PAL");
		break;
	default:
		wcscpy(text, L"NTSC M");
		break;
	}
	AddItem(L"Video Standard", text, ITEM_STANDARD, FALSE, TRUE);
	AddItem(GetString(STR_FORMAT), video.m_bWidescreen ? L"Widescreen" : L"Normal", ITEM_FORMAT, FALSE, TRUE);
	AddItem(GetString(STR_ENABLE_480P), video.m_bUse480p ? L"Yes" : L"No", ITEM_480P, FALSE, hdtvPack);
	AddItem(GetString(STR_ENABLE_720P), video.m_bUse720p ? L"Yes" : L"No", ITEM_720P, FALSE, hdtvPack);
	AddItem(GetString(STR_ENABLE_1080I), video.m_bUse1080i ? L"Yes" : L"No", ITEM_1080I, FALSE, hdtvPack);
	AddItem(GetString(STR_ENABLE_PAL60), video.m_bUsePAL60 ? L"Yes" : L"No", ITEM_PAL60, FALSE,
	        XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I);
	AddItem(GetString(STR_FASTER_REFRESH_RATE), video.m_bFastRefresh ? L"Yes" : L"No", ITEM_FAST_REFRESH, FALSE,
	        TRUE);
	AddItem(GetString(STR_TEXTURE_COMPRESSION), video.m_bTextureCompression ? L"Yes" : L"No",
	        ITEM_TEXTURE_COMPRESSION, FALSE, TRUE);
	AddItem(GetString(STR_TEST), L"(Test Settings without Saving)", ITEM_TEST, FALSE, TRUE);

	AddItem(GetString(STR_QUALITY), L"", ITEM_QUALITY_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_COLOR), video.m_bUse16bit ? L"16 bit" : L"32 bit", ITEM_COLOR, FALSE, TRUE);
	AddItem(GetString(STR_SOFTEN), video.m_bSoften ? L"Yes" : L"No", ITEM_SOFTEN, FALSE, TRUE);
	unsigned level = (unsigned)video.m_nFilterLevel;
	if (level > 5)
		level = 5;
	swprintf(text, 100, L"%d", level);
	AddItem(GetString(STR_FILTER_LEVEL), text, ITEM_FILTER_LEVEL, FALSE, TRUE);

	AddItem(L"Video Mode (EEPROM Safe)", L"", ITEM_VIDEO_MODE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_AUTO_SWITCH), prefs.m_bAutoVideoMode ? L"Yes" : L"No", ITEM_AUTO_SWITCH, FALSE, TRUE);
	switch (prefs.m_nMultiRegion)
	{
	case 1:
		wcscpy(text, L"NTSC M");
		break;
	case 2:
		wcscpy(text, L"NTSC J");
		break;
	case 3:
		wcscpy(text, L"PAL");
		break;
	default:
		wcscpy(text, L"Off");
		break;
	}
	AddItem(GetString(STR_MULTI_REGION_MODE), text, ITEM_MULTI_REGION, FALSE, prefs.m_bAutoVideoMode);
	AddItem(GetString(STR_FORCE_PAL60), prefs.m_bAutoVideoPAL60 ? L"Yes" : L"No", ITEM_FORCE_PAL60, FALSE,
	        prefs.m_bAutoVideoMode);

	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// 0x000BEEAC
void CVideoSetting::SetItemEnabled(int, BOOL enabled)
{
	m_items[ITEM_MULTI_REGION].m_bEnabled = enabled;
	m_items[ITEM_FORCE_PAL60].m_bEnabled = enabled;
}

// Part of HandleInput. The mode is built from the lines on the page and
// the modes the Xbox allows, the device is reset to it for 10 seconds and
// then back.
void CVideoSetting::TestVideoMode()
{
	SPresentParameters params = *g_pPresentParams;
	params.FullScreen_PresentationInterval =
		IsYesW(m_items[ITEM_FAST_REFRESH].m_strValue.c_str()) ? D3DPRESENT_INTERVAL_IMMEDIATE
		                                                      : D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE;
	params.BackBufferWidth = 640;
	if (g_pSettings->m_video.m_bPAL)
	{
		params.BackBufferHeight = 576;
		if ((g_dwVideoFlags & XC_VIDEO_FLAGS_WIDESCREEN) &&
		    _wcsicmp(m_items[ITEM_FORMAT].m_strValue.c_str(), L"Widescreen") == 0)
		{
			params.Flags |= D3DPRESENTFLAG_WIDESCREEN;
			params.BackBufferWidth = 720;
		}
		params.FullScreen_RefreshRateInHz = 0;
		if ((g_dwVideoFlags & XC_VIDEO_FLAGS_PAL_60Hz) && IsYesW(m_items[ITEM_PAL60].m_strValue.c_str()))
		{
			params.FullScreen_RefreshRateInHz = 60;
			params.BackBufferHeight = 480;
		}
	}
	else
	{
		params.BackBufferHeight = 480;
		if ((g_dwVideoFlags & XC_VIDEO_FLAGS_WIDESCREEN) &&
		    _wcsicmp(m_items[ITEM_FORMAT].m_strValue.c_str(), L"Widescreen") == 0)
		{
			params.Flags |= D3DPRESENTFLAG_WIDESCREEN;
			params.BackBufferWidth = 720;
		}
		if (XGetAVPack() == XC_AV_PACK_HDTV)
		{
			if ((g_dwVideoFlags & XC_VIDEO_FLAGS_HDTV_1080i) && IsYesW(m_items[ITEM_1080I].m_strValue.c_str()))
			{
				params.Flags = D3DPRESENTFLAG_WIDESCREEN | D3DPRESENTFLAG_INTERLACED;
				params.BackBufferWidth = 1920;
				params.BackBufferHeight = 1080;
			}
			else if ((g_dwVideoFlags & XC_VIDEO_FLAGS_HDTV_720p) && IsYesW(m_items[ITEM_720P].m_strValue.c_str()))
			{
				params.Flags = D3DPRESENTFLAG_WIDESCREEN | D3DPRESENTFLAG_PROGRESSIVE;
				params.BackBufferWidth = 1280;
				params.BackBufferHeight = 720;
			}
			else if ((g_dwVideoFlags & XC_VIDEO_FLAGS_HDTV_480p) && IsYesW(m_items[ITEM_480P].m_strValue.c_str()))
			{
				params.Flags |= D3DPRESENTFLAG_PROGRESSIVE;
				params.BackBufferWidth = 720;
				params.BackBufferHeight = 480;
			}
		}
	}

	if (params.BackBufferWidth == g_pPresentParams->BackBufferWidth &&
	    params.BackBufferHeight == g_pPresentParams->BackBufferHeight && params.Flags == g_pPresentParams->Flags &&
	    params.FullScreen_PresentationInterval == g_pPresentParams->FullScreen_PresentationInterval)
		return;

	if (SUCCEEDED(g_pd3dDevice->Reset(&params)))
	{
		D3DVIEWPORT8 viewport = { 0, 0, params.BackBufferWidth, params.BackBufferHeight, 0.0f, 1.0f };
		g_pd3dDevice->SetViewport(&viewport);
		float scale = (float)params.BackBufferWidth * (1.0f / 640.0f);
		if (scale > 1.0f)
			scale = 1.0f;
		m_pFont->SetScaleFactors(scale, scale);

		DWORD now = GetTickCount();
		DWORD end = now + 10000;
		while (now < end)
		{
			wchar_t text[100];
			float centerX = (float)params.BackBufferWidth * 0.5f;
			g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
			                    g_colorScheme.AMBIENT_SKYCOLOR, 1.0f, 0);
			float y = (float)params.BackBufferHeight * (1.0f / 3.0f);
			m_pFont->DrawText(centerX, y, g_colorScheme.DIALOG_TEXTCOLOR, L"Selected Setting is working properly.",
			                  XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
			y = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY + y;
			m_pFont->DrawText(centerX, y, g_colorScheme.DIALOG_TEXTCOLOR, L"You need to save and restart the Xbox.",
			                  XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
			y = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY + y;
			swprintf(text, 100, L"Screen Size Width = %d, Height = %d", params.BackBufferWidth,
			         params.BackBufferHeight);
			m_pFont->DrawText(centerX, y, g_colorScheme.DIALOG_TEXTCOLOR, text, XBFONT_CENTER_X | XBFONT_CENTER_Y,
			                  0.0f);
			y = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY + 10.0f + y;
			swprintf(text, 100, L"Your settings will reset in %d seconds", (end - now) / 1000);
			m_pFont->DrawText(centerX, y, g_colorScheme.DIALOG_TEXTCOLOR, text, XBFONT_CENTER_X | XBFONT_CENTER_Y,
			                  0.0f);
			g_pd3dDevice->Swap(0);
			Sleep(500);
			now = GetTickCount();
		}
		m_pFont->SetScaleFactors(g_pSettings->m_fUnknown131C, g_pSettings->m_fUnknown131C);
	}
	g_pd3dDevice->Reset(g_pPresentParams);
}

// 0x000C9182
HRESULT CVideoSetting::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	if (SUCCEEDED(CSettingsBase::HandleInput(action, pad)) && m_bModified)
	{
		if (m_bAskSave)
		{
			// "Save changes?" was answered Yes.
			if (Save())
			{
				m_bClosed = TRUE;
				m_bAskSave = FALSE;
				return S_OK;
			}
		}
		else if (m_bClosed)
		{
			// Undo the filters tried out on the page. Quirk: unreachable (the
			// base class returns E_FAIL when the page is closed with changes),
			// so leaving without saving keeps them until the next restart.
			g_pd3dDevice->SetSoftDisplayFilter(g_pSettings->m_video.m_bSoften);
			g_pd3dDevice->SetFlickerFilter(g_pSettings->m_video.m_nFilterLevel);
		}
	}
	if (*action == INPUT_NONE)
		return S_FALSE;

	CSettingItem item = m_items[m_nSelected];
	// A timer that nothing reads (0x0027A40C).
	static DWORD s_dwTimer = GetTickCount() + 1000;
	(void)s_dwTimer;

	// The page also has its own B/BACK and up/down handling here, the same as
	// the base class's, but the base class has always used those actions.
	if (*action != INPUT_A && *action != INPUT_START)
		return S_OK;
	if (IsSelectedDisabled())
		return S_OK;

	CSettingItem *selected = &m_items[m_nSelected];
	const wchar_t *value = item.m_strValue.c_str();
	switch (item.m_nType)
	{
	case ITEM_STANDARD:
		// NTSC M -> NTSC J -> PAL -> NTSC M
		if (_wcsicmp(value, L"NTSC M") == 0)
			SetItemValue(selected, L"NTSC J", TRUE);
		else if (_wcsicmp(value, L"NTSC J") == 0)
			SetItemValue(selected, L"PAL", TRUE);
		else
			SetItemValue(selected, L"NTSC M", TRUE);
		break;

	case ITEM_FORMAT:
		SetItemValue(selected, _wcsicmp(value, L"Normal") != 0 ? L"Normal" : L"Widescreen", TRUE);
		break;

	case ITEM_SOFTEN:
		// Applied right away.
		g_pd3dDevice->SetSoftDisplayFilter(item.m_strValue != L"Yes");
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		break;

	case ITEM_480P:
	case ITEM_720P:
	case ITEM_1080I:
	case ITEM_PAL60:
	case ITEM_FAST_REFRESH:
	case ITEM_TEXTURE_COMPRESSION:
	case ITEM_FORCE_PAL60:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		break;

	case ITEM_COLOR:
		SetItemValue(selected, item.m_strValue != L"16 bit" ? L"16 bit" : L"32 bit", TRUE);
		break;

	case ITEM_FILTER_LEVEL:
	{
		// 0..5, applied right away.
		int level = (_wtol(value) + 1) % 6;
		wchar_t text[100];
		swprintf(text, 100, L"%d", level);
		SetItemValue(selected, text, TRUE);
		g_pd3dDevice->SetFlickerFilter(level);
		break;
	}

	case ITEM_TEST:
		if (!m_bModified)
			ShowMessage("Change some of your settings before proceeding with the test.", L"Info", MSGBOX_BUTTONS_OK);
		else
			TestVideoMode();
		break;

	case ITEM_AUTO_SWITCH:
	{
		BOOL enable;
		if (_wcsicmp(value, L"Yes") == 0)
		{
			SetItemValue(selected, L"No", TRUE);
			enable = FALSE;
		}
		else if (_wcsicmp(value, L"No") != 0)
		{
			SetItemValue(selected, L"No", TRUE);
			enable = FALSE;
		}
		else
		{
			SetItemValue(selected, L"Yes", TRUE);
			enable = TRUE;
		}
		m_items[ITEM_FORCE_PAL60].m_bEnabled = enable;
		m_items[ITEM_MULTI_REGION].m_bEnabled = enable;
		break;
	}

	case ITEM_MULTI_REGION:
		// Off -> NTSC M -> NTSC J -> PAL -> Off
		if (_wcsicmp(value, L"Off") == 0)
			SetItemValue(selected, L"NTSC M", TRUE);
		else if (_wcsicmp(value, L"NTSC M") == 0)
			SetItemValue(selected, L"NTSC J", TRUE);
		else if (_wcsicmp(value, L"NTSC J") == 0)
			SetItemValue(selected, L"PAL", TRUE);
		else
			SetItemValue(selected, L"Off", TRUE);
		break;

	case ITEM_SAVE:
		if (m_bModified)
			Save();
		else
			m_bClosed = TRUE;
		break;

	case ITEM_RESET:
		RevertChanges();
		// Quirk: enables the Auto Switch lines whatever the restored value is.
		SetItemEnabled(ITEM_AUTO_SWITCH, TRUE);
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

// The <Settings><Preference><name> element, added if missing.
static CXmlElement *GetPreferenceElement(CXmlElement *settings, const char *name)
{
	CXmlElement *preference = g_config.FindElement(settings, "Preference");
	if (!GetOrAddChildElement("Preference", settings, &preference))
		return NULL;
	CXmlElement *element = preference->FindFirstChild(name);
	if (!GetOrAddChildElement(name, preference, &element))
		return NULL;
	return element;
}

// 0x000C8387
BOOL CVideoSetting::Save()
{
	BOOL ok = TRUE;
	BOOL saveConfig = FALSE;
	BOOL needRestart = FALSE;
	BOOL resetDevice = FALSE;
	char message[100];
	if (!m_bModified)
		return TRUE;

	std::string text;
	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
	{
		// The message has no format fields; GetStringA's argument is still on
		// the stack as a third sprintf argument in the original.
		sprintf(message, GetStringA(STR_MSG_SETTING_NOCONFIG), STR_MSG_SETTING_NOCONFIG);
		ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return FALSE;
	}

	SVideoSettings &video = g_pSettings->m_video;
	SPreferences &prefs = g_pSettings->m_prefs;
	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
	{
		if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
			continue;
		const wchar_t *value = it->m_strValue.c_str();
		CXmlElement *element;
		BOOL *flag = NULL;
		const char *attribute = NULL;
		switch (it->m_nType)
		{
		case ITEM_STANDARD:
		{
			// Only when it differs from the current standard.
			DWORD standard = XGetVideoStandard();
			DWORD region;
			if (_wcsicmp(value, L"NTSC M") == 0)
			{
				if (standard == XC_VIDEO_STANDARD_NTSC_M)
					continue;
				region = AV_REGION_NTSC_M;
			}
			else if (_wcsicmp(value, L"NTSC J") == 0)
			{
				if (standard == XC_VIDEO_STANDARD_NTSC_J)
					continue;
				region = AV_REGION_NTSC_J;
			}
			else if (_wcsicmp(value, L"PAL") == 0)
			{
				if (standard == XC_VIDEO_STANDARD_PAL_I)
					continue;
				region = AV_REGION_PAL_I;
			}
			else
			{
				region = 0;   // can't happen; the original passes a stale register
			}
			SetVideoStandard(region);
			saveConfig = TRUE;
			needRestart = TRUE;
			continue;
		}

		case ITEM_FORMAT:
		case ITEM_480P:
		case ITEM_720P:
		case ITEM_1080I:
		case ITEM_PAL60:
			element = GetPreferenceElement(settings, "VideoSettings");
			if (element == NULL)
				break;
			if (it->m_nType == ITEM_FORMAT)
			{
				video.m_bWidescreen = _wcsicmp(value, L"Widescreen") == 0;
				SetVideoWidescreen(video.m_bWidescreen);
				attribute = "Widescreen";
				flag = &video.m_bWidescreen;
			}
			else
			{
				switch (it->m_nType)
				{
				case ITEM_480P:
					flag = &video.m_bUse480p;
					attribute = "Use480P";
					break;
				case ITEM_720P:
					flag = &video.m_bUse720p;
					attribute = "Use720P";
					break;
				case ITEM_1080I:
					flag = &video.m_bUse1080i;
					attribute = "Use1080I";
					break;
				default:
					flag = &video.m_bUsePAL60;
					attribute = "UsePAL60";
					break;
				}
				*flag = !IsNoW(value);
				switch (it->m_nType)
				{
				case ITEM_480P:
					SetVideo480p(*flag);
					break;
				case ITEM_720P:
					SetVideo720p(*flag);
					break;
				case ITEM_1080I:
					SetVideo1080i(*flag);
					break;
				default:
					SetVideoPAL60(*flag);
					break;
				}
			}
			element->SetAttribute(attribute, *flag ? "Yes" : "No");
			saveConfig = TRUE;
			needRestart = TRUE;
			continue;

		case ITEM_FAST_REFRESH:
			element = GetPreferenceElement(settings, "VideoSettings");
			if (element == NULL)
				break;
			if (!IsNoW(value))
			{
				resetDevice = !video.m_bFastRefresh;
				video.m_bFastRefresh = TRUE;
			}
			else
			{
				resetDevice = video.m_bFastRefresh;
				video.m_bFastRefresh = FALSE;
			}
			element->SetAttribute("FastRefresh", video.m_bFastRefresh ? "Yes" : "No");
			saveConfig = TRUE;
			continue;

		case ITEM_TEXTURE_COMPRESSION:
			element = GetPreferenceElement(settings, "VideoSettings");
			if (element == NULL)
				break;
			video.m_bTextureCompression = !IsNoW(value);
			element->SetAttribute("EnableTextureCompression", video.m_bTextureCompression ? "Yes" : "No");
			saveConfig = TRUE;
			continue;

		case ITEM_COLOR:
		{
			element = GetPreferenceElement(settings, "VideoSettings");
			if (element == NULL)
				break;
			BOOL old = video.m_bUse16bit;
			video.m_bUse16bit = StrFromWide(value) == "16 bit";
			if (!resetDevice)
				resetDevice = old != video.m_bUse16bit;
			element->SetAttribute("Use16bit", video.m_bUse16bit ? "True" : "False");
			saveConfig = TRUE;
			continue;
		}

		case ITEM_SOFTEN:
			element = GetPreferenceElement(settings, "VideoSettings");
			if (element == NULL)
				break;
			video.m_bSoften = !IsNoW(value);
			element->SetAttribute("Soften", video.m_bSoften ? "Yes" : "No");
			saveConfig = TRUE;
			g_pd3dDevice->SetSoftDisplayFilter(video.m_bSoften);
			continue;

		case ITEM_FILTER_LEVEL:
		{
			element = GetPreferenceElement(settings, "VideoSettings");
			if (element == NULL)
				break;
			int level = _wtol(value) < 6 ? _wtol(value) : 5;
			StrFormat(text, "%d", level);
			video.m_nFilterLevel = level;
			element->SetAttribute("FilterLevel", text.c_str());
			saveConfig = TRUE;
			g_pd3dDevice->SetFlickerFilter(video.m_nFilterLevel);
			continue;
		}

		case ITEM_AUTO_SWITCH:
		case ITEM_MULTI_REGION:
		case ITEM_FORCE_PAL60:
			element = GetPreferenceElement(settings, "AutoVideoMode");
			if (element == NULL)
			{
				sprintf(message, "Unable to set value for Video Mode switching. Error Code: %d", GetLastError());
				ok = FALSE;
				continue;
			}
			if (it->m_nType == ITEM_AUTO_SWITCH)
			{
				prefs.m_bAutoVideoMode = !IsNoW(value);
				element->SetValue(prefs.m_bAutoVideoMode ? "Yes" : "No");
			}
			else if (it->m_nType == ITEM_MULTI_REGION)
			{
				if (_wcsicmp(value, L"NTSC M") == 0)
				{
					text = "NTSC M";
					prefs.m_nMultiRegion = 1;
				}
				else if (_wcsicmp(value, L"NTSC J") == 0)
				{
					text = "NTSC J";
					prefs.m_nMultiRegion = 2;
				}
				else if (_wcsicmp(value, L"PAL") == 0)
				{
					text = "PAL";
					prefs.m_nMultiRegion = 3;
				}
				else
				{
					text = "Off";
					prefs.m_nMultiRegion = 0;
				}
				element->SetAttribute("MultiRegion", text.c_str());
			}
			else
			{
				prefs.m_bAutoVideoPAL60 = !IsNoW(value);
				element->SetAttribute("UsePAL60", prefs.m_bAutoVideoPAL60 ? "Yes" : "No");
			}
			saveConfig = TRUE;
			continue;

		default:
			continue;
		}
		// The <VideoSettings> element couldn't be made.
		sprintf(message, "Unable to set Video Settings. Error Code: %d", GetLastError());
		ok = FALSE;
	}

	if (!ok)
	{
		ShowMessage(message, L"Save Error", MSGBOX_BUTTONS_OK);
		return FALSE;
	}
	if (!saveConfig)
		return TRUE;

	if (g_pSettings->m_bRunFromDVD)
	{
		ShowMessage("Some settings have been applied but you need to run this application from the hard drive to "
		            "persist the changes.",
		            L"Warning", MSGBOX_BUTTONS_OK);
	}
	else
	{
		std::string path;
		if (g_config.m_strFileName.size() != 0)
			ResolvePath(g_config.m_strFileName.c_str(), path);
		else
			ResolvePath("D:\\config.xml", path);
		if (!g_config.SaveAs(path.c_str()))
		{
			DWORD error = GetLastError();
			sprintf(message, GetStringA(STR_MSG_SETTING_SAVE_ERROR), error);
			ShowMessage(message, L"Save Error", MSGBOX_BUTTONS_OK);
			ok = FALSE;
		}
		else
		{
			if (needRestart)
			{
				ShowRestartMessage();
			}
			else
			{
				if (resetDevice)
				{
					// The color depth and refresh rate apply right away.
					g_pPresentParams->BackBufferFormat = video.m_bUse16bit ? D3DFMT_X1R5G5B5 : D3DFMT_LIN_A8R8G8B8;
					if (video.m_bFastRefresh)
						g_pPresentParams->FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
					else if (g_pPresentParams->FullScreen_PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE)
						g_pPresentParams->FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE;
					else
						resetDevice = FALSE;   // Quirk: so a color change alone isn't applied
					if (resetDevice)
						g_pd3dDevice->Reset(g_pPresentParams);
				}
				ShowSavedMessage();
			}
			ClearChanged();
		}
	}
	if (ok)
		m_bModified = FALSE;
	return ok;
}
