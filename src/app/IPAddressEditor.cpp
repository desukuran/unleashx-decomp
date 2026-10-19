// CIPAddressEditor: the settings pages' IP address pop-up. Four spinners
// (0-255) separated by dots, with A Accept / B Cancel at the bottom.
#include <cstdlib>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "Screens.h"

// 0x000C662B
CIPAddressEditor::CIPAddressEditor()
{
	m_strTitle = L"";
	m_strAddress = "0.0.0.0";
	m_pFont = NULL;
	m_pButtonFont = NULL;
	m_bInitialized = FALSE;
	m_bOpen = FALSE;
	m_nFocus = 0;
	m_pSoundMove = NULL;
	m_pSoundAccept = NULL;
	m_pSoundBack = NULL;
	m_bCancelled = FALSE;
}

// 0x000C0AB6
CIPAddressEditor::~CIPAddressEditor()
{
	m_pFont = NULL;
	m_pButtonFont = NULL;
	m_bInitialized = FALSE;
	m_bOpen = FALSE;
	m_pSoundMove = NULL;
	m_pSoundAccept = NULL;
	m_pSoundBack = NULL;
	m_strTitle.clear();
}

// 0x000C1D45
HRESULT CIPAddressEditor::Init(CXBFont *font)
{
	g_fonts.Find("MainFont", &m_pFont);
	g_fonts.Find("XboxDings", &m_pButtonFont);
	if (m_pFont == NULL)
		m_pFont = font;
	if (m_bInitialized || m_pFont == NULL)
		return E_FAIL;

	m_nFocus = 0;
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	for (int i = 0; i < 4; i++)
	{
		for (int n = 0; n < 256; n++)
		{
			wchar_t text[6];
			swprintf(text, 6, L"%d", n);
			m_spinners[i].AddItem(text);
		}
	}

	m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
	float y = lineHeight + 80.0f;
	ScaleToScreen(NULL, NULL, &y, NULL, FALSE);
	float dotWidth = m_pFont->GetTextWidth(L".") + 4.0f;
	float width = dotWidth * 3.0f;
	for (int i = 0; i < 4; i++)
	{
		m_spinners[i].Create(NULL, 100.0f, y, 0.0f, 0.0f);
		width = m_spinners[i].m_fWidth + width;
	}
	m_spinners[0].m_bFocused = TRUE;

	// Centered (Show redoes all this with the font scale).
	D3DDISPLAYMODE mode;
	g_pd3dDevice->GetDisplayMode(&mode);
	float x = ((float)mode.Width - width) * 0.5f;
	m_spinners[0].m_fX = x;
	m_fX = x - 30.0f;
	m_spinners[1].m_fX = x + dotWidth + m_spinners[0].m_fWidth;
	m_spinners[2].m_fX = m_spinners[1].m_fX + m_spinners[1].m_fWidth + dotWidth;
	m_spinners[3].m_fX = m_spinners[2].m_fX + m_spinners[2].m_fWidth + dotWidth;
	m_fWidth = width + 60.0f;
	m_fY = 40.0f;
	m_fHeight = m_spinners[0].m_fHeight + y + 40.0f;

	m_pSoundMove = g_soundEffects.Find("Scroll");
	m_pSoundAccept = g_soundEffects.Find("Select");
	m_pSoundBack = g_soundEffects.Find("Back");
	m_bInitialized = TRUE;
	Show();
	return S_OK;
}

// 0x000C066E
void CIPAddressEditor::Show()
{
	m_bCancelled = FALSE;
	m_bOpen = TRUE;
	m_spinners[m_nFocus].m_bFocused = FALSE;
	m_nFocus = 0;
	if (m_strAddress.size() == 0)
		m_strAddress = "0.0.0.0";
	if (m_strAddress.size() != 0)
	{
		// One number per spinner (ones over 255 show as 255; ones that don't
		// parse as 0).
		std::string address = m_strAddress;
		address.append(".");
		int pos = 0;
		for (int i = 0; i < 4; i++)
		{
			int dot = StrFind(address, ".", pos);
			if (dot == -1)
				continue;
			unsigned value = (unsigned)atoi(StrMid(address, pos, dot - pos).c_str());
			pos = dot + 1;
			if (value > 255)
				value = 255;
			if ((int)value < (int)m_spinners[i].m_items.size())
				m_spinners[i].m_nSelected = (int)value;
		}
	}
	m_spinners[m_nFocus].m_bFocused = TRUE;

	// The layout, centered.
	m_fY = 40.0f;
	m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	ScaleToScreen(NULL, &scaleX, &m_fY, &scaleY, FALSE);
	if (1.0f > scaleX)
		scaleX = 1.0f;
	if (1.0f > scaleY)
		scaleY = 1.0f;
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	float spinnerY = scaleY * 40.0f + m_fY + lineHeight;
	float dotWidth = m_pFont->GetTextWidth(L".") + scaleX * 4.0f;
	float width = dotWidth * 3.0f;
	for (int i = 0; i < 4; i++)
	{
		m_spinners[i].m_fY = spinnerY;
		width = m_spinners[i].GetBoxWidth() + width;
	}
	float screenWidth;
	GetScreenSize(&screenWidth, NULL);
	float x = (screenWidth - width) * 0.5f + (float)g_pSettings->m_viewport.X;
	m_spinners[0].m_fX = x;
	m_spinners[1].m_fX = m_spinners[0].GetBoxWidth() + x + dotWidth;
	m_spinners[2].m_fX = m_spinners[1].GetBoxWidth() + m_spinners[1].m_fX + dotWidth;
	m_spinners[3].m_fX = m_spinners[2].GetBoxWidth() + m_spinners[2].m_fX + dotWidth;
	m_fX = x - scaleX * 30.0f;
	m_fWidth = scaleX * 60.0f + width;
	float buttonHeight = lineHeight;
	if (m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY > buttonHeight)
		buttonHeight = m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY;
	m_fHeight = m_spinners[0].GetBoxHeight() + scaleY * 10.0f + buttonHeight + spinnerY;
}

// 0x000C1A55
void CIPAddressEditor::SetAddress(const wchar_t *address)
{
	if (address != NULL && wcslen(address) != 0)
	{
		// (Not terminated by the conversion: an address under 16 characters
		// is followed by whatever the buffer held.)
		char text[16];
		WideCharToMultiByte(0, 0, address, (int)wcslen(address), text, 16, NULL, NULL);
		if (m_strAddress.compare(text) == 0)
			return;
		m_strAddress = text;
	}
	else
	{
		if (m_strAddress.size() != 0)
			return;
		m_strAddress = "0.0.0.0";
	}
	if (m_bInitialized)
		Show();
}

// 0x000BFC30
void CIPAddressEditor::GetAddress(wchar_t *address)
{
	swprintf(address, 16, L"%s.%s.%s.%s", m_spinners[0].GetText(), m_spinners[1].GetText(), m_spinners[2].GetText(),
	         m_spinners[3].GetText());
}

// 0x000C09EE
HRESULT CIPAddressEditor::ProcessInput(int *action, SGamepadState **pad)
{
	CSound *sound;
	switch (*action)
	{
	case INPUT_A:
		m_bCancelled = FALSE;
		sound = m_pSoundAccept;
		break;

	case INPUT_B:
	case INPUT_BACK:
		Show();   // puts the spinners back to the address
		m_bCancelled = TRUE;
		sound = m_pSoundBack;
		break;

	case INPUT_LEFT:
		m_spinners[m_nFocus].m_bFocused = FALSE;
		m_nFocus = m_nFocus == 0 ? 3 : m_nFocus - 1;
		m_spinners[m_nFocus].m_bFocused = TRUE;
		PlaySound(m_pSoundMove);
		// (And the spinner gets the input too.)
		m_spinners[m_nFocus].ProcessInput(action, pad);
		return S_OK;

	case INPUT_RIGHT:
		m_spinners[m_nFocus].m_bFocused = FALSE;
		m_nFocus = m_nFocus == 3 ? 0 : m_nFocus + 1;
		m_spinners[m_nFocus].m_bFocused = TRUE;
		PlaySound(m_pSoundMove);
		m_spinners[m_nFocus].ProcessInput(action, pad);
		return S_OK;

	default:
		m_spinners[m_nFocus].ProcessInput(action, pad);
		return S_OK;
	}

	// Closed.
	m_bOpen = FALSE;
	PlaySound(sound);
	*action = INPUT_NONE;
	return S_OK;
}

// 0x000BF86F
HRESULT CIPAddressEditor::Render()
{
	if (!m_bInitialized)
		return E_FAIL;
	if (!m_bOpen)
		return S_OK;

	m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	float buttonHeight = lineHeight;
	if (!(buttonHeight > m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY))
		buttonHeight = m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY;
	DrawWindowFrame(m_fX, m_fY, m_fWidth, m_fHeight, 10.0f, lineHeight, buttonHeight + 10.0f,
	                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	                g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 2);
	for (int i = 0; i < 4; i++)
		m_spinners[i].Render();

	float dotY = m_spinners[0].GetBoxHeight() * 0.5f + m_spinners[0].m_fY;
	m_pFont->Begin();
	m_pFont->DrawText(m_fX + 10.0f, (lineHeight + 10.0f) * 0.5f + m_fY, g_colorScheme.CAPTION_TEXTCOLOR,
	                  m_strTitle.c_str(), XBFONT_CENTER_Y, 0.0f);
	// The dots, after spinners 0-2 (all placed using spinner 0's width).
	for (int i = 0; i < 3; i++)
	{
		m_pFont->DrawText(m_spinners[0].GetBoxWidth() + m_spinners[i].m_fX + 2.0f, dotY, g_colorScheme.DIALOG_TEXTCOLOR,
		                  L".", XBFONT_CENTER_Y, 0.0f);
	}
	m_pFont->End();

	// A Accept under the first spinner, B Cancel from the middle.
	float glyphWidth = m_pButtonFont->GetTextWidth(L"A") + 5.0f;
	buttonHeight = lineHeight;
	if (!(buttonHeight > m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY))
		buttonHeight = m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY;
	float buttonY = (m_fY + m_fHeight) - buttonHeight * 0.5f - 5.0f;
	m_pButtonFont->DrawText(m_spinners[0].m_fX, buttonY, 0xFFFFFFFF, L"A", XBFONT_CENTER_Y, 0.0f);
	m_pFont->DrawText(m_spinners[0].m_fX + glyphWidth, buttonY, g_colorScheme.DIALOG_TEXTCOLOR, L"Accept",
	                  XBFONT_CENTER_Y, 0.0f);
	m_pButtonFont->DrawText(m_fWidth * 0.5f + m_fX, buttonY, 0xFFFFFFFF, L"B", XBFONT_CENTER_Y, 0.0f);
	m_pFont->DrawText(m_fWidth * 0.5f + m_fX + glyphWidth, buttonY, g_colorScheme.DIALOG_TEXTCOLOR, L"Cancel",
	                  XBFONT_CENTER_Y, 0.0f);
	return S_OK;
}
