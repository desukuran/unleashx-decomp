// CAudioGUI::Render: the skin's <MusicPlayer> elements in z-order, with
// the player's own shapes (buttons, bars, state icons, level meter), then
// the song list and the open pop-up (0x0013101F-0x00135089).
//
// The buttons come from one texture map (the skin's <ButtonMap>, else
// MPlayer.png), an 8x8 grid of 0.125 cells:
//   rows 0-1  the buttons PREV..NEXTTRACK in columns 0-6; column 7 holds
//             RANDOM (row 0) over MUTE (row 1)
//   rows 2-3  the same, lit: the button of the music's state (play, pause,
//             stop), and RANDOM/MUTE when on
//   rows 4-5  the same, focused
//   row 6     the progress bar: track (left half), fill (right half)
//   row 7     the volume bar, the same way
//
// Unused: RenderBuiltIn (0x00135089), an older built-in player window drawn
// from the layout Show computes. Nothing calls it.
#include "AudioGUI.h"

#include <cmath>
#include <cwchar>

#include "../audio/AudioDevice.h"
#include "../audio/AudioPlayer.h"
#include "../audio/MusicManager.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../skin/SkinText.h"
#include "../ui/FileBrowser.h"
#include "SongList.h"
#include "TrackOptions.h"

// A colored, textured screen vertex (D3DFVF_XYZRHW | D3DFVF_DIFFUSE |
// D3DFVF_TEX1).
struct SButtonVertex
{
	float x, y, z, rhw;
	D3DCOLOR color;
	float u, v;
};

// One cell of the button map over (left, top)-(right, bottom). rotation
// (0-3) turns the picture a quarter at a time: corner i of the box gets
// texture corner i + rotation (clockwise from the top left).
static void DrawMapQuad(float left, float top, float right, float bottom, float u0, float v0, float u1, float v1,
                        D3DCOLOR color, int rotation)
{
	const float corners[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
	const float positions[4][2] = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
	SButtonVertex v[4];
	for (int i = 0; i < 4; i++)
	{
		v[i].x = positions[i][0] - 0.5f;
		v[i].y = positions[i][1] - 0.5f;
		v[i].z = 1.0f;
		v[i].rhw = 1.0f;
		v[i].color = color;
		v[i].u = corners[(i + rotation) & 3][0];
		v[i].v = corners[(i + rotation) & 3][1];
	}
	g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
	g_pd3dDevice->DrawVerticesUP(D3DPT_QUADLIST, 4, v, sizeof(SButtonVertex));
}

// A bar (row v0..v1 of the map): the track, then the part filled to
// fraction (0..1) from the side the shape's direction names.
static void DrawMapBar(float x, float y, float width, float height, float v0, float v1, float fraction,
                       int direction)
{
	int rotation = 0;
	if (direction == DIR_DOWN)
		rotation = 1;
	else if (direction == DIR_LEFT)
		rotation = 2;
	else if (direction == DIR_OTHER)
		rotation = 3;
	DrawMapQuad(x, y, x + width, y + height, 0.0f, v0, 0.5f, v1, 0xFFFFFFFF, rotation);

	float left = x, top = y, right = x + width, bottom = y + height;
	if (direction == DIR_DOWN)
		bottom = fraction * height + y;               // from the top
	else if (direction == DIR_LEFT)
		left = (1.0f - fraction) * width + x;         // from the right
	else if (direction == DIR_OTHER)
		top = (1.0f - fraction) * height + y;         // from the bottom
	else
		right = fraction * width + x;                 // from the left
	DrawMapQuad(left, top, right, bottom, 0.5f, v0, fraction * 0.5f + 0.5f, v1, 0xFFFFFFFF, rotation);
}

// 0x0013101F
void CAudioGUI::DrawButton(IDirect3DTexture8 *texture, UINT id, float x, float y, float width, float height,
                           int direction)
{
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	g_pd3dDevice->SetTexture(0, texture);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	// (The original writes these into D3D's texture stage state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);

	float right = x + width;
	float bottom = y + height;
	D3DCOLOR color = 0xFFFFFFFF;
	if (id < BUTTON_RANDOM)
	{
		// Faded when disabled; lit when it's the music's state.
		color = m_buttons[id].enabled ? 0xFFFFFFFF : 0x80FFFFFF;
		float u = (float)id * 0.125f;
		DrawMapQuad(x, y, right, bottom, u, 0.0f, u + 0.125f, 0.25f, color, 0);
		int state = g_pMusicManager->GetState();
		if ((state == MUSIC_STOPPED && id == BUTTON_STOP) || (state == MUSIC_PAUSED && id == BUTTON_PAUSE) ||
		    (state == MUSIC_PLAYING && id == BUTTON_PLAY))
			DrawMapQuad(x, y, right, bottom, u, 0.25f, u + 0.125f, 0.5f, color, 0);
	}
	else if (id == BUTTON_RANDOM || id == BUTTON_MUTE)
	{
		float v = id == BUTTON_MUTE ? 0.125f : 0.0f;
		if (m_buttons[id].on)
			v += 0.25f;
		DrawMapQuad(x, y, right, bottom, 0.875f, v, 1.0f, v + 0.125f, 0xFFFFFFFF, 0);
	}
	else if (id == BUTTON_VOLUME)
	{
		float volume = (float)m_pSettings->m_audio.m_nMusicVolume * 0.01f;
		DrawMapBar(x, y, width, height, 0.875f, 1.0f, volume, direction);
	}
	else if (id == BUTTON_PROGRESS)
	{
		SSongFormat *format = g_pMusicManager->GetSongFormat();
		if (format == NULL || format->duration == 0)
		{
			g_pd3dDevice->SetTexture(0, NULL);
			return;
		}
		float position = g_pMusicManager->GetSongPosition() / (float)format->duration;
		if (position > 1.0f)
			position = 1.0f;
		DrawMapBar(x, y, width, height, 0.75f, 0.875f, position, direction);
	}

	// The focused button.
	if (id == m_nButton && id < BUTTON_VOLUME)
	{
		float u0 = (float)id * 0.125f;
		float u1 = u0 + 0.125f;
		float v0 = 0.5f;
		float v1 = 0.75f;
		if (id == BUTTON_RANDOM)
		{
			u0 = 0.875f;
			u1 = 1.0f;
			v1 = 0.625f;
		}
		else if (id == BUTTON_MUTE)
		{
			u0 = 0.875f;
			u1 = 1.0f;
			v0 = 0.625f;
		}
		DrawMapQuad(x, y, right, bottom, u0, v0, u1, v1, color, 0);
	}
	g_pd3dDevice->SetTexture(0, NULL);
}

// 0x00132308
void DrawLevelMeter(float x, float y, float width, float height)
{
	DSOUTPUTLEVELS levels;
	IDirectSound_GetOutputLevels(CAudioDevice::GetInstance()->GetDirectSound(), &levels, FALSE);
	DWORD bars[12] = {levels.dwAnalogLeftTotalPeak, levels.dwAnalogRightTotalPeak, levels.dwAnalogLeftTotalRMS,
	                  levels.dwAnalogRightTotalRMS, levels.dwDigitalFrontLeftPeak, levels.dwDigitalFrontLeftRMS,
	                  levels.dwDigitalFrontRightPeak, levels.dwDigitalFrontRightRMS, levels.dwDigitalFrontCenterPeak,
	                  levels.dwDigitalLowFrequencyPeak, levels.dwDigitalBackLeftPeak, levels.dwDigitalBackRightPeak};

	float barX = x + 5.0f;
	height -= 6.0f;
	float barBottom = y + height;
	float step = (width - 10.0f) * (1.0f / 12.0f);
	float barWidth = (float)floor(step * 0.8f);

	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	// Quirk: the top color is the dark color with the red channel set
	// from the level times (|dark - light| >> 16).
	int colorRange = abs((int)(g_colorScheme.MMDarkBarColor - g_colorScheme.MMLightBarColor)) >> 16;
	g_pd3dDevice->Begin(D3DPT_QUADLIST);
	for (int i = 0; i < 12; i++)
	{
		float level = (float)bars[i];
		float db = -60.0f;
		if (fabs(level) * (1.0f / 8388608.0f) != 0.0f)
			db = (float)(log10(fabs(level) * (1.0f / 8388608.0f)) * 20.0);
		float fraction = (db + 60.0f) * (1.0f / 60.0f);
		if (fraction > 0.0f)
		{
			D3DCOLOR top = ((DWORD)(int)((float)colorRange * fraction) << 16) | g_colorScheme.MMDarkBarColor;
			float barTop = barBottom - fraction * height;
			g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, top);
			g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, barX, barTop, 0.0f, 1.0f);
			g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, barWidth + barX, barTop, 0.0f, 1.0f);
			g_pd3dDevice->SetVertexDataColor(D3DVSDE_DIFFUSE, g_colorScheme.MMDarkBarColor);
			g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, barWidth + barX, barBottom, 0.0f, 1.0f);
			g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, barX, barBottom, 0.0f, 1.0f);
		}
		barX = step + barX;
	}
	g_pd3dDevice->End();
}

// 0x001325B6
void DrawStateIcon(int kind, float x, float y, float width, float height)
{
	int state = g_pMusicManager->GetState();
	DWORD color = g_colorScheme.MMForeColor;
	if (kind == 0)
	{
		// Stop: a square.
		DrawRect(x, y, width, height, 0, color, state == MUSIC_STOPPED ? 1.0f : 0.1f, 1);
	}
	else if (kind == 1)
	{
		// Play: a triangle pointing right.
		DrawTriangle(x, y, width, height, 4, 0, color, state == MUSIC_PLAYING ? 1.0f : 0.1f);
	}
	else if (kind == 2)
	{
		// Pause: two bars, 0.3 of the width each.
		float alpha = state == MUSIC_PAUSED ? 1.0f : 0.1f;
		float barWidth = width * 0.3f;
		DrawRect(x, y, (float)floor(barWidth), height, 0, color, alpha, 1);
		DrawRect((float)floor(width * 0.7 + x), y, (float)floor(barWidth), height, 0, color, alpha, 1);
	}
}

// 0x001333BA
void CAudioGUI::DrawKey(float x, float y, float width, float height, const wchar_t *text, bool enabled,
                        bool pressed, bool focused)
{
	DWORD factor = 0xFFFFFFFF;
	if (enabled)
	{
		if (pressed)
			factor = g_colorScheme.KB_KEY_PRESSED;
		if (focused)
			factor |= g_colorScheme.KB_KEYBG_HIGHLIGHT;
	}
	DrawPanelTexture(m_pKeyboardTexture, x, y, width, height, factor);   // (inlined)
	if (focused)
		DrawBevelBox(x - 1.0f, y - 1.0f, (x + width + 1.0f) - x, (y + height + 1.0f) - y,
		             g_colorScheme.KB_KEY_ACTIVEBORDER, 0, 1.0f, 1);
	m_pFont->DrawText(width * 0.5f + x, height * 0.5f + y, g_colorScheme.KB_KEY_NORMAL, text,
	                  XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
}

// 0x00134853
BOOL CAudioGUI::Render()
{
	DimScreen(0.3f);
	// Texts scroll a step every <ScrollSpeed> ms (first after 2 s).
	static DWORD s_dwScrollTick = GetTickCount() + 2000;   // 0x0027A41C
	DWORD now = GetTickCount();
	BOOL step = s_dwScrollTick < now;
	if (step)
		s_dwScrollTick = (DWORD)m_pSettings->m_nScrollSpeed + now;
	float scale = GetSkinTextScale();

	// The player's background.
	float x, y, w, h;
	SkinRectToFloats(g_skin.m_rcMusicPlayer.left, g_skin.m_rcMusicPlayer.top, g_skin.m_rcMusicPlayer.width,
	                 g_skin.m_rcMusicPlayer.height, &x, &y, &w, &h);
	if (w != 0.0f)
	{
		ScaleToScreen(&x, &w, &y, &h, FALSE);
		if (g_skin.m_pMusicPlayerBackground != NULL)
			DrawTexture(g_skin.m_pMusicPlayerBackground, x, y, w, h, 1.0f, NULL, 0.0f, NULL);
		else
			DrawPanelTexture(m_pKeyboardTexture, x, y, w, h);   // (its own copy, 0x00132781)
	}

	BOOL playing = g_pMusicManager->GetState() == MUSIC_PLAYING;
	IDirect3DTexture8 *buttonMap = g_skin.m_pButtonMap != NULL ? g_skin.m_pButtonMap : m_pButtonTexture;

	for (std::vector<SSkinDrawItem>::iterator it = g_skin.m_musicOrder.begin(); it != g_skin.m_musicOrder.end();
	     ++it)
	{
		if (it->kind == 0)
		{
			// An image (without the main screen's stop for the preview).
			SSkinImage &image = g_skin.m_musicImages[it->index];
			LoadSkinImage(&image);
			if (image.texture != NULL)
			{
				SkinRectToScreen(image.left, image.top, image.width, image.height, &x, &y, &w, &h);
				DrawTexture(image.texture, x, y, w, h, image.opacity, NULL, 0.0f, NULL);
			}
			else if (image.source != 0 && g_skin.m_pEventSink != NULL &&
			         (image.source != 2 || g_skin.m_bPreviewPlaying))
			{
				g_skin.m_pEventSink->DrawImage(image.source, image.left, image.top, image.width, image.height,
				                               image.opacity);
			}
		}
		else if (it->kind == 1)
		{
			SSkinShape &shape = g_skin.m_musicShapes[it->index];
			SkinRectToScreen(shape.left, shape.top, shape.width, shape.height, &x, &y, &w, &h);
			switch (shape.type)
			{
			case SHAPE_METER:
				if (playing)
					DrawLevelMeter(x, y, w, h);
				break;
			case SHAPE_ROUNDEDBOX:
				DrawBevelBox(x, y, w, h, shape.borderColor, shape.backColor, 1.0f, 1);
				break;
			case SHAPE_TRIANGLE:
				DrawTriangle(x, y, w, h, shape.direction, shape.borderColor, shape.backColor, 1.0f);
				break;
			case SHAPE_STOPSTATUS:
			case SHAPE_PLAYSTATUS:
			case SHAPE_PAUSESTATUS:
				DrawStateIcon(shape.type - SHAPE_STOPSTATUS, x, y, w, h);
				break;
			case SHAPE_SONGSTATUS:
			{
				// The icon of the current state.
				int state = g_pMusicManager->GetState();
				DrawStateIcon(state == MUSIC_PLAYING ? 1 : state == MUSIC_PAUSED ? 2 : 0, x, y, w, h);
				break;
			}
			default:
				if (shape.type >= SHAPE_PREV && shape.type <= SHAPE_PROGRESS)
					DrawButton(buttonMap, shape.type - SHAPE_PREV, x, y, w, h, shape.direction);
				else
					DrawRect(x, y, w, h, shape.borderColor, shape.backColor, 1.0f, 1);
				break;
			}
		}
		else if (it->kind == 2 && g_skin.m_pInfoFont != NULL)
		{
			SSkinText &text = g_skin.m_musicTexts[it->index];
			CXBFont *font = text.font != NULL ? text.font : g_skin.m_pInfoFont;
			SkinRectToScreen(text.left, text.top, text.width, text.height, &x, &y, &w, &h);
			float savedScaleX = font->m_fScaleFactorX;
			float savedScaleY = font->m_fScaleFactorY;
			font->SetScaleFactors(text.scaleX * scale, text.scaleY * scale);
			if (h == 0.0f)
				h = font->m_fFontHeight * font->m_fScaleFactorY + 1.0f;
			wchar_t buffer[0x105];
			GetSkinTextString(text, buffer);
			if (buffer[0] == 0)
				continue;   // (the font keeps the text's scale)
			if (text.scroll == 0 || w <= 0.0f)
				font->DrawText(x, y, text.color, buffer, text.align, w);
			else
				// Texts only scroll while the music plays.
				DrawScrollingSkinText(text, font, buffer, x, y, w, h, playing && step, playing);
			font->SetScaleFactors(savedScaleX, savedScaleY);
		}
	}

	if (m_bSongListShown)
		m_pSongList->Render();
	if (m_bMenuOpen)
		m_menu.Render();
	else if (m_bTrackOptionsOpen)
		m_pTrackOptions->Render();
	else if (m_bDialogOpen)
		m_pDialog->Render();
	else if (m_bBrowserOpen)
		m_pBrowser->Render();
	return FALSE;
}

// --- The unused built-in player window --------------------------------------

#define BUILTIN_TEXT 0xFFF2F2F2
#define BUILTIN_BORDER 0xFF000080

// A textured screen vertex (D3DFVF_XYZRHW | D3DFVF_TEX1).
struct SBuiltInVertex
{
	float x, y, z, rhw;
	float u, v;
};

// The scrolling "<soundtrack> - <song>" line, shared by all windows.
static float s_fBuiltInScroll;           // 0x00270F8C
static DWORD s_dwBuiltInScrollTime;      // 0x0027A424 (set 2 s ahead on the first call)
static BOOL s_bBuiltInScrollInit;        // 0x0027A428

// 0x00135089 (this in ebx; nothing calls it)
// The keyboard texture as the panel, a bevel box with the time, the stop /
// play / pause icons (the current one lit), the format texts, a box with
// the soundtrack and song scrolling through it, and the level meter's
// background (no meter).
void CAudioGUI::RenderBuiltIn()
{
	float left = m_fX - 0.5f;
	float top = m_fY - 0.5f;
	float right = m_fWidth + m_fX - 0.5f;
	float bottom = m_fHeight + m_fY - 0.5f;
	SBuiltInVertex v[4] = {
		{left, top, 1.0f, 1.0f, 0.1f, 0.1f},
		{right, top, 1.0f, 1.0f, 0.9f, 0.1f},
		{right, bottom, 1.0f, 1.0f, 0.9f, 0.9f},
		{left, bottom, 1.0f, 1.0f, 0.1f, 0.9f},
	};
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
	g_pd3dDevice->SetTexture(0, m_pKeyboardTexture);
	// (The original writes these into D3D's texture stage state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
	g_pd3dDevice->DrawVerticesUP(D3DPT_POLYGON, 4, v, sizeof(SBuiltInVertex));

	float margin = m_fScaleY * 5.0f;
	float innerWidth = m_fWidth - margin * 2.0f;
	DrawBevelBox(margin + m_fX, m_fY + margin, innerWidth, m_fInfoHeight, BUILTIN_BORDER, 0xFF3B5A92, 0.95f, 2);
	m_pFont->SetScaleFactors(m_fTitleScale, m_fTitleScale);
	m_pFont->DrawText(m_fTitleX, m_fTitleY, BUILTIN_TEXT, GetSkinTextValue(TEXT_SONGTIME), 0, 0.0f);

	// The state icons, one under the other: stop, play, pause.
	float iconGap = (m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight - m_fMargin * 3.0f) * 0.5f;
	int state = g_pMusicManager->GetState();
	if (state == MUSIC_STOPPED || state == MUSIC_PAUSED || state == MUSIC_PLAYING)
	{
		float x = margin * 3.0f + m_fX;
		float y = m_fTitleY;
		float stopAlpha = state == MUSIC_STOPPED ? 1.0f : 0.1f;
		float playAlpha = state == MUSIC_PLAYING ? 1.0f : 0.1f;
		float pauseAlpha = state == MUSIC_PAUSED ? 1.0f : 0.1f;
		DrawRect(x, y, m_fMargin, m_fMargin, 0, BUILTIN_TEXT, stopAlpha, 1);
		y = m_fMargin + iconGap + y;
		DrawTriangle(x, y, m_fMargin, m_fMargin, 4, 0, BUILTIN_TEXT, playAlpha);
		y = m_fMargin + iconGap + y;
		// (Paused draws the bars 0.3 wide at 0 and 0.7; otherwise 0.3 wide at
		// 0 and 0.7 too, through float constants instead of doubles.)
		DrawRect(x, y, m_fMargin * 0.3f, m_fMargin, 0, BUILTIN_TEXT, pauseAlpha, 1);
		DrawRect(m_fMargin * 0.7f + x, y, m_fMargin * 0.3f, m_fMargin, 0, BUILTIN_TEXT, pauseAlpha, 1);
	}

	// The format: bit rate, sample rate / length, channels.
	if (m_szChannels[0] != 0)
	{
		float scale = (float)floor((m_fSmallScale + 0.24f) * 4.0f) * 0.25f;
		m_pFont->SetScaleFactors(scale, scale);
		float rowHeight = (float)ceil(m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight) + 2.0f;
		float column = (m_fMeterX - m_fTimeX) * 0.5f;
		m_pFont->DrawText(m_fTimeX, m_fTimeY, BUILTIN_TEXT, m_szBitRate, 0, 0.0f);
		m_pFont->DrawText(m_fTimeX + column, m_fTimeY, BUILTIN_TEXT, m_szSampleRate, 0, 0.0f);
		m_pFont->DrawText(m_fTimeX, m_fTimeY + rowHeight, BUILTIN_TEXT, m_szLength, 0, 0.0f);
		m_pFont->DrawText(m_fTimeX + column, m_fTimeY + rowHeight, BUILTIN_TEXT, m_szChannels, 0, 0.0f);
	}

	// The soundtrack and song, scrolling one pixel per scroll step inside
	// the box.
	m_pFont->SetScaleFactors(m_fInfoScale, m_fInfoScale);
	float boxHeight = m_fScaleY * 30.0f;
	DrawBevelBox(m_fStateX, m_fStateY, innerWidth, boxHeight, 0, BUILTIN_BORDER, 0.8f, 1);
	wchar_t line[250];
	swprintf(line, 250, L"%ls - %ls", GetSkinTextValue(TEXT_SOUNDTRACK), GetSkinTextValue(TEXT_SONGNAME));
	if (!s_bBuiltInScrollInit)
	{
		s_bBuiltInScrollInit = TRUE;
		s_dwBuiltInScrollTime = GetTickCount() + 2000;
	}
	DWORD now = GetTickCount();
	BOOL step = FALSE;
	if (s_dwBuiltInScrollTime < now)
	{
		s_dwBuiltInScrollTime = m_pSettings->m_nScrollSpeed + now;
		step = TRUE;
	}
	float textX = m_fStateX + margin;
	float textY = margin + m_fStateY;
	float screenWidth = 640.0f, screenHeight = 480.0f;
	GetDisplaySize(&screenWidth, &screenHeight, FALSE);
	D3DRECT clip;
	clip.x1 = (long)textX;
	clip.y1 = (long)textY;
	clip.x2 = (long)(innerWidth + textX > screenWidth ? screenWidth : innerWidth + textX);
	clip.y2 = (long)(boxHeight + textY > screenHeight ? screenHeight : boxHeight + textY);
	g_pd3dDevice->SetScissors(1, FALSE, &clip);
	float textWidth = m_pFont->GetTextWidth(line);
	if (step)
	{
		// Past the left edge: start again from the right.
		if ((float)clip.x1 > textWidth + textX - s_fBuiltInScroll)
			s_fBuiltInScroll = 0.0f - innerWidth;
		else
			s_fBuiltInScroll += 1.0f;
	}
	m_pFont->DrawText(textX - s_fBuiltInScroll, margin + m_fStateY, BUILTIN_TEXT, line, 0, 0.0f);
	g_pd3dDevice->SetScissors(0, FALSE, NULL);

	DrawRect(m_fMeterX, m_fMeterY, m_fMeterWidth, m_fMeterHeight, 0, BUILTIN_BORDER, 0.4f, 1);
}
