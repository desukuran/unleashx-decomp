// CPassCode: the passcode pad (the startup passcode and locked menu items).
// See MenuDialogs.h.
#include <cctype>
#include <cmath>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "MenuDialogs.h"

// The entry's limit (the stored code's is 15, see SetCode).
#define PASSCODE_MAX_INPUT 16

// 0x0010302B
CPassCode::CPassCode() : m_md5(NULL, -1)
{
	m_pFont = NULL;
	m_pSymbols = NULL;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_nCursor = 0;
	m_fBoxHeight = 0.0f;
	m_fTextWidth = 0.0f;
	m_fTitleHeight = 0.0f;
	m_bCancelled = FALSE;
	m_bClosed = FALSE;
	m_cursorTimer.Resume();
	m_bClickSound = FALSE;
	m_fLineHeight = 42.0f;
	m_strTitle = L"Passcode";
	m_fScaleX = 1.0f;
	m_fScaleY = 1.0f;
}

// 0x000264E2
CPassCode::~CPassCode()
{
}

// 0x00102757
void CPassCode::PlayClick()
{
	PlaySound(m_pClickSound);
}

// 0x0002209E
void CPassCode::SetTitle(const wchar_t *title)
{
	m_strTitle = title;
	if (m_strTitle.size() == 0)
		m_fTitleHeight = 0.0f;
}

// 0x0010341C
HRESULT CPassCode::Init(CXBFont *font, CXBFont *symbols)
{
	HRESULT hr = E_FAIL;
	if (font != NULL && m_pFont == NULL)
	{
		m_fWidth = 640.0f;
		m_fHeight = 480.0f;
		m_pFont = font;
		if (symbols == NULL)
			g_fonts.Find("XboxDings", &m_pSymbols);
		else
			m_pSymbols = symbols;
		m_pClickSound = g_soundEffects.Find("Keyboard Stroke");
		m_bClickSound = m_pClickSound != NULL;
		hr = S_OK;
	}
	Show();
	return hr;
}

// 0x00103100
void CPassCode::SetCode(const char *code)
{
	m_strCode.clear();
	if (code == NULL || strlen(code) == 0)
		return;
	char *upper = _strdup(code);   // (leaked)
	for (char *p = upper; *p != '\0'; p++)
		*p = (char)toupper((unsigned char)*p);
	m_strCode = upper;
	if (m_strCode.size() > 15)
		m_strCode = StrLeft(m_strCode, 15);
}

// 0x00102805
// Sized to the screen's scale: the entry field (at least 390 wide), the
// help lines and the Accept/Cancel buttons; centered.
void CPassCode::Show()
{
	m_fScaleX = 1.0f;
	m_fScaleY = 1.0f;
	ScaleToScreen(NULL, &m_fScaleX, NULL, &m_fScaleY, TRUE);
	if (m_fScaleX < 1.0f)
		m_fScaleX = 1.0f;
	if (m_fScaleY < 1.0f)
		m_fScaleY = 1.0f;
	float screenWidth = 640.0f;
	float screenHeight = 480.0f;
	GetScreenSize(&screenWidth, &screenHeight);
	m_fWidth = screenWidth;
	m_fHeight = screenHeight;
	m_pSymbols->SetScaleFactors(m_fScaleX, m_fScaleY);
	float symbolLine = m_pSymbols->m_fScaleFactorY * m_pSymbols->m_fFontHeight;
	m_fLineHeight = symbolLine + m_fScaleY * 12.0f;
	m_fBoxHeight = m_fScaleY * 10.0f + m_fLineHeight;
	m_fTextWidth = m_pSymbols->GetTextWidth(L"*****************");
	m_pFont->SetScaleFactors(m_fScaleX, m_fScaleY);
	if (m_fTextWidth < 390.0f)
		m_fTextWidth = 390.0f;
	m_fWidth = m_fTextWidth + 20.0f;
	float fontLine = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	m_fHeight = fontLine + m_fBoxHeight + 25.0f;
	m_fHeight = symbolLine * 2.0f + m_fHeight + 10.0f;
	m_fHeight = fontLine + m_fHeight + 50.0f;
	float originX;
	float originY;
	GetScreenOrigin(&originX, &originY);
	m_bClosed = FALSE;
	m_bCancelled = FALSE;
	m_fX = (screenWidth - m_fWidth) * 0.5f + originX;
	m_fY = (screenHeight - m_fHeight) * 0.5f + originY;
	m_strInput.erase(0, std::wstring::npos);
	m_nCursor = 0;
	if (m_strTitle.size() != 0)
		m_fTitleHeight = fontLine * m_fScaleY + 8.0f;
}

// 0x00102A03
// Every action is consumed.
HRESULT CPassCode::ProcessInput(int *action, SGamepadState **pad)
{
	wchar_t ch;
	switch (*action)
	{
	case INPUT_A:
		ch = L'A';
		// The remote's SELECT comes as A too: it accepts.
		if (pad == NULL || ((CGamepadInput *)pad)->GetRemoteAction() != INPUT_IR_SELECT)
			break;
		// fall through
	case INPUT_START:
	case INPUT_IR_SELECT:
		m_bCancelled = FALSE;
		m_bClosed = TRUE;
		PlayClick();
		*action = INPUT_NONE;
		return S_OK;
	case INPUT_B:
		ch = L'B';
		break;
	case INPUT_BACK:
	case INPUT_IR_BACK:
		m_bClosed = TRUE;
		m_bCancelled = TRUE;
		PlayClick();
		*action = INPUT_NONE;
		return S_OK;
	case INPUT_X:
		ch = L'X';
		break;
	case INPUT_Y:
		ch = L'Y';
		break;
	case INPUT_WHITE:
		ch = L'W';
		break;
	case INPUT_BLACK:
		// Deletes before the cursor (at the start: the first press).
		if (m_nCursor == 0)
		{
			if (m_strInput.size() == 0)
			{
				*action = INPUT_NONE;
				return S_OK;
			}
		}
		else
		{
			m_nCursor--;
		}
		m_strInput.erase(m_nCursor, 1);
		PlayClick();
		*action = INPUT_NONE;
		return S_OK;
	case INPUT_LTRIGGER:
		if (m_nCursor != 0)
			m_nCursor--;
		*action = INPUT_NONE;
		return S_OK;
	case INPUT_RTRIGGER:
		if (m_nCursor < m_strInput.size())
			m_nCursor++;
		*action = INPUT_NONE;
		return S_OK;
	case INPUT_UP:
	case INPUT_IR_UP:
		ch = L'U';
		break;
	case INPUT_DOWN:
	case INPUT_IR_DOWN:
		ch = L'D';
		break;
	case INPUT_LEFT:
	case INPUT_IR_LEFT:
		ch = L'L';
		break;
	case INPUT_RIGHT:
	case INPUT_IR_RIGHT:
		ch = L'R';
		break;
	case INPUT_IR_0:
		ch = L'0';
		break;
	default:
		if (*action >= INPUT_IR_1 && *action <= INPUT_IR_1 + 8)
		{
			ch = (wchar_t)(L'1' + (*action - INPUT_IR_1));
			break;
		}
		*action = INPUT_NONE;
		return S_OK;
	}
	if (m_strInput.size() < PASSCODE_MAX_INPUT)
	{
		m_strInput.insert(m_nCursor, 1, ch);
		m_nCursor++;
		PlayClick();
	}
	*action = INPUT_NONE;
	return S_OK;
}

// 0x00021FCE
// Quirk: the MD5 comparison can't match, as the stored code is at most 15
// upper-case characters and the digest 32 lower-case ones.
BOOL CPassCode::IsCorrect()
{
	if (m_bCancelled)
		return FALSE;
	m_strEntered = StrFromWide(m_strInput.c_str());
	if (m_strCode.compare(m_strEntered) == 0)
		return TRUE;
	BOOL correct = FALSE;
	if (m_md5.HashMemory(m_strEntered.c_str(), -1))
	{
		std::string digest = m_md5.m_szHex;
		correct = m_strCode.compare(digest) == 0;
	}
	return correct;
}

// 0x00102765
void CPassCode::DrawTextBox()
{
	DrawRect(m_fX + 10.0f, m_fY + 10.0f, m_fTextWidth, m_fBoxHeight, g_colorScheme.KB_KEY_TEXTBORDERCOLOR,
	         g_colorScheme.KB_KEY_TEXTBACKCOLOR, 1.0f, 1);
}

// 0x001027B8
void CPassCode::DrawFrame()
{
	DrawRect(m_fX, m_fY - m_fTitleHeight, m_fWidth, m_fHeight + m_fTitleHeight, g_colorScheme.DIALOG_BORDERCOLOR,
	         g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
}

// 0x00102C48
// The title bar, the entry as stars (button glyph font) and the blinking
// cursor.
void CPassCode::DrawText(float x, float y)
{
	if (m_strTitle.size() != 0)
	{
		DrawRect(m_fX + 3.0f, (m_fY - m_fTitleHeight) + 3.0f, m_fWidth - 6.0f, m_fTitleHeight,
		         g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.CAPTION_BACKCOLOR, 1.0f, 1);
		m_pFont->DrawText(m_fX + 5.0f, (m_fY + 3.0f) - m_fTitleHeight * 0.5f, g_colorScheme.CAPTION_TEXTCOLOR,
		                  m_strTitle.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - 10.0f);
	}
	wchar_t stars[16];
	memset(stars, 0, sizeof(stars));
	if (m_strInput.size() != 0)
	{
		UINT count = m_strInput.size() < 16 ? (UINT)m_strInput.size() : 15;
		wcsncpy(stars, m_strInput.substr(0, count).c_str(), count);
		for (UINT i = 0; i < count; i++)
			stars[i] = L'*';
		m_pSymbols->DrawText(x, y, g_colorScheme.KB_KEY_TEXTCOLOR, stars, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
		                     m_fTextWidth);
	}
	if (fmodf(m_cursorTimer.GetElapsedSeconds(), 1.0f) >= 0.75f)
		return;
	float offset = 0.0f;
	if (m_nCursor != 0)
	{
		std::wstring prefix(stars, m_nCursor < 15 ? m_nCursor : 15);
		m_pSymbols->GetTextExtent(prefix.c_str(), &offset, NULL, FALSE);
		x += offset;
	}
	IDirect3DDevice8 *device = g_pd3dDevice;
	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.KB_KEY_TEXTCOLOR);
	y -= m_fLineHeight * 0.5f;
	float half = m_fScaleX * 2.0f;
	float top = y + 2.0f;
	float bottom = y + m_fLineHeight;
	device->Begin(D3DPT_LINELIST);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - half, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x + half, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x, bottom, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - 1.0f, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - 1.0f, bottom, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - half, bottom, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x + half, bottom, 1.0f, 1.0f);
	device->End();
}

// 0x0010317B
// The dimmed screen, the box, the entry and the help: which buttons to use
// (as XboxDings glyphs), and the Accept (START, "G") and Cancel (BACK,
// "H") buttons.
HRESULT CPassCode::Render()
{
	if (IsClosed())
		return S_OK;
	m_pSymbols->SetScaleFactors(m_fScaleX, m_fScaleY);
	m_pFont->SetScaleFactors(m_fScaleX, m_fScaleY);
	DimScreen(0.3f);
	DrawFrame();
	DrawTextBox();
	DrawText(m_fX + 12.0f, m_fBoxHeight * 0.5f + m_fY + 10.0f);
	float y = m_fBoxHeight + m_fY + 20.0f;
	float centerX = m_fWidth * 0.5f + m_fX;
	m_pFont->DrawText(centerX, y, g_colorScheme.DIALOG_TEXTCOLOR, L"Use the Following Gamepad buttons", XBFONT_CENTER_X,
	                  0.0f);
	y = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight + y + 5.0f;
	if (m_pSymbols != NULL)
	{
		m_pSymbols->DrawText(centerX, y, 0xFFFFFFFF, L"A B C D I K M O Q", XBFONT_CENTER_X, 0.0f);
		y = m_pSymbols->m_fScaleFactorY * m_pSymbols->m_fFontHeight + y + 10.0f;
	}
	m_pFont->DrawText(centerX, y, g_colorScheme.DIALOG_TEXTCOLOR,
	                  L"You can also use directional and 0-9 buttons on the IR", XBFONT_CENTER_X, 0.0f);
	if (m_pSymbols != NULL)
	{
		float glyphWidth = m_pSymbols->GetTextWidth(L"G");
		float x = m_fWidth * 0.2f + m_fX + 10.0f;
		float buttonY = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight + y + 50.0f;
		m_pSymbols->DrawText(x, buttonY, 0xFFFFFFFF, L"G", XBFONT_CENTER_Y, 0.0f);
		m_pFont->DrawText(glyphWidth + x, buttonY, g_colorScheme.DIALOG_TEXTCOLOR, L"Accept", XBFONT_CENTER_Y, 0.0f);
		x = m_fWidth * 0.4f + x;
		m_pSymbols->DrawText(x, buttonY, 0xFFFFFFFF, L"H", XBFONT_CENTER_Y, 0.0f);
		m_pFont->DrawText(glyphWidth + x, buttonY, g_colorScheme.DIALOG_TEXTCOLOR, L"Cancel", XBFONT_CENTER_Y, 0.0f);
	}
	return S_OK;
}
