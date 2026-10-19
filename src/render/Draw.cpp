// 2D drawing primitives: rectangles, polygons, gradients and dialog boxes,
// drawn with transformed (XYZRHW) vertices and the texture factor as color.
// Xbox only (Direct3D).
#include "Draw.h"

#include <cmath>
#include <cstring>
#include <cwchar>

#include "../skin/SkinColors.h"
#include "D3D.h"
#include "Fonts.h"
#include "Texture.h"

struct SScreenVertex
{
	float x, y, z, rhw;
};

static DWORD FloatBits(float value)
{
	DWORD bits;
	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

// The state shared by the flat-colored primitives: no texture, the color
// (and alpha) from the texture factor, alpha blending.
static void SetFlatColorState()
{
	g_pd3dDevice->SetTexture(0, NULL);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW);
}

static void SetVertex(SScreenVertex *v, float x, float y)
{
	v->x = x;
	v->y = y;
	v->z = 0.0f;
	v->rhw = 0.0f;
}

// 0x000A087B
// The corners are snapped to whole pixels (and moved to pixel centers).
// The fill color's alpha is replaced by alpha * 255.
HRESULT DrawRect(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                 float alpha, DWORD borderWidth)
{
	SetFlatColorState();

	float left = (float)floor(x);
	float right = (float)floor(x + width);
	float top = (float)floor(y);
	float bottom = (float)floor(y + height);
	SScreenVertex v[5];
	SetVertex(&v[0], left - 0.5f, top - 0.5f);
	SetVertex(&v[1], right - 0.5f, top - 0.5f);
	SetVertex(&v[2], right - 0.5f, bottom - 0.5f);
	SetVertex(&v[3], left - 0.5f, bottom - 0.5f);
	SetVertex(&v[4], left - 0.5f, bottom - 0.5f);   // unused

	if (fillColor != 0)
	{
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR,
		                             (fillColor & 0x00FFFFFF) + ((DWORD)(int)(alpha * 255.0f) << 24));
		g_pd3dDevice->DrawVerticesUP(D3DPT_QUADLIST, 4, v, sizeof(SScreenVertex));
	}
	if (borderColor != 0 && borderWidth != 0)
	{
		if (borderWidth > 1)
			g_pd3dDevice->SetRenderState(D3DRS_LINEWIDTH, FloatBits((float)borderWidth));
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, borderColor);
		g_pd3dDevice->DrawVerticesUP(D3DPT_LINELOOP, 4, v, sizeof(SScreenVertex));
		if (borderWidth > 1)
			g_pd3dDevice->SetRenderState(D3DRS_LINEWIDTH, FloatBits(1.0f));
	}
	return S_OK;
}

// 0x000A1D37
HRESULT DrawFrame(float x, float y, float width, float height, D3DCOLOR color, DWORD lineWidth)
{
	return DrawRect(x, y, width, height, color, 0, 1.0f, lineWidth);
}

// 0x000A1A39
// Darkens the whole screen: black at the given alpha.
void DimScreen(float alpha)
{
	float right, bottom;
	GetScreenEnd(&right, &bottom);
	DrawRect(0.0f, 0.0f, right, bottom, 0, 0xFF000000, alpha, 1);
}

// Scales a color's alpha by alpha (only used below 1.0).
static D3DCOLOR FadeColor(D3DCOLOR color, float alpha)
{
	if (alpha < 1.0f)
		color -= (DWORD)(int)((double)(color >> 24) * (1.0 - alpha)) << 24;
	return color;
}

// 0x000A0A87
// A filled convex polygon (a triangle fan) and its outline.
HRESULT DrawPolygon(const SScreenVertex *vertices, UINT count, D3DCOLOR fillColor, D3DCOLOR borderColor,
                    float alpha, DWORD lineWidth)
{
	g_pd3dDevice->SetTexture(0, NULL);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW);

	if (fillColor != 0)
	{
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, FadeColor(fillColor, alpha));
		g_pd3dDevice->DrawVerticesUP(D3DPT_TRIANGLEFAN, count, vertices, sizeof(SScreenVertex));
	}
	if (borderColor != 0)
	{
		borderColor = FadeColor(borderColor, alpha);
		if (lineWidth > 1)
			g_pd3dDevice->SetRenderState(D3DRS_LINEWIDTH, FloatBits((float)lineWidth));
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, borderColor);
		g_pd3dDevice->DrawVerticesUP(D3DPT_LINELOOP, count, vertices, sizeof(SScreenVertex));
		if (lineWidth > 1)
			g_pd3dDevice->SetRenderState(D3DRS_LINEWIDTH, FloatBits(1.0f));
	}
	return S_OK;
}

// 0x000A1BBF (x and y arrive in xmm0/xmm5)
// A box with its corners cut off by 3 pixels: the frame of dialogs and
// message boxes.
void DrawBevelBox(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                  float alpha, DWORD lineWidth)
{
	const float cut = 3.0f;
	float left = x - 0.5f;
	float top = y - 0.5f;
	float right = x + width - 0.5f;
	float bottom = y + height - 0.5f;
	SScreenVertex v[8];
	SetVertex(&v[0], left + cut, top);
	SetVertex(&v[1], right - cut, top);
	SetVertex(&v[2], right, top + cut);
	SetVertex(&v[3], right, bottom - cut);
	SetVertex(&v[4], right - cut, bottom);
	SetVertex(&v[5], left + cut, bottom);
	SetVertex(&v[6], left, bottom - cut);
	SetVertex(&v[7], left, top + cut);
	DrawPolygon(v, 8, fillColor, borderColor, alpha, lineWidth);
}

// 0x000A0D78
// A triangle filling the box, pointing up (1), down (2), left (3) or right
// (4). Quirk: any other direction draws whatever is on the stack.
HRESULT DrawTriangle(float x, float y, float width, float height, int direction, D3DCOLOR borderColor,
                     D3DCOLOR fillColor, float alpha)
{
	SetFlatColorState();

	float left = x - 0.5f;
	float top = y - 0.5f;
	float right = left + width;
	float bottom = top + height;
	SScreenVertex v[3];
	switch (direction)
	{
	case 1:
		SetVertex(&v[0], left, bottom);
		SetVertex(&v[1], right, bottom);
		SetVertex(&v[2], left + width * 0.5f, top);
		break;
	case 2:
		SetVertex(&v[0], left, top);
		SetVertex(&v[1], right, top);
		SetVertex(&v[2], left + width * 0.5f, bottom);
		break;
	case 3:
		SetVertex(&v[0], left, top + height * 0.5f);
		SetVertex(&v[1], right, top);
		SetVertex(&v[2], right, bottom);
		break;
	case 4:
		SetVertex(&v[0], left, top);
		SetVertex(&v[1], right, top + height * 0.5f);
		SetVertex(&v[2], left, bottom);
		break;
	}
	for (int i = 0; i < 3; i++)
		v[i].rhw = 1.0f;

	if (fillColor != 0)
	{
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR,
		                             (fillColor & 0x00FFFFFF) + ((DWORD)(int)(alpha * 255.0f) << 24));
		g_pd3dDevice->DrawVerticesUP(D3DPT_TRIANGLELIST, 3, v, sizeof(SScreenVertex));
	}
	if (borderColor != 0)
	{
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, borderColor);
		g_pd3dDevice->DrawVerticesUP(D3DPT_LINELOOP, 3, v, sizeof(SScreenVertex));
	}
	return S_OK;
}

// 0x000A1F08
// A rectangle shaded from color1 to color2, left to right (or top to bottom
// when vertical), with an optional 2-pixel frame around it (horizontal only).
HRESULT DrawGradientRect(float x, float y, float width, float height, D3DCOLOR color1, D3DCOLOR color2,
                         BOOL vertical, D3DCOLOR frameColor)
{
	if (frameColor != 0 && !vertical)
		DrawFrame((float)(x + 0.5), y - 0.25f, width - 0.9f, height + 1.25f, frameColor, 2);

	g_pd3dDevice->SetTexture(0, NULL);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

	g_pd3dDevice->Begin(D3DPT_QUADLIST);
	g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, color1);
	float left = x - 0.5f;
	float top = y - 0.5f;
	float right = x + width - 0.5f;
	float bottom = y + height - 0.5f;
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, top, 1.0f, 1.0f);
	if (!vertical)
	{
		g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, bottom, 1.0f, 1.0f);
		g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, color2);
		g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, bottom, 1.0f, 1.0f);
		g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, top, 1.0f, 1.0f);
	}
	else
	{
		g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, top, 1.0f, 1.0f);
		g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, color2);
		g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, bottom, 1.0f, 1.0f);
		g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, bottom, 1.0f, 1.0f);
	}
	g_pd3dDevice->End();
	return S_OK;
}

// 0x000A1D70 (text arrives in ebx)
// A centered box with text and animated dots ("Loading", "Loading.", ...).
HRESULT DrawBusyBox(CXBFont *font, const wchar_t *text)
{
	if (font == NULL || text == NULL)
		return S_OK;

	float centerX = 320.0f;
	float centerY = 240.0f;
	ScaleToScreen(&centerX, NULL, &centerY, NULL, FALSE);
	DWORD phase = (GetTickCount() / 500) & 3;
	wchar_t line[152];
	swprintf(line, 152, L"%ls%ls", text, L"...");
	float textWidth = font->GetTextWidth(text);
	float boxWidth = textWidth + 50.0f;
	if (boxWidth < 200.0f)
		boxWidth = 200.0f;
	if (phase == 3)
		swprintf(line, 152, L"%ls%ls", text, L"..");
	else if (phase == 2)
		swprintf(line, 152, L"%ls%ls", text, L".");
	else if (phase == 1)
		wcsncpy(line, text, 0x96);

	float lineHeight = font->m_fScaleFactorY * font->m_fFontHeight;
	DrawBevelBox(centerX - boxWidth * 0.5f, centerY - 5.0f - lineHeight * 0.5f, boxWidth, lineHeight + 10.0f,
	             g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 1.0f, 2);
	font->DrawText(centerX - textWidth * 0.5f, centerY, g_colorScheme.CAPTION_TEXTCOLOR, line, XBFONT_CENTER_Y,
	               0.0f);
	return S_OK;
}
