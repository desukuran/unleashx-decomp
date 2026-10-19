// CXBFontEx: the XDK sample's bitmap font, or a TrueType font drawn with
// XFONT straight onto the back buffer (the skin's <MenuFont> .ttf). Every
// method checks for the TrueType font first and otherwise does what
// CXBFont does. Also the free helpers the menus draw with.
#include "Fonts.h"

#include <cmath>
#include <cwchar>

#include "Texture.h"
#include "XFont.h"

// What the average character width is measured on.
static const wchar_t s_szMeasureText[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";
#define MEASURE_TEXT_LENGTH 62

// The XFONT glyph cache size.
#define TRUETYPE_CACHE_SIZE 0x4000

// 0x00137887
CXBFontEx::CXBFontEx()
{
	m_pTrueTypeFont = NULL;
	m_unknownC0 = 0;
	m_dwTextHeight = 0;
	m_pBackBuffer = NULL;
	m_fScaleFactorX = 1.0f;
	m_fScaleFactorY = 1.0f;
}

// 0x00137A8F
HRESULT CXBFontEx::CreateFromMemory(const void *data)
{
	if (FAILED(m_xpr.CreateFromMemory(data, 2, NULL)))
		return E_FAIL;
	// The font texture comes first, its glyph data after its header.
	IDirect3DTexture8 *texture = (IDirect3DTexture8 *)m_xpr.GetResource(0);
	return Create(texture, m_xpr.m_pSysMemData + sizeof(D3DResource));
}

// 0x00137841
HRESULT CXBFontEx::CreateFromTTF(const wchar_t *fileName, D3DCOLOR color, DWORD height, DWORD antialiasLevel)
{
	if (XFONT_OpenTrueTypeFont(fileName, TRUETYPE_CACHE_SIZE, &m_pTrueTypeFont) < 0)
		return E_FAIL;
	XFONT_SetTextAntialiasLevel(m_pTrueTypeFont, antialiasLevel);
	SetTextHeight(height);
	SetTextColor(color);
	return S_OK;
}

// 0x001377E1
HRESULT CXBFontEx::SetTextHeight(DWORD height)
{
	if (m_dwTextHeight != height)
	{
		m_dwTextHeight = height;
		if (m_pTrueTypeFont == NULL)
			return CXBFont::SetTextHeight(height);
		XFONT_SetTextHeight(m_pTrueTypeFont, height);
		unsigned width = 0;
		XFONT_GetTextExtent(m_pTrueTypeFont, s_szMeasureText, MEASURE_TEXT_LENGTH, &width);
		m_dwAverageCharWidth = width / MEASURE_TEXT_LENGTH;
	}
	return S_OK;
}

// 0x001377AF
HRESULT CXBFontEx::SetTextColor(D3DCOLOR color)
{
	if (m_dwTrueTypeColor != color)
	{
		m_dwTrueTypeColor = color;
		if (m_pTrueTypeFont == NULL)
			return CXBFont::SetTextColor(color);
		XFONT_SetTextColor(m_pTrueTypeFont, color);
	}
	return S_OK;
}

// 0x00031CCB
HRESULT CXBFontEx::Begin()
{
	if (m_pTrueTypeFont == NULL)
		return CXBFont::Begin();
	if (m_dwNestedBeginCount == 0)
		g_pd3dDevice->GetBackBuffer(0, 0, &m_pBackBuffer);
	m_dwNestedBeginCount++;
	return S_OK;
}

// 0x00031CFD
HRESULT CXBFontEx::End()
{
	if (m_pTrueTypeFont == NULL)
		return CXBFont::End();
	if (m_dwNestedBeginCount == 0)
		return E_FAIL;
	if (--m_dwNestedBeginCount != 0)
		return S_FALSE;
	m_pBackBuffer->Release();
	return S_OK;
}

// 0x00031D3C
// TrueType: XFONT measures the whole text, but that result is written as
// an integer into the float width and then replaced: the width is the
// longest line in average character widths plus the character spacing
// (a newline counts as a character), the height a cell height per line.
HRESULT CXBFontEx::GetTextExtent(const wchar_t *text, float *width, float *height, BOOL firstLineOnly)
{
	if (m_pTrueTypeFont == NULL)
		return CXBFont::GetTextExtent(text, width, height, firstLineOnly);
	if (text == NULL || width == NULL || height == NULL)
		return E_INVALIDARG;
	*width = 0.0f;
	*height = 0.0f;
	unsigned length = (unsigned)wcslen(text);
	XFONT_GetTextExtent(m_pTrueTypeFont, text, length, (unsigned *)width);
	unsigned cellHeight, descent;
	XFONT_GetFontMetrics(m_pTrueTypeFont, &cellHeight, &descent);
	float lineHeight = (float)(cellHeight + descent);
	float totalHeight = lineHeight;
	float lineWidth = 0.0f;
	while (length != 0)
	{
		wchar_t c = *text++;
		length--;
		if (c == L'\n')
		{
			if (firstLineOnly)
				break;
			lineWidth = 0.0f;
			totalHeight += lineHeight;
		}
		lineWidth += (float)m_dwAverageCharWidth;
		lineWidth += (float)XFONT_GetIntercharacterSpacing(m_pTrueTypeFont);
		if (*width < lineWidth)
			*width = lineWidth;
		if (*height < totalHeight)
			*height = totalHeight;
	}
	return S_OK;
}

// 0x001378CC
// TrueType: XBFONT_TRUNCATED only decides whether the text is clipped at
// maxWidth (when it's wider); there's no ellipsis.
HRESULT CXBFontEx::DrawTextN(float x, float y, DWORD color, const wchar_t *text, int length, DWORD flags,
                             float maxWidth)
{
	if (m_pTrueTypeFont == NULL)
		return CXBFont::DrawTextN(x, y, color, text, length, flags, maxWidth);
	Begin();
	SetTextColor(color);
	m_fCursorX = (float)floor(x);
	m_fCursorY = (float)floor(y);
	if (flags & XBFONT_TRUNCATED)
	{
		BOOL tooWide = FALSE;
		if (maxWidth > 0.0f)
		{
			float width, height;
			GetTextExtent(text, &width, &height, TRUE);
			if (maxWidth < width)
				tooWide = TRUE;
			else
				maxWidth = 0.0f;
		}
		if (!tooWide)
			flags &= ~XBFONT_TRUNCATED;
	}
	if (flags & XBFONT_CENTER_Y)
	{
		float width, height;
		GetTextExtent(text, &width, &height, FALSE);
		m_fCursorY = (float)floor(m_fCursorY - height * 0.5f);
	}
	if (flags & XBFONT_RIGHT)
		XFONT_SetTextAlignment(m_pTrueTypeFont, XFONT_RIGHT);
	else if (flags & XBFONT_CENTER_X)
		XFONT_SetTextAlignment(m_pTrueTypeFont, XFONT_CENTER);
	else
		XFONT_SetTextAlignment(m_pTrueTypeFont, XFONT_LEFT);
	if (maxWidth > 0.0f)
	{
		// Quirk: the bottom edge is the text height, not the cursor's y
		// plus it. TODO: verify how XFONT clips with it
		m_clipRect.x1 = (long)m_fCursorX;
		m_clipRect.y1 = (long)m_fCursorY;
		m_clipRect.x2 = (long)(maxWidth + m_fCursorX);
		m_clipRect.y2 = (long)m_dwTextHeight;
		XFONT_SetClippingRectangle(m_pTrueTypeFont, &m_clipRect);
	}
	else
	{
		XFONT_SetClippingRectangle(m_pTrueTypeFont, NULL);
	}
	XFONT_TextOut(m_pTrueTypeFont, m_pBackBuffer, text, length, (long)m_fCursorX, (long)m_fCursorY);
	End();
	return S_OK;
}

// The helpers below take any font. Quirk: the plain CXBFonts a font table
// loads are only 0xA4 bytes, so for them the TrueType pointer is read past
// the object.

// 0x00021EA0
float GetFontHeight(CXBFont *font)
{
	CXBFontEx *fontEx = (CXBFontEx *)font;
	if (fontEx->m_pTrueTypeFont == NULL)
		return font->m_fScaleFactorY * font->m_fFontHeight;
	return (float)XFONT_GetTextHeight(fontEx->m_pTrueTypeFont);
}

// 0x00021E58
void SetFontCursor(CXBFont *font, float x, float y)
{
	CXBFontEx *fontEx = (CXBFontEx *)font;
	if (fontEx->m_pTrueTypeFont == NULL)
	{
		font->SetCursorPosition(x, y);
		return;
	}
	font->m_fCursorX = (float)floor(x);
	font->m_fCursorY = (float)floor(y);
}

// 0x00021ECB
void DrawFontText(CXBFont *font, DWORD color, const wchar_t *text, DWORD flags, float maxWidth)
{
	CXBFontEx *fontEx = (CXBFontEx *)font;
	fontEx->DrawTextN(font->m_fCursorX, font->m_fCursorY, color, text, (int)wcslen(text), flags, maxWidth);
}

// 0x0002AED1
void DrawTextCentered(CXBFont *font, float x, float y, const wchar_t *text, D3DCOLOR color, DWORD flags)
{
	CXBFontEx *fontEx = (CXBFontEx *)font;
	fontEx->DrawTextN(x, y, color, text, (int)wcslen(text), flags, 0.0f);
}
