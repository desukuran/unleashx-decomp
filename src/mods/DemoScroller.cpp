// MOD (not in the original UnleashX): see DemoScroller.h.
#include "DemoScroller.h"

#include <math.h>
#include <wchar.h>

#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Texture.h"

static const wchar_t s_szMessage[] =
	L"*** UNLEASHX 0.39.0528A ... REBUILT FROM DECOMPILED SOURCE ... EVERY PIXEL YOU SEE IS OUR CODE NOW ... "
	L"THE ORIGINAL DASHBOARD LIVES ON ... GREETINGS TO THE UNLEASHX TEAM AND THE XBOX SCENE ... "
	L"PRESS A TO LAUNCH, B TO GO BACK, AND NEVER STOP MODDING ***          ";

static const int BAR_COUNT = 6;
static const float BAR_HEIGHT = 16.0f;
static const float SCROLL_SPEED = 140.0f;   // pixels per second
static const float TEXT_SCALE = 1.4f;

// A fully saturated color around the hue wheel (0..1).
static D3DCOLOR HueColor(float hue, DWORD alpha)
{
	hue -= (float)floor(hue);
	float h = hue * 6.0f;
	int sector = (int)h;
	float f = h - (float)sector;
	DWORD rise = (DWORD)(f * 255.0f), fall = 255 - rise;
	DWORD r, g, b;
	switch (sector)
	{
	case 0: r = 255; g = rise; b = 0; break;
	case 1: r = fall; g = 255; b = 0; break;
	case 2: r = 0; g = 255; b = rise; break;
	case 3: r = 0; g = fall; b = 255; break;
	case 4: r = rise; g = 0; b = 255; break;
	default: r = 255; g = 0; b = fall; break;
	}
	return (alpha << 24) | (r << 16) | (g << 8) | b;
}

// One copper bar: clear at its edges, color in the middle.
static void DrawCopperBar(float left, float right, float y, D3DCOLOR color)
{
	D3DCOLOR clear = color & 0x00FFFFFF;
	float top = y - BAR_HEIGHT * 0.5f, bottom = y + BAR_HEIGHT * 0.5f;
	g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, clear);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, top, 1.0f, 1.0f);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, top, 1.0f, 1.0f);
	g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, color);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, y, 1.0f, 1.0f);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, y, 1.0f, 1.0f);

	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, y, 1.0f, 1.0f);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, y, 1.0f, 1.0f);
	g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, clear);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, bottom, 1.0f, 1.0f);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, bottom, 1.0f, 1.0f);
}

void RenderDemoScroller(CXBFont *font, float t)
{
	if (font == NULL)
		return;
	float originX, originY, width, height;
	GetScreenOrigin(&originX, &originY);
	GetScreenSize(&width, &height);
	float centerY = originY + height * 0.74f;

	// The bars, chasing each other on a sine.
	g_pd3dDevice->SetTexture(0, NULL);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);   // additive: overlaps glow
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	g_pd3dDevice->Begin(D3DPT_QUADLIST);
	for (int i = 0; i < BAR_COUNT; i++)
	{
		float y = centerY + 30.0f * sinf(t * 1.6f + i * 0.55f);
		DrawCopperBar(originX, originX + width, y, HueColor(t * 0.08f + i / (float)BAR_COUNT, 0xC0));
	}
	g_pd3dDevice->End();
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	// The text: each character on its own point of the wave, its color
	// following its place on screen.
	float oldScaleX = font->m_fScaleFactorX, oldScaleY = font->m_fScaleFactorY;
	font->SetScaleFactors(TEXT_SCALE, TEXT_SCALE);
	int length = (int)wcslen(s_szMessage);
	float total = 0.0f;
	float charWidth[sizeof(s_szMessage) / sizeof(wchar_t)];
	for (int i = 0; i < length; i++)
	{
		float h;
		font->GetTextExtentN(&s_szMessage[i], 1, TRUE, &charWidth[i], &h);
		total += charWidth[i];
	}
	float x = originX + width - (float)fmod(t * SCROLL_SPEED, total + width);
	float right = originX + width;
	font->Begin();
	for (int i = 0; i < length && x < right; i++)
	{
		if (x + charWidth[i] >= originX && s_szMessage[i] != L' ')
		{
			float y = centerY - 14.0f + 24.0f * sinf(t * 3.5f + x * 0.018f);
			// A dark copy below for depth, then the rainbow.
			font->DrawTextN(x + 2.0f, y + 2.0f, 0xA0000000, &s_szMessage[i], 1, 0, 0.0f);
			font->DrawTextN(x, y, HueColor(t * 0.25f + x * 0.0015f, 0xFF), &s_szMessage[i], 1, 0, 0.0f);
		}
		x += charWidth[i];
	}
	font->End();
	font->SetScaleFactors(oldScaleX, oldScaleY);
}
