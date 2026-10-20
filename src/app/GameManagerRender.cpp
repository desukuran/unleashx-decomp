// CGameManager's drawing (0x00119C91-0x0011BA03, 0x0011C8F4): the MS
// dashboard style panels of the game saves manager. Each panel is a
// polygon with cut corners, in the dialog colors; sizes are at 640x480
// and grow with m_fScaleX/m_fScaleY. See GameManager.h.
#include "GameManager.h"

#include <cmath>
#include <cwchar>

#include "../common/StrUtil.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../skin/SkinColors.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"

// The selected icon's pulsing frame.
static float s_fIconPulse = 1.0f;      // 0x0026C668
static float s_fIconPulseStep;         // 0x0026C66C

static void SetVertex(SScreenVertex *v, float x, float y, float rhw)
{
	v->x = x;
	v->y = y;
	v->z = 0.0f;
	v->rhw = rhw;
}

static float LineHeight(const CXBFont *font)
{
	return font->m_fScaleFactorY * font->m_fFontHeight;
}

// The rows of a game's save grid (1 to 3).
static int SaveRows(const SGameEntry *game)
{
	int rows = (int)ceil((float)(UINT)game->saves.size() * 0.25f);
	if (rows > 3)
		return 3;
	return rows < 1 ? 1 : rows;
}

// 0x0011AB23
// The title bar: the "Xbox Game Saves" plate on the left (with the hard
// disk's free space) and a long plate to the right.
void CGameManager::DrawHeader(float x, float y)
{
	float sx = m_fScaleX;
	float sy = m_fScaleY;
	float right = m_fWidth + m_fX;
	SScreenVertex v[11];

	float left = (sx * 10.0f + x) - 0.5f;
	float top = (sy * 36.0f + y) - 0.5f;
	SetVertex(&v[0], sx * 8.0f + left, top, 1.0f);
	SetVertex(&v[1], sx * 42.0f + left, top, 1.0f);
	SetVertex(&v[2], sx * 68.0f + left, sy * 24.0f + top, 1.0f);
	SetVertex(&v[3], sx * 136.0f + left, sy * 24.0f + top, 1.0f);
	SetVertex(&v[4], sx * 120.0f + left, sy * 54.0f + top, 1.0f);
	SetVertex(&v[5], sx * 4.0f + left, sy * 54.0f + top, 1.0f);
	SetVertex(&v[6], left, sy * 50.0f + top, 1.0f);
	SetVertex(&v[7], left, sy * 8.0f + top, 1.0f);
	DrawPolygon(v, 8, g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_BORDERCOLOR, 2.0f, 1);

	float r = right - 0.5f;
	float t = y - 0.5f;
	float l = (sx * 18.0f + x) - 0.5f;
	float bottom = (sy * 66.0f + y) - 0.5f;
	SetVertex(&v[0], r - sx * 86.0f, t, 1.0f);
	SetVertex(&v[1], r - sx * 68.0f, sy * 20.0f + t, 1.0f);
	SetVertex(&v[2], r - sx * 28.0f, sy * 20.0f + t, 1.0f);
	SetVertex(&v[3], r, bottom - sy * 12.0f, 1.0f);
	SetVertex(&v[4], r, bottom, 1.0f);
	SetVertex(&v[5], r - sx * 76.0f, bottom, 1.0f);
	SetVertex(&v[6], r - sx * 82.0f, bottom - sy * 8.0f, 1.0f);
	SetVertex(&v[7], sx * 60.0f + l, bottom - sy * 8.0f, 1.0f);
	SetVertex(&v[8], sx * 34.0f + l, sy * 34.0f + t, 1.0f);
	SetVertex(&v[9], l, sy * 34.0f + t, 1.0f);
	SetVertex(&v[10], sx * 28.0f + l, t, 1.0f);
	DrawPolygon(v, 11, g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_BORDERCOLOR, 1.0f, 3);

	if (m_pTitleFont != NULL)
	{
		float textX = sx * 120.0f + x;
		float textY = sy * 16.0f + y;
		m_pTitleFont->SetScaleFactors(sx, sy);
		m_pTitleFont->DrawText(textX, textY, g_colorScheme.CAPTION_TEXTCOLOR, L"Xbox Game Saves", XBFONT_CENTER_Y,
		                       0.0f);
		SDriveInfo *drive = GetDriveInfo('E');
		if (drive != NULL)
		{
			// In MB, or in KB under 10 MB.
			double bytes = (double)(LONGLONG)drive->freeBytes;
			double amount = bytes * (1.0 / 1048576.0);
			BOOL small = amount < 10.0;
			if (small)
				amount = bytes * (1.0 / 1024.0);
			wchar_t text[50];
			FormatDoubleW(text, 2, TRUE, amount);
			wcscat(text, small ? L" KB free" : L" MB free");
			m_pTitleFont->DrawText(textX, LineHeight(m_pTitleFont) + textY, g_colorScheme.CAPTION_TEXTCOLOR, text,
			                       XBFONT_CENTER_Y, 0.0f);
		}
	}
}

// 0x0011B123
// A game's name plate, from x to the right edge.
void CGameManager::DrawTab(float x, float y, const wchar_t *text, DWORD textColor, DWORD fillColor,
                           DWORD borderColor, float alpha, DWORD lineWidth)
{
	float sx = m_fScaleX;
	float sy = m_fScaleY;
	float right = m_fWidth + m_fX;
	float bottom = (LineHeight(m_pFont) + sy * 8.0f + y) - 0.5f;
	SScreenVertex v[6];
	SetVertex(&v[0], (x - sx * 10.0f) - 0.5f, y - 0.5f, 1.0f);
	SetVertex(&v[1], (right - sx * 16.0f) - 0.5f, y - 0.5f, 1.0f);
	SetVertex(&v[2], right - 0.5f, sy * 22.0f + (y - 0.5f), 1.0f);
	SetVertex(&v[3], right - 0.5f, bottom, 1.0f);
	SetVertex(&v[4], sx * 5.0f + (x - 0.5f), bottom, 1.0f);
	SetVertex(&v[5], x - 0.5f, sy * 5.0f + bottom, 1.0f);
	DrawPolygon(v, 6, fillColor, borderColor, alpha, lineWidth);
	if (text != NULL)
	{
		// Quirk: the width is taken from y, not x.
		m_pFont->DrawText(sx * 10.0f + x, LineHeight(m_pFont) * 0.5f + sy * 4.0f + y, textColor, text,
		                  XBFONT_CENTER_Y | 0x10, (right - y) - 20.0f);
	}
}

// 0x0011A4A1
// The box under a game's name plate that holds its save grid, with the
// grid's scroll arrows for the selected game.
void CGameManager::DrawSavePanel(float x, float y, int game, DWORD fillColor, DWORD borderColor, float alpha)
{
	float sx = m_fScaleX;
	float sy = m_fScaleY;
	float left = x - sx * 10.0f;
	float right = m_fWidth + m_fX;
	float top = LineHeight(m_pFont) + sy * 12.0f + y;
	float bottom = (float)(SaveRows(m_games[game]) * 70) * sy + top;
	SScreenVertex v[8];
	float t = top - 0.5f;
	float l = left - 0.5f;
	float b = bottom - 0.5f;
	SetVertex(&v[0], l, t, 1.0f);
	SetVertex(&v[1], right - 0.5f, t, 1.0f);
	SetVertex(&v[2], right - 0.5f, b - sy * 14.0f, 1.0f);
	SetVertex(&v[3], (right - 0.5f) - sy * 8.0f, b, 1.0f);   // (sy for an x)
	SetVertex(&v[4], sx * 5.0f + (x - 0.5f), b, 1.0f);
	SetVertex(&v[5], x - 0.5f, sy * 5.0f + b, 1.0f);
	SetVertex(&v[6], l, b - sy * 7.0f, 1.0f);
	SetVertex(&v[7], l, sy * 22.0f + t, 1.0f);
	DrawPolygon(v, 8, fillColor, borderColor, alpha, 1);

	if (game != m_nGame)
		return;
	float arrowX = (right - sx * 18.0f) - 0.5f;
	if (m_nSaveRow > 2)
	{
		// More saves above.
		float arrowTop = (LineHeight(m_pFont) + sy * 14.0f + y) - 0.5f;
		SetVertex(&v[0], arrowX, sy * 12.0f + arrowTop, 1.0f);
		SetVertex(&v[1], sx * 8.0f + arrowX, arrowTop, 1.0f);
		SetVertex(&v[2], sx * 17.0f + arrowX, sy * 12.0f + arrowTop, 1.0f);
		SetVertex(&v[3], sx * 13.0f + arrowX, sy * 12.0f + arrowTop, 1.0f);
		SetVertex(&v[4], sx * 8.0f + arrowX, sy * 5.0f + arrowTop, 1.0f);
		SetVertex(&v[5], sx * 5.0f + arrowX, sy * 12.0f + arrowTop, 1.0f);
		DrawPolygon(v, 6, g_colorScheme.CAPTION_BACKCOLOR, 0, 1.0f, 1);
	}
	if ((m_nSaveRow / 3 + 1) * 3 < m_nSaveRows)
	{
		// More below. (sx for a y here.)
		float arrowBottom = (bottom - sx * 2.0f) - 0.5f;
		float arrowTop = arrowBottom - sy * 12.0f;
		SetVertex(&v[0], arrowX, arrowTop, 1.0f);
		SetVertex(&v[1], sx * 8.0f + arrowX, arrowBottom, 1.0f);
		SetVertex(&v[2], sx * 17.0f + arrowX, arrowTop, 1.0f);
		SetVertex(&v[3], sx * 13.0f + arrowX, arrowTop, 1.0f);
		SetVertex(&v[4], sx * 8.0f + arrowX, arrowBottom - sy * 5.0f, 1.0f);
		SetVertex(&v[5], sx * 5.0f + arrowX, arrowTop, 1.0f);
		DrawPolygon(v, 6, g_colorScheme.CAPTION_BACKCOLOR, 0, 1.0f, 1);
	}
}

// A textured, colored vertex (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1).
struct SIconVertex
{
	float x, y, z, rhw;
	D3DCOLOR color;
	float u, v;
};

static void SetIconVertex(SIconVertex *v, float x, float y, D3DCOLOR color, float u, float w)
{
	v->x = x;
	v->y = y;
	v->z = 1.0f;
	v->rhw = 1.0f;
	v->color = color;
	v->u = u;
	v->v = w;
}

// 0x0011B33C
// A game's or a save's picture: a 12-cornered (chamfered) textured tile;
// the selected one gets a pulsing frame.
void CGameManager::DrawIcon(float x, float y, float width, float height, IDirect3DTexture8 *texture, float alpha,
                            BOOL selected)
{
	if (texture == NULL)
		return;
	float left = (float)floor(x);
	float top = (float)floor(y);
	float w = (float)floor((x - left) + width);
	float h = (float)floor((y - top) + height);
	float innerLeft = m_fScaleX + m_fScaleX + left;
	float innerTop = m_fScaleY + m_fScaleY + top;
	float innerRight = (left + w) - m_fScaleX * 4.0f;
	float innerBottom = (top + h) - m_fScaleY * 4.0f;
	float cutX = w * 0.1f;
	float cutY = h * 0.1f;
	D3DCOLOR color = ((DWORD)(int)(alpha * 255.0f) << 24) | 0xFFFFFF;
	if (selected)
	{
		float margin = m_fScaleY * 2.0f;
		if (s_fIconPulse >= 1.0f)
			s_fIconPulseStep = -1.0f;
		else if (s_fIconPulse <= 0.1f)
			s_fIconPulseStep = 1.0f;
		s_fIconPulse = s_fIconPulseStep * 0.008f + s_fIconPulse;
		DrawBevelBox(left - margin, top - margin, margin * 2.0f + w, margin * 2.0f + h,
		             g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.CAPTION_BACKCOLOR, s_fIconPulse, 1);
	}
	float l = innerLeft - 0.5f;
	float t = innerTop - 0.5f;
	float r = innerRight - 0.5f;
	float b = innerBottom - 0.5f;
	SIconVertex v[12];
	SetIconVertex(&v[0], l + cutX, t, color, 0.1f, 0.0f);
	SetIconVertex(&v[1], (innerRight - cutX) - 0.5f, t, color, 0.9f, 0.0f);
	SetIconVertex(&v[2], r - cutX * 0.5f, cutY * 0.5f + t, color, 0.95f, 0.05f);
	SetIconVertex(&v[3], r, t + cutY, color, 1.0f, 0.1f);
	SetIconVertex(&v[4], r, b - cutY, color, 1.0f, 0.9f);
	SetIconVertex(&v[5], (innerRight - cutX * 0.5f) - 0.5f, b - cutY * 0.5f, color, 0.95f, 0.95f);
	SetIconVertex(&v[6], (innerRight - cutX) - 0.5f, b, color, 0.9f, 1.0f);
	SetIconVertex(&v[7], (cutX + innerLeft) - 0.5f, b, color, 0.1f, 1.0f);
	SetIconVertex(&v[8], (cutX * 0.5f + innerLeft) - 0.5f, b - cutY * 0.5f, color, 0.05f, 0.95f);
	SetIconVertex(&v[9], l, b - cutY, color, 0.0f, 0.9f);
	SetIconVertex(&v[10], l, t + cutY, color, 0.0f, 0.1f);
	SetIconVertex(&v[11], cutX * 0.5f + l, cutY * 0.5f + t, color, 0.05f, 0.05f);

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
	g_pd3dDevice->DrawVerticesUP(D3DPT_POLYGON, 12, v, sizeof(SIconVertex));
}

// 0x00119C91
// The plate at the left with the selected game's (or save's) details.
// (Quirks: some sizes use the other axis' scale.)
void CGameManager::DrawInfoPanel(float x, float y)
{
	float sx = m_fScaleX;
	float sy = m_fScaleY;
	SScreenVertex v[10];
	float l = (x - sx * 10.0f) - 0.5f;
	float t = y - 0.5f;
	SetVertex(&v[0], x - 0.5f, t, 0.0f);
	SetVertex(&v[1], sx * 62.0f + l, t, 0.0f);
	SetVertex(&v[2], sx * 70.0f + l, sy * 10.0f + t, 0.0f);
	SetVertex(&v[3], sx * 98.0f + l, sy * 10.0f + t, 0.0f);
	SetVertex(&v[4], sx * 108.0f + l, sy * 22.0f + t, 0.0f);
	SetVertex(&v[5], sx * 26.0f + l, sy * 22.0f + t, 0.0f);
	SetVertex(&v[6], sx * 12.0f + l, sy * 34.0f + t, 0.0f);
	SetVertex(&v[7], sx * 12.0f + l, sy * 88.0f + t, 0.0f);
	SetVertex(&v[8], l, sy * 74.0f + t, 0.0f);
	SetVertex(&v[9], l, sy * 12.0f + t, 0.0f);
	DrawPolygon(v, 10, g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_BORDERCOLOR, 1.0f, 2);

	float textX = sx * 14.0f + (x - sx * 10.0f);
	float textY = sy * 18.0f + y;
	float cut = sx * 14.0f;
	float right = sy * 172.0f + textX;
	float l2 = textX - 0.5f;
	float t2 = textY - 0.5f;
	float r2 = right - 0.5f;
	float b2 = (sx * 150.0f + textY) - 0.5f;
	SetVertex(&v[0], cut + l2, sy * 6.0f + t2, 0.0f);
	SetVertex(&v[1], sx * 82.0f + l2, sy * 6.0f + t2, 0.0f);
	SetVertex(&v[2], sx * 87.0f + l2, t2, 0.0f);
	SetVertex(&v[3], r2 - cut, t2, 0.0f);
	SetVertex(&v[4], r2, sy * 18.0f + t2, 0.0f);
	SetVertex(&v[5], r2, b2 - sy * 18.0f, 0.0f);
	SetVertex(&v[6], r2 - cut, b2, 0.0f);
	SetVertex(&v[7], sx * 4.0f + l2, b2, 0.0f);
	SetVertex(&v[8], l2, b2 - sy * 4.0f, 0.0f);
	SetVertex(&v[9], l2, sy * 18.0f + t2, 0.0f);
	DrawPolygon(v, 10, g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_BORDERCOLOR, 2.0f, 1);

	if (m_games.size() == 0)
		return;
	textX = sx * 4.0f + textX;
	textY = sy * 18.0f + textY;
	SGameEntry *game = m_games[m_nGame];
	if (game == NULL)
		return;
	float width = (right - textX) - 8.0f;
	DWORD flags = 0x10;
	m_pFont->DrawText(textX, textY, g_colorScheme.CAPTION_TEXTCOLOR, game->name, flags, width);
	float step = sy * 2.0f;
	textY = LineHeight(m_pFont) + step + textY;
	CGameSaveItem *save = game->saves.size() != 0 ? game->saves[m_nSave] : NULL;
	const wchar_t *last;
	if (!m_bInSaves || save == NULL)
	{
		// The game: counts, size, blocks, folder.
		m_pFont->DrawText(textX, textY, g_colorScheme.DIALOG_TEXTCOLOR, game->countText, flags, width);
		textY = LineHeight(m_pFont) + step + textY;
		m_pFont->DrawText(textX, textY, g_colorScheme.DIALOG_TEXTCOLOR, game->sizeText, flags, width);
		textY = LineHeight(m_pFont) + step + textY;
		m_pFont->DrawText(textX, textY, g_colorScheme.DIALOG_TEXTCOLOR, game->blocksText, flags, width);
		last = game->folderName;
	}
	else
	{
		// The save: name, size, blocks, folder.
		m_pFont->DrawText(textX, textY, g_colorScheme.DIALOG_TEXTCOLOR, save->m_pszTitle, flags, width);
		textY = LineHeight(m_pFont) + step + textY;
		m_pFont->DrawText(textX, textY, g_colorScheme.DIALOG_TEXTCOLOR, save->m_szSize, flags, width);
		textY = LineHeight(m_pFont) + step + textY;
		m_pFont->DrawText(textX, textY, g_colorScheme.DIALOG_TEXTCOLOR, save->m_szBlocks, flags, width);
		last = save->m_szFolder;
	}
	textY = LineHeight(m_pFont) + step + textY;
	m_pFont->DrawText(textX, textY, g_colorScheme.DIALOG_TEXTCOLOR, last, flags, width);
	m_pFont->DrawText(textX, LineHeight(m_pFont) + step + textY, g_colorScheme.DIALOG_TEXTCOLOR,
	                  game->onMemoryUnit ? L"Mem Unit" : L"Hard disk", flags, width);
}

// 0x0011C8F4
HRESULT CGameManager::Render()
{
	if (m_bLoaded)
	{
		float sx = m_fScaleX;
		float sy = m_fScaleY;
		DimScreen(0.7f);
		g_pd3dDevice->SetTransform(D3DTS_WORLD, XDK_CAST(const D3DMATRIX *, m_world));
		float listX = m_fX + 80.0f;
		float y = m_fY;
		float bottom = y + m_fHeight;
		DrawHeader(m_fX, y);
		y = sy * 76.0f + y;
		float infoY = sy * 100.0f + y;
		m_pFont->SetScaleFactors(sx, sy);

		// The games from the selected one down: a name plate, the save
		// panel and the game's picture; the others fade.
		float fade = 1.0f;
		int index = m_nGame;
		for (std::vector<SGameEntry *>::iterator it = m_games.begin() + m_nGame; it != m_games.end();
		     ++it, index++)
		{
			SGameEntry *game = *it;
			if (index < m_nGame || game == NULL)
				continue;
			game->LoadImages();
			IDirect3DTexture8 *picture = game->image != NULL ? game->image : m_pDefaultIcon;
			if (index == m_nGame)
			{
				DrawTab(sx * 92.0f + listX, y, game->name, g_colorScheme.CAPTION_TEXTCOLOR,
				        g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BORDERCOLOR, 1.0f, 2);
				// (Quirk: here the panel's 92 isn't scaled.)
				DrawSavePanel(listX + 92.0f, y, index, g_colorScheme.DIALOG_INNER_BACKCOLOR,
				              g_colorScheme.DIALOG_INNER_BORDERCOLOR, 1.0f);
				DrawIcon(listX, y, sx * 92.0f, sy * 92.0f, picture, 1.0f, !m_bInSaves);
			}
			else
			{
				DrawTab(sx * 92.0f + listX, y, game->name, g_colorScheme.DIALOG_TEXTCOLOR,
				        g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_BORDERCOLOR, fade * 0.5f, 1);
				DrawSavePanel(sx * 92.0f + listX, y, index, g_colorScheme.DIALOG_INNER_BACKCOLOR,
				              g_colorScheme.DIALOG_INNER_BORDERCOLOR, fade * 0.8f);
				DrawIcon(listX, y, sx * 92.0f, sy * 92.0f, picture, fade * 0.4f, FALSE);
				fade = fade - 0.1f;
			}
			y = LineHeight(m_pFont) + sy * 12.0f + y;
			if (game->saves.size() == 0)
			{
				y = sy * 80.0f + y;
			}
			else
			{
				// The saves, four a row; the selected game's grid scrolls
				// to keep its row in view (three rows at a time).
				int rows = SaveRows(game);
				float gridY = y;
				float gridX = sx * 100.0f + listX;
				int first = 0;
				int last = 11;
				if (index == m_nGame && m_nSaveRow > 2 && m_nSaveRow < m_nSaveRows)
				{
					first = m_nSaveRow * 4 - 8;
					last = m_nSaveRow * 4 + 3;
				}
				int column = 0;
				int saveIndex = 0;
				for (std::vector<CGameSaveItem *>::iterator s = game->saves.begin(); s != game->saves.end();
				     ++s, saveIndex++)
				{
					if (saveIndex < first)
						continue;
					if (saveIndex > last)
						break;
					CGameSaveItem *save = *s;
					if (save == NULL)
						continue;
					BOOL selected = m_bInSaves && m_nGame == index && m_nSave == saveIndex;
					float alpha;
					if (m_nGame != index)
						alpha = 0.4f;
					else if (m_nSave == saveIndex || m_bInSaves)
						alpha = 1.0f;
					else
						alpha = 0.8f;
					DrawIcon(gridX, gridY, sx * 64.0f, sy * 64.0f,
					         save->m_pImage != NULL ? save->m_pImage : m_pDefaultIcon, alpha, selected);
					column++;
					gridX = sx * 72.0f + gridX;
					if (column > 3)
					{
						gridX = sx * 100.0f + listX;
						column = 0;
						gridY = sy * 70.0f + gridY;
						if (gridY > bottom)
							break;
					}
				}
				y = (float)(rows * 70 + 10) * sy + y;
			}
			if (y > bottom)
				break;
		}
		DrawInfoPanel(m_fX, infoY);

		// "J Reload Game Saves" at the bottom. (Placed after the width of
		// "Download Game Saves", whose button was removed.)
		if (m_pSymbolFont != NULL)
		{
			m_pSymbolFont->SetScaleFactors(sx, sy);
			float hintX = sx * 20.0f + m_fX;
			float hintY = (m_fY + m_fHeight) - LineHeight(m_pSymbolFont) * 0.5f;
			float glyphWidth = m_pSymbolFont->GetTextWidth(L"A");
			hintX = m_pFont->GetTextWidth(L"Download Game Saves") + hintX + glyphWidth;
			m_pSymbolFont->DrawText(hintX, hintY, 0xFFFFFFFF, L"J", XBFONT_CENTER_Y, 0.0f);
			m_pFont->DrawText(glyphWidth + hintX, hintY, g_colorScheme.DIALOG_TEXTCOLOR, L"Reload Game Saves",
			                  XBFONT_CENTER_Y, 0.0f);
		}

		if (g_bFileOperationQueued && !g_bFileOperationPaused)
		{
			// A copy runs: a box with "Copying. Please Wait..." and a bar.
			float boxX = (m_fWidth * 0.5f + m_fX) - sx * 200.0f;
			float boxY = m_fHeight * 0.5f + m_fY;
			int dots = (int)((GetTickCount() / 500) % 3);
			DrawBevelBox(boxX, boxY, sx * 400.0f, sy * 80.0f, g_colorScheme.DIALOG_BORDERCOLOR,
			             g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
			boxY = boxY + 10.0f;
			const wchar_t *text = dots < 1 ? L"Copying. Please Wait..."
			                               : dots == 2 ? L"Copying. Please Wait.." : L"Copying. Please Wait.";
			m_pFont->DrawText(sx * 10.0f + boxX, boxY, g_colorScheme.MENU_ACTIVETEXTCOLOR, text, 0, 0.0f);
			boxY = LineHeight(m_pFont) + sy * 10.0f + boxY;
			// By bytes, else by files and folders done.
			unsigned percent = 0;
			if ((LONGLONG)g_ullOperationTotal > 0)
				percent = (unsigned)((double)(LONGLONG)(g_ullOperationDone * 100) / (double)(LONGLONG)g_ullOperationTotal);
			if (percent == 0)
			{
				unsigned items = g_nOperationFiles + g_nOperationFolders;
				if (items > 1)
					percent = g_nOperationProgress * 100 / items;
			}
			if (percent > 100)
				percent = 100;
			wchar_t label[6];
			swprintf(label, 6, L"%d", percent);
			wcscat(label, L"%");
			DrawProgressBar(sx * 10.0f + boxX, boxY, sx * 318.0f, LineHeight(m_pFont), (float)percent,
			                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 0xFF0000FF, 0xFF000040, 1);
			m_pFont->DrawText(sx * 390.0f + boxX, LineHeight(m_pFont) * 0.5f + boxY,
			                  g_colorScheme.OPTION_ACTIVETEXTCOLOR, label, XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);
		}
		else if (g_bOperationFailed)
		{
			if (!g_bCancelOperation)
			{
				std::string text;
				StrFormat(text, GetStringA(STR_MSG_GAMESAVES_COPY_ERROR), g_szOperationFile, GetLastError());
				ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			}
			g_bOperationFailed = FALSE;
			g_bFileOperationQueued = FALSE;
		}
		else if (m_bCopyMenuShown)
		{
			m_pCopyMenu->Render();
		}
		else if (m_bMenuShown)
		{
			m_menu.Render();
		}
	}
	if (m_bMessageShown)
		m_pMessageBox->Render();
	return S_OK;
}
