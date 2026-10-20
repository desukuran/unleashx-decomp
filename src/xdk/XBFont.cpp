// The XDK sample framework's bitmap font (CXBFont) in the version UnleashX
// links: font files version 4 and 5, drawing into a surface, and integer
// text sizes. Xbox only. See render/Fonts.h.
#include "../render/Fonts.h"

#include <math.h>
#include <wchar.h>

// A glyph in the font data. 0x18 bytes.
struct SGlyph
{
	float tu1, tv1, tu2, tv2;   // +0x00 texture coordinates
	short sOffset;              // +0x10 pixels before the glyph
	short sWidth;               // +0x12
	short sAdvance;             // +0x14 pixels after it
	short reserved;
};

// The font data that follows the texture in the .xpr.
struct SFontHeader
{
	DWORD unknown00[2];
	DWORD dwVersion;            // +0x08 4 or 5
	DWORD dwFontHeight;         // +0x0C
	DWORD dwTexWidth;           // +0x10
	DWORD dwTexHeight;          // +0x14
	DWORD dwLineHeight;         // +0x18 (version 5)
	WORD wLowChar;              // +0x1C version 4: the first character; version 5: the last
	WORD wHighChar;             // +0x1E version 4: the last character; version 5: the translator table starts here
};

static DWORD s_dwFontVertexShader;   // 0x00277670
static DWORD s_dwFontPixelShader;    // 0x00277674

// 0x0009906E (this in edx)
CXBFont::CXBFont()
{
	m_dwTextColor = 0xFFFFFFFF;
	m_fCellHeight = 36.0f;
	m_fFontHeight = 36.0f;
	m_pFontTexture = NULL;
	m_dwNumGlyphs = 0;
	m_pGlyphs = NULL;
	m_cMaxGlyph = 0;
	m_pTranslatorTable = NULL;
	m_bTranslatorTableAllocated = FALSE;
	m_dwNestedBeginCount = 0;
	m_dwTexWidth = 0;
	m_dwTexHeight = 0;
	m_fCursorX = 0.0f;
	m_fCursorY = 0.0f;
	m_fScaleFactorX = 1.0f;
	m_fScaleFactorY = 1.0f;
	m_fSlantFactor = 0.0f;
}

// 0x00099051 (this in eax)
CXBFont::~CXBFont()
{
	Destroy();
}

// 0x00098FFE (this in edi)
HRESULT CXBFont::Destroy()
{
	m_xprResource.Destroy();
	m_pFontTexture = NULL;
	m_fCellHeight = 0.0f;
	m_dwTexWidth = 0;
	m_dwTexHeight = 0;
	m_fFontHeight = 0.0f;
	m_dwNumGlyphs = 0;
	m_pGlyphs = NULL;
	m_cMaxGlyph = 0;
	if (m_bTranslatorTableAllocated)
		delete[] m_pTranslatorTable;
	m_pTranslatorTable = NULL;
	m_bTranslatorTableAllocated = FALSE;
	m_dwNestedBeginCount = 0;
	return S_OK;
}

// 0x000995C7: the screen-space shader and the pixel shader shared by all
// fonts. The vertex shader takes v0.xy as the screen position, v0.zw as the
// texture coordinates and v3 as the color.
static HRESULT CreateShaders()
{
	if (s_dwFontVertexShader == 0)
	{
		DWORD declaration[] =
		{
			D3DVSD_STREAM(0),
			D3DVSD_REG(0, D3DVSDT_FLOAT4),
			D3DVSD_REG(3, D3DVSDT_D3DCOLOR),
			D3DVSD_END()
		};
		static const char program[] =
			"xvss.1.1\n"
			"mov oPos.xy, v0.xy\n"
			"mov oT0.xy, v0.zw\n"
			"mov oD0, v3\n";
		LPXGBUFFER microcode = NULL;
		if (FAILED(XGAssembleShader("font.vsh", program, sizeof(program) - 1, 0, NULL, &microcode, NULL, NULL,
		                            NULL, NULL, NULL)))
			return E_FAIL;
		HRESULT hr = D3DDevice_CreateVertexShader(declaration, (const DWORD *)microcode->GetBufferPointer(),
		                                          &s_dwFontVertexShader, D3DUSAGE_PERSISTENTDIFFUSE);
		microcode->Release();
		if (FAILED(hr))
			return E_FAIL;
	}
	if (s_dwFontPixelShader == 0)
	{
		// The texture's alpha times the diffuse color.
		D3DPIXELSHADERDEF shader;
		memset(&shader, 0, sizeof(shader));
		shader.PSCombinerCount = 1;
		shader.PSTextureModes = 1;
		shader.PSAlphaInputs[0] = 0x18140000;
		shader.PSFinalCombinerInputsABCD = 0x010C0002;
		shader.PSFinalCombinerInputsEFG = 0x00001C00;
		shader.PSAlphaOutputs[0] = 0xC0;
		shader.PSRGBInputs[0] = 0x08040000;
		shader.PSFinalCombinerConstant0 = 0xDADADA;
		shader.PSFinalCombinerConstant1 = 0x101010;
		shader.PSRGBOutputs[0] = 0xC0;
		shader.PSFinalCombinerConstants = 0x10;
		D3DDevice_CreatePixelShader(&shader, &s_dwFontPixelShader);
	}
	return S_OK;
}

// 0x00099EC9: the font texture is the .xpr's first resource; the glyph
// data follows it.
HRESULT CXBFont::Create(const char *fileName, DWORD resourceOffset)
{
	if (FAILED(m_xprResource.Create(fileName, 0, NULL)))
		return E_FAIL;
	IDirect3DTexture8 *texture = (IDirect3DTexture8 *)m_xprResource.GetResource(0);
	return Create(texture, m_xprResource.m_pSysMemData + resourceOffset + 0x14);
}

// 0x000998F0 (this in esi, texture in eax)
HRESULT CXBFont::Create(IDirect3DTexture8 *texture, const void *fontData)
{
	m_pFontTexture = texture;
	const SFontHeader *header = (const SFontHeader *)fontData;
	if (header->dwVersion == 4)
	{
		m_fCellHeight = (float)header->dwFontHeight;
		m_fFontHeight = m_fCellHeight;
		float texWidth = (float)header->dwTexWidth;
		float texHeight = (float)header->dwTexHeight;
		WORD lowChar = header->wLowChar;
		m_pGlyphs = (SGlyph *)((const BYTE *)fontData + 0x24);
		m_cMaxGlyph = header->wHighChar;
		m_pTranslatorTable = new short[m_cMaxGlyph + 1];
		m_bTranslatorTableAllocated = TRUE;
		// Version 4 glyphs are one pixel narrower than their cells, and
		// the coordinates are pulled in by a texel.
		for (WORD c = 0; c <= m_cMaxGlyph; c++)
		{
			if (c < lowChar)
			{
				m_pTranslatorTable[c] = 0;
				continue;
			}
			m_pTranslatorTable[c] = (short)(c - lowChar);
			SGlyph *glyph = &m_pGlyphs[m_pTranslatorTable[c]];
			glyph->sWidth++;
			float u = (texWidth - 1.0f) / texWidth;
			float v = (texHeight - 1.0f) / texHeight;
			glyph->tu1 *= u;
			glyph->tv1 *= v;
			glyph->tu2 *= u;
			glyph->tv2 *= v;
		}
		m_fCellHeight += 1.0f;
	}
	else if (header->dwVersion == 5)
	{
		m_fCellHeight = *(const float *)&header->dwFontHeight;
		m_dwTexWidth = header->dwTexWidth;
		m_dwTexHeight = header->dwTexHeight;
		m_fFontHeight = *(const float *)&header->dwLineHeight;
		m_cMaxGlyph = header->wLowChar;
		m_bTranslatorTableAllocated = FALSE;
		m_pTranslatorTable = (short *)&header->wHighChar;
		const DWORD *glyphs = (const DWORD *)((const BYTE *)fontData + 0x20 + m_cMaxGlyph * 2);
		m_dwNumGlyphs = glyphs[0];
		m_pGlyphs = (SGlyph *)(glyphs + 1);
	}
	else
	{
		if (m_bTranslatorTableAllocated)
			delete[] m_pTranslatorTable;
		m_pTranslatorTable = NULL;
		m_bTranslatorTableAllocated = FALSE;
		return E_FAIL;
	}
	if (FAILED(CreateShaders()))
		return E_FAIL;
	// A pure device can't read its states back, so they aren't saved.
	D3DDEVICE_CREATION_PARAMETERS parameters;
	D3DDevice_GetCreationParameters(&parameters);
	m_bSaveState = (parameters.BehaviorFlags & D3DCREATE_PUREDEVICE) == 0;
	return S_OK;
}

// 0x00098FEB
void CXBFont::SetScaleFactors(float scaleX, float scaleY)
{
	m_fScaleFactorX = scaleX;
	m_fScaleFactorY = scaleY;
}

// 0x000995A2
void CXBFont::SetCursorPosition(float x, float y)
{
	m_fCursorX = floorf(x);
	m_fCursorY = floorf(y);
}

// The glyph for c: through the translator table, the first glyph when out
// of range or unmapped.
static SGlyph *GetGlyph(CXBFont *font, WCHAR c)
{
	if (c > font->m_cMaxGlyph || font->m_pTranslatorTable[c] == 0)
		return font->m_pGlyphs;
	return &font->m_pGlyphs[font->m_pTranslatorTable[c]];
}

// The render states Begin sets, in the order they're saved.
static const D3DRENDERSTATETYPE s_savedStates[] =
{
	D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_ALPHATESTENABLE, D3DRS_ALPHAREF,
	D3DRS_ALPHAFUNC, D3DRS_FILLMODE, D3DRS_CULLMODE, D3DRS_ZENABLE, D3DRS_STENCILENABLE, D3DRS_EDGEANTIALIAS
};

// 0x0009978B
HRESULT CXBFont::Begin()
{
	if (m_dwNestedBeginCount == 0)
	{
		if (m_bSaveState)
		{
			for (int i = 0; i < 11; i++)
				D3DDevice_GetRenderState(s_savedStates[i], &m_dwSavedState[i]);
			D3DDevice_GetTextureStageState(0, D3DTSS_MINFILTER, &m_dwSavedState[11]);
			D3DDevice_GetTextureStageState(0, D3DTSS_MAGFILTER, &m_dwSavedState[12]);
		}
		D3DDevice_SetTexture(0, m_pFontTexture);
		D3DDevice_SetVertexShader(s_dwFontVertexShader);
		D3DDevice_SetPixelShader(s_dwFontPixelShader);
		D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		D3DDevice_SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		D3DDevice_SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		D3DDevice_SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
		D3DDevice_SetRenderState(D3DRS_ALPHAREF, 0x08);
		D3DDevice_SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		D3DDevice_SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		D3DDevice_SetRenderState(D3DRS_ZENABLE, FALSE);
		D3DDevice_SetRenderState(D3DRS_STENCILENABLE, FALSE);
		D3DDevice_SetRenderState(D3DRS_EDGEANTIALIAS, FALSE);
		D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		D3DDevice_Begin(D3DPT_QUADLIST);
	}
	m_dwNestedBeginCount++;
	return S_OK;
}

// 0x0009968D
HRESULT CXBFont::End()
{
	if (m_dwNestedBeginCount == 0)
		return E_FAIL;
	if (--m_dwNestedBeginCount != 0)
		return S_FALSE;
	D3DDevice_End();
	if (m_bSaveState)
	{
		D3DDevice_SetTexture(0, NULL);
		D3DDevice_SetPixelShader(0);
		for (int i = 0; i < 11; i++)
			D3DDevice_SetRenderState(s_savedStates[i], m_dwSavedState[i]);
		D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, m_dwSavedState[11]);
		D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, m_dwSavedState[12]);
	}
	return S_OK;
}

// 0x00098E3A
HRESULT CXBFont::DrawText(float x, float y, DWORD color, const wchar_t *text, DWORD flags, float maxWidth)
{
	return DrawTextN(x, y, color, text, (int)wcslen(text), flags, maxWidth);
}

// 0x00098E9E: the size of the first length characters, scaled. A tab is
// four spaces wide. (this in eax, width in edx, height in esi)
HRESULT CXBFont::GetTextExtentN(const wchar_t *text, int length, BOOL firstLineOnly, float *width, float *height)
{
	if (text == NULL || width == NULL || height == NULL)
		return E_INVALIDARG;
	*width = 0.0f;
	*height = 0.0f;
	float lineHeight = m_fCellHeight;
	float lineWidth = 0.0f;
	while (length-- != 0)
	{
		WCHAR c = *text++;
		int advance;
		short offset;
		if (c == L'\n')
		{
			if (firstLineOnly)
				break;
			lineHeight += m_fFontHeight;
			lineWidth = 0.0f;
		}
		if (c == L'\t')
		{
			SGlyph *space = GetGlyph(this, L' ');
			offset = space->sOffset;
			advance = space->sAdvance * 4;
		}
		else
		{
			SGlyph *glyph = GetGlyph(this, c);
			if (glyph == NULL)
				continue;
			offset = glyph->sOffset;
			advance = glyph->sAdvance;
		}
		lineWidth += (float)offset + (float)advance;
		if (lineWidth > *width)
			*width = lineWidth;
		if (lineHeight > *height)
			*height = lineHeight;
	}
	*width *= m_fScaleFactorX;
	*height *= m_fScaleFactorY;
	return S_OK;
}

// 0x00098FC3 (height in esi)
HRESULT CXBFont::GetTextExtent(const wchar_t *text, float *width, float *height, BOOL firstLineOnly)
{
	return GetTextExtentN(text, (int)wcslen(text), firstLineOnly, width, height);
}

// 0x00099571
float CXBFont::GetTextWidth(const wchar_t *text)
{
	float width = 0.0f;
	float height;
	GetTextExtent(text, &width, &height, FALSE);
	return width;
}

// 0x00099345: the size in whole pixels; length -1 for all of it.
HRESULT CXBFont::GetTextExtentInt(const wchar_t *text, int length, int *width, int *height)
{
	if (length == -1)
		length = (int)wcslen(text);
	float w, h;
	HRESULT hr = GetTextExtentN(text, length, FALSE, &w, &h);
	*width = (int)w;
	*height = (int)h;
	return hr;
}

// 0x00099AAD
HRESULT CXBFont::DrawTextN(float x, float y, DWORD color, const wchar_t *text, int length, DWORD flags,
                           float maxWidth)
{
	Begin();
	D3DDevice_SetVertexDataColor(D3DVSDE_DIFFUSE, color);
	m_fCursorX = floorf(x);
	m_fCursorY = floorf(y);

	// XBFONT_TRUNCATED with an ellipsis; flag 0x10 truncates without one.
	BOOL ellipsis = TRUE;
	if (flags & 0x10)
	{
		ellipsis = FALSE;
		flags = (flags & ~0x10) | XBFONT_TRUNCATED;
	}
	if (flags & XBFONT_TRUNCATED)
	{
		float width, height;
		if (maxWidth <= 0.0f || (GetTextExtentN(text, length, TRUE, &width, &height), width <= maxWidth))
			flags &= ~XBFONT_TRUNCATED;
	}
	if (flags & XBFONT_CENTER_Y)
	{
		float width, height;
		GetTextExtent(text, &width, &height, FALSE);
		m_fCursorY = floorf(m_fCursorY - height * 0.5f);
	}

	BOOL lineStart = TRUE;
	while (length-- != 0)
	{
		if (lineStart)
		{
			if (flags & (XBFONT_RIGHT | XBFONT_CENTER_X))
			{
				float width, height;
				GetTextExtent(text, &width, &height, TRUE);
				if (flags & XBFONT_RIGHT)
					m_fCursorX = floorf(x - width);
				if (flags & XBFONT_CENTER_X)
					m_fCursorX = floorf(x - width * 0.5f);
			}
			lineStart = FALSE;
		}
		WCHAR c = *text++;
		if (c == L'\n')
		{
			m_fCursorX = x;
			m_fCursorY += m_fScaleFactorY * m_fFontHeight;
			lineStart = TRUE;
			continue;
		}
		SGlyph *glyph = c > m_cMaxGlyph ? m_pGlyphs : &m_pGlyphs[m_pTranslatorTable[c]];
		if (c == L'\t' && m_cMaxGlyph >= 0x20)
			glyph = &m_pGlyphs[m_pTranslatorTable[L' ']];
		if (glyph == NULL)
			return E_FAIL;
		float tabs = c == L'\t' ? 4.0f : 1.0f;
		float width = (float)glyph->sWidth * tabs * m_fScaleFactorX;
		float offset = (float)glyph->sOffset * m_fScaleFactorX;
		if (flags & XBFONT_TRUNCATED)
		{
			// Room for the "..." that follows (one dot without it).
			SGlyph *dot = &m_pGlyphs[m_pTranslatorTable[L'.']];
			float dots = (float)(dot->sAdvance + dot->sOffset) * m_fScaleFactorX;
			if (ellipsis)
				dots *= 3.0f;
			if (x + maxWidth < m_fSlantFactor + m_fCursorX + dots + width + offset)
			{
				if (ellipsis)
					DrawText(m_fCursorX, m_fCursorY, color, L"...", 0, 0.0f);
				break;
			}
		}
		float left = m_fCursorX + offset;
		float slant = m_fSlantFactor;
		m_fCursorX = left + (float)glyph->sAdvance * tabs * m_fScaleFactorX;
		float top = m_fCursorY;
		float bottom = top + m_fScaleFactorY * m_fCellHeight;
		D3DDevice_SetVertexData4f(D3DVSDE_VERTEX, left, bottom, glyph->tu1, glyph->tv2);
		D3DDevice_SetVertexData4f(D3DVSDE_VERTEX, slant + left, top, glyph->tu1, glyph->tv1);
		D3DDevice_SetVertexData4f(D3DVSDE_VERTEX, slant + left + width, top, glyph->tu2, glyph->tv1);
		D3DDevice_SetVertexData4f(D3DVSDE_VERTEX, left + width, bottom, glyph->tu2, glyph->tv2);
	}
	End();
	return S_OK;
}

// 0x00098E22
HRESULT CXBFont::SetTextColor(D3DCOLOR color)
{
	m_dwTextColor = color;
	return S_OK;
}

// 0x00098E35: nothing for a bitmap font.
HRESULT CXBFont::SetTextHeight(DWORD height)
{
	return S_OK;
}

// 0x000326FA
HRESULT CXBFont::Unused1()
{
	return S_OK;
}

// 0x00099E24: the text drawn at (width, height) into surface, in the
// font's own color.
HRESULT CXBFont::DrawToSurface(IDirect3DSurface8 *surface, const wchar_t *text, int length, DWORD width,
                               DWORD height)
{
	if (length == 0)
		return S_OK;
	if (length == -1)
		length = (int)wcslen(text);
	IDirect3DSurface8 *renderTarget = NULL;
	IDirect3DSurface8 *depth = NULL;
	D3DDevice_GetRenderTarget(&renderTarget);
	D3DDevice_GetDepthStencilSurface(&depth);
	D3DDevice_SetRenderTarget(surface, NULL);
	HRESULT hr = DrawTextN((float)width, (float)height, m_dwTextColor, text, length, 0, 0.0f);
	D3DDevice_SetRenderTarget(renderTarget, depth);
	if (renderTarget != NULL)
		renderTarget->Release();
	if (depth != NULL)
		depth->Release();
	return hr;
}
