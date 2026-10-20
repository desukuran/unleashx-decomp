// The Control Panel (Action="Settings"): a list of ten icons from
// CtlPanel.xpr; A opens the page, which then takes the input and draws over
// the panel until it's closed.
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/StrUtil.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"
#include "SettingsPages.h"

// The icon (in CtlPanel.xpr) and caption of each page, in page order.
static const char *const s_iconNames[CSettingsMenu::PAGE_COUNT] = {
	"AutoLaunch", "DateTime", "ShortCut", "DashUpdate", "Network",
	"ScreenAdjust", "Music", "HDD", "System", "Display",
};
static const int s_captions[CSettingsMenu::PAGE_COUNT] = {
	STR_AUTOLAUNCH, STR_DATETIME, STR_SHORTCUT, STR_LIVEUPDATE, STR_NETWORK,
	STR_SCRCALIBRATION, STR_MUSIC, STR_STORAGE, STR_SYSTEM, STR_DISPLAY,
};

// 0x000BEBFD
CSettingsMenu::CSettingsMenu()
{
	m_pSelectedIcon = NULL;
	memset(m_pIcons, 0, sizeof(m_pIcons));
	memset(m_pPages, 0, sizeof(m_pPages));
	m_fX = 10.0f;
	m_fY = 10.0f;
	m_nSelected = 0;
	m_nLastVisible = 0;
	m_pFont = NULL;
	m_pSoundMove = NULL;
	m_pSoundSelect = NULL;
	m_bClosed = FALSE;
	m_nFirstVisible = 0;
	m_nVisibleCount = 0;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_nOpenPage = NO_PAGE;
}

// 0x000BEBCF
CSettingsMenu::~CSettingsMenu()
{
	m_resource.Destroy();
	for (int i = 0; i < PAGE_COUNT; i++)
	{
		if (m_pPages[i] != NULL)
		{
			delete m_pPages[i];
			m_pPages[i] = NULL;
		}
	}
	// (m_resource's destructor runs here.)
}

// 0x000C18A3
HRESULT CSettingsMenu::Init(CXBFont *font)
{
	HRESULT hr = E_FAIL;
	if (font != NULL)
	{
		if (m_pFont == NULL)
		{
			m_pFont = font;
			std::string file = g_pSettings->m_strDataPath;
			StrReplaceAll(file, "U:\\", "E:\\UDATA\\");
			file.append("CtlPanel.xpr");
			if (FAILED(m_resource.Create(file.c_str(), 0, NULL)))
			{
				m_bClosed = TRUE;
			}
			else
			{
				m_fY = 20.0f;
				m_fX = 30.0f;
				m_fWidth = 580.0f;
				m_fHeight = 440.0f;
				m_pSoundMove = g_soundEffects.Find("Scroll");
				m_pSoundSelect = g_soundEffects.Find("Select");
				for (int i = 0; i < PAGE_COUNT; i++)
					m_pIcons[i] = m_resource.GetTexture(s_iconNames[i]);
				m_pSelectedIcon = m_pIcons[0];
				m_bClosed = FALSE;
				hr = S_OK;
				Show();
			}
		}
		else
		{
			m_bClosed = FALSE;
			hr = S_OK;
		}
	}
	m_nSelected = 0;
	return hr;
}

// 0x0002B530
void CSettingsMenu::Show()
{
	m_fY = 20.0f;
	m_fX = 30.0f;
	m_fWidth = 580.0f;
	m_fHeight = 440.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
	m_bClosed = FALSE;
}

// 0x000C62EE
int CSettingsMenu::HandleInput(int *action, SGamepadState **pad)
{
	if (m_nOpenPage != NO_PAGE)
	{
		// The open page has the input until it closes.
		CSettingsBase *page = m_pPages[m_nSelected];
		if (page != NULL)
		{
			page->HandleInput(action, pad);
			if (page->IsOpen())
				return 1;
			// Screen Adjust may have changed the screen area.
			if (m_nOpenPage == 5)
				Show();
		}
		m_nOpenPage = NO_PAGE;
		return 1;
	}

	// The left stick, pushed all the way, works like the d-pad.
	if (*action == INPUT_LTHUMB && pad != NULL)
	{
		if (*pad != NULL)
		{
			if ((*pad)->fY1 == 1.0f)
				*action = INPUT_UP;
			else if ((*pad)->fY1 == -1.0f)
				*action = INPUT_DOWN;
		}
		if (*pad != NULL)
		{
			if ((*pad)->fX1 == 1.0f)
				*action = INPUT_RIGHT;
			else if ((*pad)->fX1 == -1.0f)
				*action = INPUT_LEFT;
		}
	}

	switch (*action)
	{
	case INPUT_A:
	{
		CSettingsBase *&page = m_pPages[m_nSelected];
		if (page == NULL)
		{
			switch (m_nSelected)
			{
			case 0:
				page = new CAutoLaunch;
				break;
			case 1:
				page = new CDateTimeSetting;
				break;
			case 2:
				page = new CQuickLaunch;
				break;
			case 3:
				page = new CLiveUpdate;
				break;
			case 4:
				page = new CNetworkSetting;
				break;
			case 5:
				page = new CScreenAdjust;
				break;
			case 6:
				page = new CMusicSetting;
				break;
			case 7:
				page = new CHDDSetting;
				break;
			case 8:
				page = new CSystemSetting;
				break;
			case 9:
				page = new CVideoSetting;
				break;
			}
			if (page != NULL)
			{
				if (FAILED(page->Init(m_pFont)))
				{
					delete page;
					page = NULL;
				}
				else
				{
					page->m_pBackground = m_pSelectedIcon;
				}
			}
		}
		if (page != NULL)
		{
			page->Reset();
			m_nOpenPage = m_nSelected;
			PlaySound(m_pSoundSelect);
		}
		return 1;
	}

	case INPUT_B:
	case INPUT_BACK:
		*action = INPUT_NONE;
		m_bClosed = TRUE;
		return 1;

	case INPUT_LTRIGGER:
	case INPUT_UP:
		if (m_nSelected == 0)
			return 1;
		m_nSelected--;
		break;

	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		if (m_nSelected > 8)
			return 1;
		m_nSelected++;
		break;

	default:
		return 1;
	}

	// Moved: scroll the list so the selection is shown.
	PlaySound(m_pSoundMove);
	if (m_nVisibleCount != 0)
	{
		if (m_nSelected < m_nFirstVisible)
		{
			m_nFirstVisible = m_nSelected;
			m_nLastVisible = m_nVisibleCount - 1 + m_nSelected;
			if (m_nLastVisible > 9)
			{
				m_nLastVisible = 9;
				m_nFirstVisible = 10 - m_nVisibleCount;
			}
		}
		if (m_nLastVisible < m_nSelected)
		{
			m_nLastVisible = m_nSelected;
			m_nFirstVisible = m_nSelected - m_nVisibleCount;
			if (m_nFirstVisible < 0)
			{
				m_nFirstVisible = 0;
				m_nLastVisible = m_nVisibleCount - 1;
			}
		}
	}
	m_pSelectedIcon = m_pIcons[m_nSelected];
	return 1;
}

// 0x000C5E8A
HRESULT CSettingsMenu::Render()
{
	if (m_bClosed)
		return 1;

	DimScreen(0.3f);
	// Screen Adjust draws its own full screen.
	if (m_nOpenPage != 5)
	{
		DrawRect(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
		         1.0f, 1);
		m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
		DrawTexture(m_pSelectedIcon, m_fX + 2.0f, m_fY + 2.0f, m_fWidth - 4.0f, m_fHeight - 4.0f, 0.12f, NULL, 0.0f,
		            NULL);

		float iconSize = 32.0f;
		ScaleToScreen(NULL, &iconSize, NULL, NULL, FALSE);
		float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
		float x = m_fX + 8.0f;
		float y = m_fY + 6.0f;
		float right = m_fWidth + m_fX - 4.0f;
		float bottom = m_fY + m_fHeight - iconSize;

		// The caption.
		DrawRect(m_fX + 2.0f, m_fY + 2.0f, m_fWidth - 4.0f, lineHeight + 8.0f, g_colorScheme.DIALOG_BORDERCOLOR,
		         g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
		y = lineHeight * 0.5f + y;
		m_pFont->DrawText(x, y, g_colorScheme.CAPTION_TEXTCOLOR, L"Control Panel", XBFONT_CENTER_Y, 0.0f);
		y = lineHeight * 0.5f + 10.0f + y;

		// The icons with their captions.
		wchar_t caption[42];
		for (int i = m_nFirstVisible; i < PAGE_COUNT; i++)
		{
			wcsncpy(caption, GetString(s_captions[i]), 40);
			if (i == m_nSelected)
			{
				DrawGradientRect(x - 2.0f, y - 2.0f, right - x, iconSize + 4.0f, g_colorScheme.OPTION_ACTIVEBACKCOLOR,
				                 g_colorScheme.DIALOG_BACKCOLOR, FALSE, g_colorScheme.OPTION_ACTIVEBACKCOLOR);
			}
			DrawTexture(m_pIcons[i], x, y, iconSize, iconSize, 1.0f, NULL, 0.0f, NULL);
			DWORD color = i == m_nSelected ? g_colorScheme.OPTION_ACTIVETEXTCOLOR : g_colorScheme.OPTION_TEXTCOLOR;
			m_pFont->DrawText(x + iconSize + 8.0f, iconSize * 0.5f + y, color, caption, XBFONT_CENTER_Y, 0.0f);
			y = iconSize + 8.0f + y;
			if (y > bottom)
			{
				if (m_nVisibleCount == 0)
					m_nVisibleCount = i - m_nFirstVisible;
				break;
			}
		}
	}
	if (m_nOpenPage != NO_PAGE)
		m_pPages[m_nSelected]->Render();
	return S_OK;
}
