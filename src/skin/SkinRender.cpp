// CSkin::Render: the skin's <Main> shapes, texts and images over the
// background, and the loading picture while a skin loads. See Skin.h.
#include <cmath>
#include <cstring>
#include <cwchar>

#include "../media/WMVideoPlayer.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "Skin.h"
#include "SkinText.h"

// 0x000F910C (image in eax)
void LoadSkinImage(SSkinImage *image)
{
	if (image->texture != NULL || image->fileName[0] == 0)
		return;
	if (!LoadTextureFromFile(image->fileName, g_pd3dDevice, &image->texture, 1, 1, NULL))
		image->texture = NULL;
	memset(image->fileName, 0, sizeof(image->fileName));
}

// 0x0009E312
void SkinRectToFloats(int left, int top, int width, int height, float *x, float *y, float *w, float *h)
{
	*x = (float)left;
	*y = (float)top;
	*w = (float)width;
	*h = (float)height;
}

// (0x0009E312, then ScaleToScreen)
void SkinRectToScreen(int left, int top, int width, int height, float *x, float *y, float *w, float *h)
{
	SkinRectToFloats(left, top, width, height, x, y, w, h);
	ScaleToScreen(x, w, y, h, FALSE);
}

// (inlined)
float GetSkinTextScale()
{
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	ScaleToScreen(NULL, &scaleX, NULL, &scaleY, FALSE);
	float scale = scaleY <= scaleX ? scaleY : scaleX;
	float rounded = (float)floor(scale * 100.0f) * 0.01f;
	return rounded >= 1.0f ? rounded : 1.0f;
}

// (inlined)
void GetSkinTextString(const SSkinText &text, wchar_t *buffer)
{
	memset(buffer, 0, 0x105 * sizeof(wchar_t));
	if (!text.isFormat)
	{
		const wchar_t *value;
		if (text.source == TEXT_STATIC)
		{
			value = text.text;
			if (value == NULL)
				return;
		}
		else
		{
			value = GetInfoText(text.source);
		}
		wcsncpy(buffer, value, 0x104);
	}
	else
	{
		swprintf(buffer, 0x104, text.text, GetSkinTextValue(text.source));
	}
}

// (inlined)
void DrawScrollingSkinText(SSkinText &text, CXBFont *font, const wchar_t *shown, float x, float y, float width,
                           float height, BOOL step, BOOL offsetApplied)
{
	float displayWidth = 640.0f;
	float displayHeight = 480.0f;
	GetDisplaySize(&displayWidth, &displayHeight, FALSE);
	D3DRECT clip;
	clip.x1 = (long)x;
	clip.y1 = (long)y;
	clip.x2 = (long)(x + width > displayWidth ? displayWidth : x + width);
	clip.y2 = (long)(y + height > displayHeight ? displayHeight : y + height);
	g_pd3dDevice->SetScissors(1, FALSE, &clip);
	float textWidth = font->GetTextWidth(shown);
	DWORD align = text.align;
	BOOL moves = width < textWidth || text.scroll == 2;
	if (!step)
	{
		if (moves)
			align = 0;
	}
	else
	{
		align = 0;
		if (moves)
		{
			// A pixel to the left; gone: back in from the right.
			float offset;
			if ((float)clip.x1 <= (textWidth + x) - (float)text.unknown34)
				offset = (float)text.unknown34 + 1.0f;
			else
				offset = 0.0f - width;
			text.unknown34 = (int)offset;
		}
		else
		{
			text.unknown34 = 0;
		}
	}
	if (offsetApplied)
		x -= (float)text.unknown34;
	font->DrawText(x, y, text.color, shown, align, width);
	g_pd3dDevice->SetScissors(0, FALSE, NULL);
}

// An image without a picture goes to the event sink. TRUE: stop drawing
// (the preview video is playing). (inlined in both paths)
static BOOL DrawSkinImage(CSkin *skin, SSkinImage &image)
{
	LoadSkinImage(&image);
	if (image.texture != NULL)
	{
		float x, y, w, h;
		SkinRectToScreen(image.left, image.top, image.width, image.height, &x, &y, &w, &h);
		DrawTexture(image.texture, x, y, w, h, image.opacity, NULL, 0.0f, NULL);
		return FALSE;
	}
	if (image.source == 0 || skin->m_pEventSink == NULL || (image.source == 2 && !skin->m_bPreviewPlaying))
		return FALSE;
	skin->m_pEventSink->DrawImage(image.source, image.left, image.top, image.width, image.height, image.opacity);
	return !skin->m_bPreviewToggled && image.source == 2 && skin->m_pPreviewVideo->IsOpen();
}

// (inlined in both paths)
static void DrawSkinShape(const SSkinShape &shape)
{
	float x, y, w, h;
	SkinRectToScreen(shape.left, shape.top, shape.width, shape.height, &x, &y, &w, &h);
	if (shape.type == 2)
		DrawBevelBox(x, y, w, h, shape.borderColor, shape.backColor, 1.0f, 1);
	else if (shape.type == 3)
		DrawTriangle(x, y, w, h, shape.direction, shape.borderColor, shape.backColor, 1.0f);
	else
		DrawRect(x, y, w, h, shape.borderColor, shape.backColor, 1.0f, 1);
}

// 0x00100FA6 (this in ebx)
HRESULT CSkin::RenderUnsorted()
{
	// Texts scroll a step every <ScrollSpeed> ms (first after 2 s).
	static DWORD s_dwScrollTick = GetTickCount() + 2000;   // 0x0027A44C
	DWORD now = GetTickCount();
	BOOL step = s_dwScrollTick < now;
	if (step)
		s_dwScrollTick = (DWORD)m_pSettings->m_nScrollSpeed + now;

	for (std::vector<SSkinImage>::iterator it = m_mainImages.begin(); it != m_mainImages.end(); ++it)
	{
		// Quirk: a playing preview ends the drawing (no texts, and the
		// font isn't reset).
		if (DrawSkinImage(this, *it))
			return S_OK;
	}
	for (std::vector<SSkinShape>::iterator it = m_mainShapes.begin(); it != m_mainShapes.end(); ++it)
		DrawSkinShape(*it);

	g_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
	if (m_pInfoFont == NULL)
		return S_OK;
	float savedScaleX = m_pInfoFont->m_fScaleFactorX;
	float savedScaleY = m_pInfoFont->m_fScaleFactorY;
	// (Batched only without <Fonts>.)
	if (m_pFonts == NULL)
		m_pInfoFont->Begin();
	float scale = GetSkinTextScale();
	for (std::vector<SSkinText>::iterator it = m_mainTexts.begin(); it != m_mainTexts.end(); ++it)
	{
		SSkinText &text = *it;
		CXBFont *font = text.font != NULL ? text.font : m_pInfoFont;
		float x, y, w, h;
		SkinRectToScreen(text.left, text.top, text.width, text.height, &x, &y, &w, &h);
		font->SetScaleFactors(text.scaleX * scale, text.scaleY * scale);
		if (h == 0.0f)
			h = font->m_fScaleFactorY * font->m_fFontHeight + 1.0f;
		wchar_t buffer[0x105];
		GetSkinTextString(text, buffer);
		if (buffer[0] == 0)
			continue;
		if (text.scroll == 0 || w <= 0.0f)
		{
			font->DrawText(x, y, text.color, buffer, text.align, w);
			continue;
		}
		const wchar_t *shown = buffer;
		if (m_pSettings->m_bNewsfeedContinuous && text.source == TEXT_NEWSFEED)
			shown = GetNewsText();
		// (This path keeps the offset as a float in the same field.)
		DrawScrollingSkinText(text, font, shown, x, y, w, h, step, TRUE);
	}
	if (m_pFonts == NULL)
		m_pInfoFont->End();
	m_pInfoFont->SetScaleFactors(savedScaleX, savedScaleY);
	return S_OK;
}

// 0x00100948
HRESULT CSkin::RenderSorted()
{
	static DWORD s_dwScrollTick = GetTickCount() + 2000;   // 0x0027A444
	DWORD now = GetTickCount();
	BOOL step = s_dwScrollTick < now;
	if (step)
		s_dwScrollTick = (DWORD)m_pSettings->m_nScrollSpeed + now;
	float scale = GetSkinTextScale();

	for (std::vector<SSkinDrawItem>::iterator it = m_mainOrder.begin(); it != m_mainOrder.end(); ++it)
	{
		if (it->kind == 0)
		{
			if (DrawSkinImage(this, m_mainImages[it->index]))
				return S_OK;
		}
		else if (it->kind == 1)
		{
			DrawSkinShape(m_mainShapes[it->index]);
		}
		else if (it->kind == 2 && m_pInfoFont != NULL)
		{
			SSkinText &text = m_mainTexts[it->index];
			g_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
			CXBFont *font = text.font != NULL ? text.font : m_pInfoFont;
			float x, y, w, h;
			SkinRectToScreen(text.left, text.top, text.width, text.height, &x, &y, &w, &h);
			float savedScaleX = font->m_fScaleFactorX;
			float savedScaleY = font->m_fScaleFactorY;
			// (Here the box is scaled too.)
			if (h == 0.0f)
				h = font->m_fScaleFactorY * font->m_fFontHeight + 2.0f;
			else
				h = scale * h;
			w = scale * w;
			wchar_t buffer[0x105];
			GetSkinTextString(text, buffer);
			if (buffer[0] == 0)
				continue;
			font->SetScaleFactors(text.scaleX * scale, text.scaleY * scale);
			if (text.scroll == 0 || w <= 0.0f)
			{
				font->DrawText(x, y, text.color, buffer, text.align, w);
			}
			else
			{
				const wchar_t *shown = buffer;
				if (m_pSettings->m_bNewsfeedContinuous && text.source == TEXT_NEWSFEED)
					shown = GetNewsText();
				DrawScrollingSkinText(text, font, shown, x, y, w, h, step, TRUE);
			}
			font->SetScaleFactors(savedScaleX, savedScaleY);
		}
	}
	return S_OK;
}

// 0x0010160B (this in eax)
HRESULT CSkin::Render()
{
	if (m_bLoading)
	{
		// The loading picture over the whole display.
		D3DDISPLAYMODE mode;
		g_pd3dDevice->GetDisplayMode(&mode);
		DrawShade(m_pLoadingTexture, 0.0f, 0.0f, (float)mode.Width, (float)mode.Height, 1.0f);
		Sleep(5);
		return S_OK;
	}
	if (m_bSortByZIndex)
		RenderSorted();
	else
		RenderUnsorted();
	// After loading: the picture fades out for a second.
	if (m_pLoadingTexture != NULL)
	{
		DWORD now = GetTickCount();
		if (now < m_dwLoadingFadeEnd)
		{
			DrawShadeFullScreen(m_pLoadingTexture, (float)(m_dwLoadingFadeEnd - now) * 0.001f);
		}
		else
		{
			ReleaseTexture(m_pLoadingTexture);
			m_pLoadingTexture = NULL;
			CloseHandle(m_hLoadThread);
			m_hLoadThread = NULL;
		}
	}
	return S_OK;
}
