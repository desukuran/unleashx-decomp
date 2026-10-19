// Control Panel: Screen Adjust. The page only forwards to CScreenAdjuster,
// the full-screen editor of the visible area.
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"
#include "SettingsPages.h"

// 0x000C1BB8
CScreenAdjust::~CScreenAdjust()
{
	FreePopups();
	// (m_adjuster's destructor runs here.)
}

// 0x000C1B87
HRESULT CScreenAdjust::Init(CXBFont *font)
{
	if (font == NULL || m_pFont != NULL)
		return E_FAIL;
	LoadSounds();
	m_pFont = font;
	return m_adjuster.Init(font);
}

// 0x000C39E6
HRESULT CScreenAdjust::HandleInput(int *action, SGamepadState **pad)
{
	m_adjuster.HandleInput(action, pad);
	// Quirk: m_bClosed gets the editor's *open* flag. Nothing else on this
	// page reads it (Render and IsOpen are the editor's).
	m_bClosed = m_adjuster.m_bOpen;
	// Quirk: returns whatever was left in eax (not a meaningful value).
	return S_OK;
}

// 0x000C05CF
HRESULT CScreenAdjust::Render()
{
	m_adjuster.Render();
	return S_OK;   // (eax is left from the call)
}

// 0x0002DE35
void CScreenAdjust::Reset()
{
	m_adjuster.Reset();
	CSettingsBase::Reset();
}

// 0x0002DE49
BOOL CScreenAdjust::IsOpen()
{
	return m_adjuster.m_bOpen;
}

// 0x0010246E
CScreenAdjuster::CScreenAdjuster()
{
	m_bOpen = TRUE;
	m_bTopLeft = TRUE;
	float width = 0.0f;
	float height = 0.0f;
	GetDisplaySize(&width, &height, FALSE);
	m_nUnknown28 = 0;
	m_nUnknown2C = 0;
	m_nMaxLeft = (int)(width * 0.1f);
	m_nMaxTop = (int)(height * 0.1f);
	m_nMinRight = (int)(width * 0.9f);
	m_nMinBottom = (int)(height * 0.9f);
	m_nMaxRight = (int)width;
	m_nMaxBottom = (int)height;
	m_pMessageBox = NULL;
	m_bChanged = FALSE;
	m_pFont = NULL;
	m_pButtonFont = NULL;
	m_bMessageBoxOpen = FALSE;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00101F35
CScreenAdjuster::~CScreenAdjuster()
{
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
}

// 0x00101F0E
HRESULT CScreenAdjuster::Init(CXBFont *font)
{
	if (m_pFont == NULL)
	{
		m_pFont = font;
		g_fonts.Find("XboxDings", &m_pButtonFont);
	}
	Reset();
	return S_OK;
}

// 0x001018DC
void CScreenAdjuster::Reset()
{
	m_viewport = m_pSettings->m_viewport;
	m_bChanged = FALSE;
	m_bOpen = TRUE;
}

// 0x00101F49
// The same as CSettingsBase::ShowMessage.
void CScreenAdjuster::ShowMessage(const char *text, const wchar_t *title, int buttons)
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
		m_bMessageBoxOpen = TRUE;
	}
}

// 0x00102066
BOOL CScreenAdjuster::Save()
{
	if (!m_bChanged)
		return TRUE;

	BOOL saved = FALSE;
	std::string path = m_pSettings->m_strDataPath;
	path.append("videoset.dat");
	SScreenAdjust *adjust = m_pSettings->GetScreenAdjust();
	FILE *file = NULL;
	if (adjust != NULL)
	{
		// Quirk: the whole viewport is copied, so the mode's font scale
		// (+0x14) gets the viewport's MaxZ.
		memcpy(adjust, &m_pSettings->m_viewport, sizeof(SViewport));
		file = fopen(path.c_str(), "wb+");
	}
	if (file != NULL)
	{
		fwrite(m_pSettings->m_video.m_screenAdjust, sizeof(SScreenAdjust) * SCREEN_MODE_COUNT, 1, file);
		fclose(file);
		saved = TRUE;
	}
	else
	{
		std::string message;
		StrFormat(message, "An error occured while saving %s.\nError Code: %d", "calibration data", GetLastError());
		ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
	}
	return saved;
}

// 0x00102165
HRESULT CScreenAdjuster::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bMessageBoxOpen)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageBoxOpen = m_pMessageBox->IsOpen();
		return S_OK;
	}

	// The sticks, pushed all the way, work like the d-pad; the right one
	// always moves the bottom-right corner.
	BOOL leftStick = TRUE;
	if (pad != NULL)
	{
		if (*action == INPUT_LTHUMB)
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
		else if (*action == INPUT_RTHUMB)
		{
			leftStick = FALSE;
			if (*pad != NULL)
			{
				if ((*pad)->fY2 == 1.0f)
					*action = INPUT_UP;
				else if ((*pad)->fY2 == -1.0f)
					*action = INPUT_DOWN;
			}
			if (*pad != NULL)
			{
				if ((*pad)->fX2 == 1.0f)
					*action = INPUT_RIGHT;
				else if ((*pad)->fX2 == -1.0f)
					*action = INPUT_LEFT;
			}
		}
	}

	SViewport &viewport = m_pSettings->m_viewport;
	BOOL topLeft = m_bTopLeft && leftStick;
	float width = 0.0f;
	float height = 0.0f;
	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		if (Save())
			m_bOpen = FALSE;
		return S_OK;

	case INPUT_B:
	case INPUT_BACK:
		*action = INPUT_NONE;
		viewport = m_viewport;
		m_bOpen = FALSE;
		return S_OK;

	case INPUT_LTRIGGER:
		m_bTopLeft = TRUE;
		GetDisplaySize(&width, &height, FALSE);
		m_nUnknown28 = 0;
		m_nUnknown2C = 0;
		m_nMaxLeft = (int)(width * 0.1f);
		m_nMaxTop = (int)(height * 0.1f);
		return S_OK;

	case INPUT_RTRIGGER:
		m_bTopLeft = FALSE;
		GetDisplaySize(&width, &height, FALSE);
		m_nUnknown28 = (int)(width * 0.9f);
		m_nUnknown2C = (int)(height * 0.9f);
		m_nMaxLeft = (int)width;
		m_nMaxTop = (int)height;
		return S_OK;

	// The top-left corner moves with the size following, so the other
	// corner stays put.
	case INPUT_UP:
		if (topLeft)
		{
			if (viewport.Y <= 0)
				return S_OK;
			viewport.Y--;
			viewport.Height++;
		}
		else
		{
			if (viewport.Y + viewport.Height <= (DWORD)m_nMinBottom)
				return S_OK;
			viewport.Height--;
		}
		break;

	case INPUT_DOWN:
		if (topLeft)
		{
			if (viewport.Y >= (DWORD)m_nMaxTop)
				return S_OK;
			viewport.Y++;
			viewport.Height--;
		}
		else
		{
			if (viewport.Y + viewport.Height >= (DWORD)m_nMaxBottom)
				return S_OK;
			viewport.Height++;
		}
		break;

	case INPUT_LEFT:
		if (topLeft)
		{
			if (viewport.X <= 0)
				return S_OK;
			viewport.X--;
			viewport.Width++;
		}
		else
		{
			if (viewport.X + viewport.Width <= (DWORD)m_nMinRight)
				return S_OK;
			viewport.Width--;
		}
		break;

	case INPUT_RIGHT:
		if (topLeft)
		{
			if (viewport.X >= (DWORD)m_nMaxLeft)
				return S_OK;
			viewport.X++;
			viewport.Width--;
		}
		else
		{
			if (viewport.X + viewport.Width >= (DWORD)m_nMaxRight)
				return S_OK;
			viewport.Width++;
		}
		break;

	default:
		return S_OK;
	}
	m_bChanged = TRUE;
	return S_OK;
}

// 0x001018FC
HRESULT CScreenAdjuster::Render()
{
	const SViewport &viewport = m_pSettings->m_viewport;
	float left = (float)viewport.X;
	float top = (float)viewport.Y;
	float width = (float)viewport.Width;
	float height = (float)viewport.Height;

	DimScreen(0.5f);
	DrawFrame(left, top + 1.0f, width - 1.0f, height - 1.0f, 0xBFFFFFFF, 2);
	m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	float glyphWidth = m_pButtonFont->GetTextWidth(L"E");
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	ScaleToScreen(NULL, &scaleX, NULL, &scaleY, FALSE);

	// The corner being moved.
	if (m_bTopLeft)
	{
		DrawRect(left, top, scaleX * 5.0f, scaleY * 30.0f, 0xFFFFFFFF, 0xFFFFFFFF, 1.0f, 1);
		DrawRect(left, top, scaleX * 30.0f, scaleY * 5.0f, 0xFFFFFFFF, 0xFFFFFFFF, 1.0f, 1);
	}
	else
	{
		DrawRect((float)(viewport.Width + viewport.X - 5), (float)(viewport.Height + viewport.Y - 30), scaleX * 5.0f,
		         scaleY * 30.0f, 0xFFFFFFFF, 0xFFFFFFFF, 1.0f, 1);
		DrawRect((float)(viewport.Width + viewport.X - 30), (float)(viewport.Height + viewport.Y - 5),
		         scaleX * 30.0f, scaleY * 5.0f, 0xFFFFFFFF, 0xFFFFFFFF, 1.0f, 1);
	}

	// The top-left coordinates ("E" is the left trigger glyph).
	wchar_t text[40];
	float x = (float)(viewport.X + 40);
	float y = m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY * 0.5f + (float)(viewport.Y + 40);
	m_pButtonFont->DrawText(x, y, 0xFFFFFFFF, L"E", XBFONT_CENTER_Y, 0.0f);
	swprintf(text, 40, L"(%d, %d)", viewport.X, viewport.Y);
	m_pFont->DrawText(x + glyphWidth, y, 0xFFFFFFFF, text, XBFONT_CENTER_Y, 0.0f);

	// The size and the help, centered.
	y = (float)(viewport.Y + viewport.Height / 3);
	x = (float)(viewport.X + viewport.Width / 2);
	swprintf(text, 40, L"Width=%d , Height=%d", viewport.Width, viewport.Height);
	m_pFont->DrawText(x, y, 0xFFFFFFFF, text, XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
	static const wchar_t *const help[4] = {
		L"Press \"Left Trigger\" to adjust Top and Left coordinates",
		L"Press \"Right Trigger\" to adjust Bottom and Right coordinates",
		L"Press \"A\" or \"Start\" to save settings",
		L"Press \"B\" or \"Back\" to cancel",
	};
	for (int i = 0; i < 4; i++)
	{
		y = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY + y + 2.0f;
		m_pFont->DrawText(x, y, 0xFFFFFFFF, help[i], XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
	}

	// The bottom-right coordinates ("F" is the right trigger glyph).
	x = (width + left) - 40.0f;
	y = (height + top) - 40.0f - m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY * 0.5f;
	m_pButtonFont->DrawText(x, y, 0xFFFFFFFF, L"F", XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);
	swprintf(text, 40, L"(%d, %d)", viewport.X + viewport.Width, viewport.Y + viewport.Height);
	m_pFont->DrawText(x - glyphWidth, y, 0xFFFFFFFF, text, XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);

	if (m_bMessageBoxOpen)
		m_pMessageBox->Render();
	return S_OK;
}
