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

static void SetVertexRhw(SScreenVertex *v, float x, float y)
{
	v->x = x;
	v->y = y;
	v->z = 0.0f;
	v->rhw = 1.0f;
}

// 0x000A10D2 (width arrives in xmm7)
// A dialog window: the outer shape is a tab across the top 85% of the width
// that slopes down (by margin + captionHeight) to the body, with cut
// corners and a notch sticking out 5 pixels on the left; inside it, a bevel
// box (3-pixel cuts) between the caption and the button row.
void DrawWindowFrame(float x, float y, float width, float height, float margin, float captionHeight,
                     float footerHeight, D3DCOLOR borderColor, D3DCOLOR fillColor, D3DCOLOR innerBorderColor,
                     D3DCOLOR innerFillColor, DWORD borderWidth, DWORD innerBorderWidth)
{
	const float cut = 5.0f;
	float right = x + width;
	float bottom = y + height;
	if (borderWidth < 1)
		borderWidth = 1;

	float left = x - 0.5f;
	float top = y - 0.5f;
	float tabRight = right - width * 0.15f - 0.5f;
	float bodyTop = top + margin + captionHeight;
	float edgeRight = right - 0.5f;
	float edgeBottom = bottom - 0.5f;
	float notch = height * 0.25f + y - 0.5f;
	SScreenVertex v[12];
	SetVertexRhw(&v[0], left, top);
	SetVertexRhw(&v[1], tabRight, top);
	SetVertexRhw(&v[2], tabRight + margin + captionHeight, bodyTop);
	SetVertexRhw(&v[3], edgeRight - cut, bodyTop);
	SetVertexRhw(&v[4], edgeRight, bodyTop + cut);
	SetVertexRhw(&v[5], edgeRight, edgeBottom - cut);
	SetVertexRhw(&v[6], edgeRight - cut, edgeBottom);
	SetVertexRhw(&v[7], left + cut, edgeBottom);
	SetVertexRhw(&v[8], left, edgeBottom - cut);
	SetVertexRhw(&v[9], left, notch);
	SetVertexRhw(&v[10], left - cut, notch - cut);
	SetVertexRhw(&v[11], left - cut, top + cut);
	DrawPolygon(v, 12, fillColor, borderColor, 1.0f, borderWidth);

	if (innerBorderWidth < 1)
		innerBorderWidth = 1;
	const float innerCut = 3.0f;
	float innerLeft = x + margin - 0.5f;
	float innerRight = right - margin - 0.5f;
	float innerTop = margin * 2.0f + y + captionHeight - 0.5f;
	float innerBottom = bottom - (margin + footerHeight) - 0.5f;
	SScreenVertex inner[8];
	SetVertexRhw(&inner[0], innerLeft + innerCut, innerTop);
	SetVertexRhw(&inner[1], innerRight - innerCut, innerTop);
	SetVertexRhw(&inner[2], innerRight, innerTop + innerCut);
	SetVertexRhw(&inner[3], innerRight, innerBottom - innerCut);
	SetVertexRhw(&inner[4], innerRight - innerCut, innerBottom);
	SetVertexRhw(&inner[5], innerLeft + innerCut, innerBottom);
	SetVertexRhw(&inner[6], innerLeft, innerBottom - innerCut);
	SetVertexRhw(&inner[7], innerLeft, innerTop + innerCut);
	DrawPolygon(inner, 8, innerFillColor, innerBorderColor, 1.0f, innerBorderWidth);
}

// 0x000A1518 (x arrives in xmm0)
// A message box button: an outlined octagon, and a filled one 4 pixels
// inside it (cuts of 3 and 2 pixels).
void DrawButtonBox(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                   DWORD lineWidth)
{
	float left = x;
	float top = y;
	float right = x + width;
	float bottom = y + height;
	float cut = 3.0f;
	for (int i = 0; i < 2; i++)
	{
		float inset = (float)(i * 4);
		left += inset;
		top += inset;
		right -= inset;
		bottom -= inset;
		cut -= (float)i;
		float l = left - 0.5f;
		float t = top - 0.5f;
		float r = right - 0.5f;
		float b = bottom - 0.5f;
		SScreenVertex v[8];
		SetVertexRhw(&v[0], l + cut, t);
		SetVertexRhw(&v[1], r - cut, t);
		SetVertexRhw(&v[2], r, t + cut);
		SetVertexRhw(&v[3], r, b - cut);
		SetVertexRhw(&v[4], r - cut, b);
		SetVertexRhw(&v[5], l + cut, b);
		SetVertexRhw(&v[6], l, b - cut);
		SetVertexRhw(&v[7], l, t + cut);
		DrawPolygon(v, 8, i == 1 ? fillColor : 0, borderColor, 1.0f, lineWidth);
	}
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

// A textured, colored screen vertex (D3DFVF_XYZRHW | D3DFVF_DIFFUSE |
// D3DFVF_TEX1).
struct STextureVertex
{
	float x, y, z, rhw;
	D3DCOLOR color;
	float u, v;
};

static void SetTextureVertex(STextureVertex *v, float x, float y, D3DCOLOR color, float u, float w)
{
	v->x = x;
	v->y = y;
	v->z = 1.0f;
	v->rhw = 1.0f;
	v->color = color;
	v->u = u;
	v->v = w;
}

// 0x000A22AC (center in ecx)
// Scales the four vertices by scale and rotates them by angle radians
// around center. The original builds the D3DX matrices (translate, scale,
// rotate about Z, translate back: 0x000A17FC) and multiplies the vertices
// as a 4x4 matrix. TODO: verify the multiplication order
static void TransformQuad(STextureVertex *v, const float *center, const float *scale, float angle)
{
	float c = (float)cos(angle);
	float s = (float)sin(angle);
	for (int i = 0; i < 4; i++)
	{
		float x = (v[i].x - center[0]) * scale[0];
		float y = (v[i].y - center[1]) * scale[1];
		v[i].x = x * c - y * s + center[0];
		v[i].y = x * s + y * c + center[1];
	}
}

// 0x000A246F
HRESULT DrawTexture(IDirect3DTexture8 *texture, float x, float y, float width, float height, float alpha,
                    float *center, float angle, const float *scale)
{
	if (texture == NULL)
		return E_FAIL;
	if (width == 0.0f || height == 0.0f)
	{
		// Quirk: both take the texture's width.
		D3DSURFACE_DESC desc;
		texture->GetLevelDesc(0, &desc);
		if (width == 0.0f)
			width = (float)desc.Width;
		if (height == 0.0f)
			height = (float)desc.Width;
	}

	// Snapped to whole pixels.
	float left = (float)floor(x);
	float top = (float)floor(y);
	width = (float)floor(x - left + width);
	height = (float)floor(y - top + height);
	float right = left + width;
	float bottom = top + height;
	D3DCOLOR color = ((DWORD)(int)(alpha * 255.0f) << 24) | 0x00FFFFFF;
	STextureVertex v[4];
	SetTextureVertex(&v[0], left - 0.5f, top - 0.5f, color, 0.0f, 0.0f);
	SetTextureVertex(&v[1], left - 0.5f, bottom - 0.5f, color, 0.0f, 1.0f);
	SetTextureVertex(&v[2], right - 0.5f, bottom - 0.5f, color, 1.0f, 1.0f);
	SetTextureVertex(&v[3], right - 0.5f, top - 0.5f, color, 1.0f, 0.0f);

	if (angle != 0.0f || center != NULL || scale != NULL)
	{
		float pivot[2];
		if (center != NULL)
		{
			ScaleToScreen(&center[0], NULL, &center[1], NULL, FALSE);
			pivot[0] = center[0] + left;
			pivot[1] = center[1] + top;
		}
		else
		{
			pivot[0] = width * 0.5f + left;
			pivot[1] = height * 0.5f + top;
		}
		float factors[2] = {1.0f, 1.0f};
		if (scale != NULL)
		{
			factors[0] = scale[0];
			factors[1] = scale[1];
		}
		TransformQuad(v, pivot, factors, angle * 0.017453292f);
	}

	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	g_pd3dDevice->SetTexture(0, texture);
	// (The original writes these into D3D's texture stage state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->DrawVerticesUP(D3DPT_QUADLIST, 4, v, sizeof(STextureVertex));
	g_pd3dDevice->SetTexture(0, NULL);
	return S_OK;
}

// 0x000A2897
HRESULT DrawProgressBar(float x, float y, float width, float height, float percent, D3DCOLOR borderColor,
                        D3DCOLOR backColor, D3DCOLOR fill1, D3DCOLOR fill2, BYTE gradient)
{
	BOOL useGradient = gradient & (fill1 != fill2);
	g_pd3dDevice->SetTexture(0, NULL);
	// (The original writes these into D3D's texture stage state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW);

	float left = x;
	float right = x + width;
	float top = y;
	float bottom = y + height;
	float empty = 1.0f - (percent > 100.0f ? 1.0f : percent * 0.01f);
	float cut = 3.0f;
	// The frame, then the filled part.
	for (int i = 0; i < 2; i++)
	{
		float inset = (float)(i * 3);
		left += inset;
		right = right - ((float)i * width * empty + inset);
		if (!(left < right))
			right = left + 1.0f;
		cut -= inset;
		top += inset;
		bottom -= inset;
		float l = left - 0.5f;
		float t = top - 0.5f;
		float r = right - 0.5f;
		float b = bottom - 0.5f;
		SScreenVertex v[8];
		SetVertexRhw(&v[0], l + cut, t);
		SetVertexRhw(&v[1], r - cut, t);
		SetVertexRhw(&v[2], r, t + cut);
		SetVertexRhw(&v[3], r, b - cut);
		SetVertexRhw(&v[4], r - cut, b);
		SetVertexRhw(&v[5], l + cut, b);
		SetVertexRhw(&v[6], l, b - cut);
		SetVertexRhw(&v[7], l, t + cut);
		if (i == 1)
		{
			if (useGradient)
			{
				DrawGradientRect(left, top, right - left, bottom - top, fill2, fill1, FALSE, 0);
			}
			else
			{
				g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, fill1);
				g_pd3dDevice->DrawVerticesUP(D3DPT_POLYGON, 8, v, sizeof(SScreenVertex));
			}
		}
		else
		{
			if (backColor != 0)
			{
				g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, backColor);
				g_pd3dDevice->DrawVerticesUP(D3DPT_POLYGON, 8, v, sizeof(SScreenVertex));
			}
			if (borderColor != 0)
			{
				g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, borderColor);
				g_pd3dDevice->DrawVerticesUP(D3DPT_LINELOOP, 8, v, sizeof(SScreenVertex));
			}
		}
	}
	return S_OK;
}

// 0x000A282E
void DrawTextureFullScreen(IDirect3DTexture8 *texture, float alpha)
{
	SScreenRect screen = GetScreenRect();
	DrawTexture(texture, (float)(int)screen.x, (float)(int)screen.y, (float)(int)screen.width,
	            (float)(int)screen.height, alpha, NULL, 0.0f, NULL);
}

// 0x0009F2B2
void SetScissorRect(BOOL enable, float x, float y, float width, float height)
{
	if (!enable || width <= 0.0f || height <= 0.0f)
	{
		g_pd3dDevice->SetScissors(0, FALSE, NULL);
		return;
	}
	float displayWidth, displayHeight;
	GetDisplaySize(&displayWidth, &displayHeight, FALSE);
	float right = x + width;
	if (right > displayWidth)
		right = displayWidth;
	float bottom = y + height;
	if (bottom > displayHeight)
		bottom = displayHeight;
	D3DRECT rect;
	rect.x1 = (long)x;
	rect.y1 = (long)y;
	rect.x2 = (long)right;
	rect.y2 = (long)bottom;
	g_pd3dDevice->SetScissors(1, FALSE, &rect);
}
