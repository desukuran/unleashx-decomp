// CMenu's drawing (0x0010D055-0x0010FDB8): the list (Render), the row
// (RenderHorizontal, <Menu Horizontal>) and the skin's item positions
// (RenderItems, <Menu><Item Left Top ...>). Each item line is an icon
// and its text; the sink may move a line or draw it itself (DrawItem).
// See docs/MENU.md.
#include "Menu.h"

#include <cmath>
#include <cstring>

#include "../app/MenuItemEditor.h"
#include "../app/Screens.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "MenuDialogs.h"
#include "MenuItem.h"
#include "VirtualKeyboard.h"

// Pulsing scroll arrows: the alpha and its direction, one pair per layout.
static float s_fArrowAlpha;            // 0x00278CF4
static float s_fArrowStep = 1.0f;      // 0x0026C6F8
static float s_fRowArrowAlpha;         // 0x00278CF0
static float s_fRowArrowStep = 1.0f;   // 0x0026C6EC
// Which item's icon m_pIconTexture holds, per layout.
static int s_nIconOf = -1;             // 0x0026C6FC
static int s_nRowIconOf = -1;          // 0x0026C6F4
static int s_nRowWrapIconOf = -1;      // 0x0026C6F0
static int s_nPosIconOf = -1;          // 0x0026C6E8
static int s_nPosWrapIconOf = -1;      // 0x0026C6E4

// The two halves of a fading text: the first and last line dim while the
// list scrolls smoothly.
static DWORD HalfAlpha(DWORD color)
{
	return color - ((color >> 1) & 0x7F000000);
}

// The clip rectangle of the menu area, cut at the screen's edge.
static D3DRECT ClipToScreen(float x, float y, float width, float height)
{
	float screenWidth = 640.0f;
	float screenHeight = 480.0f;
	GetScreenSize(&screenWidth, &screenHeight);
	D3DRECT rect;
	rect.x1 = (long)(x >= 0.0f ? x : 0.0f);
	rect.y1 = (long)(y >= 0.0f ? y : 0.0f);
	float right = width + x + 1.0f;
	rect.x2 = (long)(right < screenWidth ? right : screenWidth);
	float bottom = height + y + 1.0f;
	rect.y2 = (long)(bottom < screenHeight ? bottom : screenHeight);
	return rect;
}

// The selected item's icon is kept (with a reference) for the skin's
// icon image. Quirk: the reference to the previous one isn't released.
static void KeepSelectedIcon(CMenu *menu, CMenuItem *item, int index, int &iconOf)
{
	if (index != menu->m_nSelected || item->m_pTexture == NULL)
		return;
	if (menu->m_pIconTexture == NULL)
		iconOf = -1;
	if (iconOf != menu->m_nSelected)
	{
		menu->m_pIconTexture = item->m_pTexture;
		item->m_pTexture->AddRef();
		iconOf = menu->m_nSelected;
	}
}

// An item's icon: its own, or (inside a sub-list) the folder icon.
static IDirect3DTexture8 *ItemIcon(CMenu *menu, CMenuItem *item)
{
	if (menu->m_nDepth != 0 && item->m_pTexture == NULL)
		return menu->m_pTexture70;
	return item->m_pTexture;
}

// 0x0010ED3D
HRESULT CMenu::Render()
{
	if (!m_bReady)
	{
		// Still loading: after a quarter second say so; until then remember
		// the font's own scale.
		if (GetTickCount() - m_dwListOpenTime > 250)
		{
			DrawBusyBox(m_pFont, L"Checking. Please Wait");
			return S_OK;
		}
		m_fFontScaleX = m_pFont->m_fScaleFactorX;
		m_fFontScaleY = m_pFont->m_fScaleFactorY;
		return S_OK;
	}
	if (m_bPreloadIcons)
	{
		// One visible icon a frame, nothing drawn.
		UINT index = m_nPreloadIndex;
		if (index <= (UINT)m_nVisibleItems && index < m_items.size())
		{
			if (m_items[index] != NULL)
				m_items[index]->LoadIcon();
			m_nPreloadIndex++;
			return S_OK;
		}
		m_nPreloadIndex = 0;
		m_bPreloadIcons = FALSE;
		return S_OK;
	}

	if (m_nSkinVisibleItems != 0 && m_pItemPositions != NULL)
		RenderItems();
	else if (m_bHorizontal)
		RenderHorizontal();
	else
	{
		float y = m_fAreaY;
		float x = m_fAreaX;
		float width = m_fAreaWidth;
		float height = m_fAreaHeight;
		float itemHeight = (float)(UINT)m_nItemHeight;
		float spacing = (float)(UINT)m_nItemSpacing;
		ScaleToScreen(&x, &width, &y, &height, FALSE);
		float scaleY = 1.0f;
		float lineStep = spacing * 2.0f + itemHeight;
		float textScaleX = m_fTextScale;
		float textScaleY = m_fTextScale;
		ScaleToScreen(NULL, &textScaleX, NULL, &textScaleY, TRUE);
		ScaleToScreen(NULL, NULL, NULL, &scaleY, FALSE);
		textScaleX = (float)floor(textScaleX * 100.0f) * 0.01f;
		textScaleY = (float)floor(textScaleY * 100.0f) * 0.01f;
		itemHeight = scaleY * itemHeight;
		spacing = scaleY * spacing;
		lineStep = lineStep * scaleY;

		if ((g_colorScheme.MENU_BORDERCOLOR != 0 && m_nBorderWidth != 0) || g_colorScheme.MENU_BACKCOLOR != 0)
			DrawBevelBox(x, y, width, height, g_colorScheme.MENU_BORDERCOLOR, g_colorScheme.MENU_BACKCOLOR, 1.0f,
			             m_nBorderWidth);

		if (m_nItemCount == 0)
		{
			SetFontCursor(m_pFont, width * 0.5f + x, y);
			DrawFontText(m_pFont, g_colorScheme.DIALOG_DISABLEDTEXT, L"( empty )", XBFONT_CENTER_X, m_fAreaWidth);
		}
		else
		{
			D3DRECT clip = ClipToScreen(x, y, width, height);
			g_pd3dDevice->SetScissors(1, FALSE, &clip);
			// m_fTextScale, scaled for the screen against 640x480 (never
			// below 1), by the smaller factor.
			float scale;
			float scaleX1 = textScaleX < 1.0f ? 1.0f : textScaleX;
			float scaleY1 = textScaleY < 1.0f ? 1.0f : textScaleY;
			scale = scaleY1 > scaleX1 ? scaleX1 : scaleY1;
			m_pFont->SetScaleFactors(m_fFontScaleX * scale, m_fFontScaleY * scale);

			float marginX = 2.0f;
			float marginY = 2.0f;
			ScaleToScreen(NULL, &marginX, NULL, &marginY, FALSE);
			float lineHeight = GetFontHeight(m_pFont);
			float rowY = marginY + y;
			float rowX = marginX + x;
			float iconWidth = 0.0f;
			float iconHeight = 0.0f;
			if (!m_bNoIcons)
			{
				// The icon is a square of the spacing plus 34.
				iconWidth = iconHeight = (float)(UINT)m_nItemSpacing + 34.0f;
				ScaleToScreen(NULL, &iconWidth, NULL, &iconHeight, FALSE);
				if (iconHeight > lineHeight)
					lineHeight = iconHeight;
			}
			else if (itemHeight > lineHeight)
			{
				itemHeight = lineHeight;
			}
			// The text's start: after the icon; centred in the rest; or at
			// the right edge.
			float textX;
			if (m_dwTextAlign & XBFONT_CENTER_X)
				textX = (width - iconWidth) * 0.5f + iconWidth - spacing;
			else if (m_dwTextAlign & XBFONT_RIGHT)
				textX = width - spacing;
			else
				textX = iconWidth;
			// Quirk: CENTER_Y is added to the menu's own flags here.
			m_dwTextAlign |= XBFONT_CENTER_Y;

			int first = m_nFirstVisible;
			int last = m_nLastVisible;
			BOOL sinkDrew = FALSE;   // (never cleared again: see below)
			if (m_bFixed)
			{
				// <Menu Fixed>: the selection stays on row m_nFixAt; blank
				// rows above it at the top of the list.
				int fixRow = m_nFixAt - 1 >= 0 ? m_nFixAt - 1 : 0;
				int blank = fixRow - m_nSelected < 0 ? 0 : fixRow - m_nSelected;
				first = m_nSelected - fixRow;
				if (first < 0)
					first = 0;
				last = first + (m_nVisibleItems - blank);
				if ((UINT)last >= (UINT)(m_nItemCount - 1))
					last = m_nItemCount - 1;
				if (blank > 0)
					rowY = (spacing * 2.0f + itemHeight) * (float)blank + rowY;
				if (last < fixRow)
				{
					last = fixRow;
					if (fixRow >= m_nLastVisible)
						last = m_nLastVisible;
				}
			}
			// Smooth scrolling: the lines slide a pixel less each 5 ms.
			if (m_nScrollDirection != 0)
			{
				if (GetTickCount() - m_dwScrollStart > 5)
				{
					m_fScrollSpeed -= 1.0f;
					m_dwScrollStart = GetTickCount();
				}
				if (m_fScrollSpeed <= 0.0f)
				{
					m_nScrollDirection = 0;
					m_fScrollSpeed = 0.0f;
				}
				else
				{
					rowY = (float)m_nScrollDirection * m_fScrollSpeed + rowY;
				}
			}
			// Keep the selection inside the visible range.
			while ((UINT)m_nSelected > (UINT)m_nFirstVisible &&
			       (UINT)(m_nSelected - m_nFirstVisible) > (UINT)m_nLastVisible)
				m_nFirstVisible++;
			for (int shown = last - first; shown < m_nSelected - m_nFirstVisible; shown++)
				last++;

			float bottom = (y + height) - lineHeight;
			if (m_nMaxVisibleItems != 0)
			{
				float limit = (float)(UINT)m_nMaxVisibleItems * lineStep;
				bottom = (limit > height ? height : limit) + y;
			}
			float listBottom = bottom;
			float twoSpacing = spacing * 2.0f;
			float textWidth = (width - twoSpacing) - iconWidth;

			int index = first;
			for (std::vector<CMenuItem *>::iterator it = m_items.begin() + first; it != m_items.end();
			     ++it, index++)
			{
				if (index >= first)
				{
					if (index > last && last != -1)
						break;
					CMenuItem *item = *it;
					if (item != NULL)
					{
						if (m_bNoIcons != 1 || index == m_nSelected)
							item->LoadIcon();
						DWORD color = item->IsLabel() ? g_colorScheme.CAPTION_TEXTCOLOR : g_colorScheme.MENU_TEXTCOLOR;
						if (m_nScrollDirection != 0 && m_nMaxVisibleItems != 1 && (index == first || index == last))
							color = HalfAlpha(g_colorScheme.MENU_TEXTCOLOR);

						if (index == m_nSelected)
						{
							// Where the context menu opens.
							m_fContextMenuX = textX + rowX;
							m_fContextMenuY = lineHeight * 0.5f + rowY;
							if (!m_pSink->DrawItem(index, &rowX, &rowY, item->m_pszText,
							                       g_colorScheme.MENU_ACTIVETEXTCOLOR, textWidth, TRUE, item, first,
							                       last))
							{
								sinkDrew = TRUE;
							}
							else
							{
								float barWidth = m_bShowText ? width - 2.0f : textX;
								if (m_bDirty)
								{
									item->m_rcHit[0] = (int)rowX;
									item->m_rcHit[1] = (int)rowY;
									item->m_rcHit[3] = (int)lineHeight;
									item->m_rcHit[2] = (int)barWidth;
								}
								// The highlight bar (it stays put while the
								// lines slide).
								if (g_colorScheme.MENU_ACTIVEBORDERCOLOR != 0 || g_colorScheme.MENU_ACTIVEBACKCOLOR != 0)
									DrawRect(rowX + 1.0f, rowY - (float)m_nScrollDirection * m_fScrollSpeed, barWidth,
									         lineHeight, g_colorScheme.MENU_ACTIVEBORDERCOLOR,
									         g_colorScheme.MENU_ACTIVEBACKCOLOR, 0.9f, 1);
								if (m_bShowText)
								{
									float textPixels = m_pFont->GetTextWidth(item->m_pszText);
									DWORD textColor = m_nScrollDirection == 0 ? g_colorScheme.MENU_ACTIVETEXTCOLOR : color;
									if (m_bScrollText && textPixels > textWidth && m_nScrollDirection == 0)
									{
										// <Menu Scroll>: the text runs right to
										// left, a pixel every <ScrollSpeed> ms.
										DWORD now = GetTickCount();
										BOOL step = m_dwMarqueeTick < now;
										if (step)
											m_dwMarqueeTick = m_pSettings->m_nScrollSpeed + now;
										D3DRECT line;
										line.x1 = (long)(iconWidth + rowX);
										float top = rowY - (float)m_nScrollDirection * m_fScrollSpeed;
										line.y1 = (long)(top > (float)clip.y1 ? top : (float)clip.y1);
										float right = textWidth + rowX;
										line.x2 = (long)(right > (float)clip.x2 ? (float)clip.x2 : right);
										float lineBottom = (float)line.y1 + lineHeight;
										line.y2 = (long)(lineBottom > (float)clip.y2 ? (float)clip.y2 : lineBottom);
										g_pd3dDevice->SetScissors(1, FALSE, &line);
										if (step)
										{
											if ((float)line.x1 > (textPixels + rowX) - m_fMarqueeOffset)
												m_fMarqueeOffset = iconWidth - width;   // gone: in again from the right
											else
												m_fMarqueeOffset = m_fMarqueeOffset + 1.0f;
										}
										SetFontCursor(m_pFont, rowX - m_fMarqueeOffset, m_fContextMenuY);
										DrawFontText(m_pFont, textColor, item->m_pszText, XBFONT_CENTER_Y, textWidth);
										g_pd3dDevice->SetScissors(1, FALSE, &clip);
									}
									else
									{
										SetFontCursor(m_pFont, m_fContextMenuX, m_fContextMenuY);
										DrawFontText(m_pFont, textColor, item->m_pszText, m_dwTextAlign, textWidth);
										m_fMarqueeOffset = 0.0f;
									}
								}
							}
						}
						else
						{
							if (!m_pSink->DrawItem(index, &rowX, &rowY, item->m_pszText, color, textWidth, FALSE, item,
							                       first, last))
							{
								sinkDrew = TRUE;
							}
							else if (m_bShowText)
							{
								SetFontCursor(m_pFont, textX + rowX, lineHeight * 0.5f + rowY);
								DrawFontText(m_pFont, color, item->m_pszText, m_dwTextAlign, textWidth);
							}
						}

						// Quirk: once the sink has drawn one item, no icon is
						// drawn for the rest of the frame.
						if (!m_bNoIcons && item->m_pszText[0] != 0 && !sinkDrew)
							DrawTexture(ItemIcon(this, item), rowX + spacing, rowY + spacing, iconWidth - 2.0f,
							            iconHeight - 2.0f, 1.0f, NULL, 0.0f, NULL);
						KeepSelectedIcon(this, item, index, s_nIconOf);

						rowY = rowY + lineStep;
						if (rowY >= bottom)
						{
							// Full: scroll the start so the last row fits.
							if (m_nLastVisible != index && (UINT)m_nLastVisible > (UINT)m_nFirstVisible)
							{
								while ((UINT)(m_nLastVisible - m_nFirstVisible) > (UINT)m_nVisibleItems)
									m_nFirstVisible++;
							}
							break;
						}
					}
				}
				if (index > last || (m_nMaxVisibleItems != 0 && index > m_nMaxVisibleItems + first))
				{
					bottom = rowY;
					last = index;
					break;
				}
			}

			// <Menu Rotate>: the list continues with the first items when
			// there's room left.
			if (m_bWrap && first > 0 && listBottom - itemHeight * 0.5f > rowY && index - 1 >= m_nLastVisible)
			{
				int wrapIndex = 0;
				for (std::vector<CMenuItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it, wrapIndex++)
				{
					if (wrapIndex >= 0 && wrapIndex < first && *it != NULL)
					{
						CMenuItem *item = *it;
						DWORD color = item->IsLabel() ? g_colorScheme.CAPTION_TEXTCOLOR : g_colorScheme.MENU_TEXTCOLOR;
						if (m_nScrollDirection != 0 && m_nMaxVisibleItems != 1 && wrapIndex == last)
							color = HalfAlpha(g_colorScheme.MENU_TEXTCOLOR);
						if (!m_pSink->DrawItem(wrapIndex, &rowX, &rowY, item->m_pszText, color, textWidth, FALSE, item,
						                       first, last))
						{
							sinkDrew = TRUE;
						}
						else if (m_bShowText)
						{
							SetFontCursor(m_pFont, textX + rowX, lineHeight * 0.5f + rowY);
							DrawFontText(m_pFont, color, item->m_pszText, m_dwTextAlign, textWidth);
						}
						if (!m_bNoIcons && item->m_pszText[0] != 0 && !sinkDrew)
							DrawTexture(ItemIcon(this, item), rowX + spacing, rowY + spacing, iconWidth - 2.0f,
							            iconHeight - 2.0f, 1.0f, NULL, 0.0f, NULL);
						rowY = twoSpacing + rowY + itemHeight;
						if (rowY >= bottom)
							break;
					}
					if (wrapIndex > last || (m_nMaxVisibleItems != 0 && wrapIndex > m_nMaxVisibleItems))
						break;
				}
			}
			g_pd3dDevice->SetScissors(0, FALSE, NULL);

			if (m_bShowArrows)
			{
				// Pulsing arrows above and below when there's more.
				rowY = rowY - (float)m_nScrollDirection * m_fScrollSpeed;
				float arrowWidth = 10.0f;
				float arrowHeight = 10.0f;
				ScaleToScreen(NULL, &arrowWidth, NULL, &arrowHeight, FALSE);
				float arrowX;
				if (m_dwTextAlign & XBFONT_CENTER_X)
					arrowX = (width - arrowWidth) * 0.5f;
				else if (m_dwTextAlign & XBFONT_RIGHT)
					arrowX = textX - iconWidth * 0.5f;
				else
					arrowX = iconWidth * 0.5f;
				arrowX = arrowX + x;
				s_fArrowAlpha = s_fArrowStep * 0.025f + s_fArrowAlpha;
				if (first > 0)
					DrawTriangle(arrowX, (y - arrowHeight) - 2.0f, arrowWidth, arrowHeight, 1,
					             g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR,
					             s_fArrowAlpha);
				if (last < m_nItemCount - 1)
				{
					float arrowY = (twoSpacing + itemHeight) * 0.5f + rowY + 2.0f;
					float limit = lineHeight * 0.5f + bottom + 2.0f;
					if (limit <= arrowY)
						arrowY = limit;
					DrawTriangle(arrowX, arrowY, arrowWidth, arrowHeight, 2, g_colorScheme.DIALOG_INNER_BORDERCOLOR,
					             g_colorScheme.DIALOG_INNER_BACKCOLOR, s_fArrowAlpha);
				}
				if (s_fArrowAlpha <= 0.0f)
					s_fArrowStep = 1.0f;
				else if (s_fArrowAlpha >= 1.0f)
					s_fArrowStep = -1.0f;
			}
		}
	}

	m_pFont->SetScaleFactors(m_fFontScaleX, m_fFontScaleY);
	// The open pop-up, if any.
	if (m_bMessageShown)
		m_pMessageBox->Render();
	else if (m_bWaitingForKeyboard)
		g_virtualKeyboard.Render();
	else if (m_bWaitingForPassCode)
		m_pPassCode->Render();
	else if (m_bRecentMenuShown)
		m_pRecentMenu->Render();
	else if (m_bContextMenuShown)
		m_pContextMenu->Render();
	else if (m_bItemEditorShown)
		m_pItemEditor->Render();
	m_bDirty = FALSE;
	return S_OK;
}

// The text scale for the screen: m_fTextScale against 640x480, rounded
// down to hundredths, at least 1, the smaller of the two.
static float ScreenTextScale(float textScale)
{
	float scaleX = textScale;
	float scaleY = textScale;
	ScaleToScreen(NULL, &scaleX, NULL, &scaleY, TRUE);
	scaleX = (float)floor(scaleX * 100.0f) * 0.01f;
	scaleY = (float)floor(scaleY * 100.0f) * 0.01f;
	float x1 = scaleX < 1.0f ? 1.0f : scaleX;
	float y1 = scaleY < 1.0f ? 1.0f : scaleY;
	return y1 > x1 ? x1 : y1;
}

// 0x0010E017
void CMenu::RenderHorizontal()
{
	float y = m_fAreaY;
	float x = m_fAreaX;
	float width = m_fAreaWidth;
	float height = m_fAreaHeight;
	ScaleToScreen(&x, &width, &y, &height, FALSE);
	float scale = ScreenTextScale(m_fTextScale);
	if ((g_colorScheme.MENU_BORDERCOLOR != 0 && m_nBorderWidth != 0) || g_colorScheme.MENU_BACKCOLOR != 0)
		DrawBevelBox(x, y, width, height, g_colorScheme.MENU_BORDERCOLOR, g_colorScheme.MENU_BACKCOLOR, 1.0f,
		             m_nBorderWidth);
	if (!m_bReady)
		return;
	if (m_nItemCount == 0)
	{
		// (Here at the unscaled area.)
		SetFontCursor(m_pFont, m_fAreaWidth * 0.5f + m_fAreaX, m_fAreaY);
		DrawFontText(m_pFont, g_colorScheme.DIALOG_DISABLEDTEXT, L"( empty )", XBFONT_CENTER_X, m_fAreaWidth);
		return;
	}

	D3DRECT clip = ClipToScreen(x, y, width, height);
	float right = width + x;
	g_pd3dDevice->SetScissors(1, FALSE, &clip);
	m_pFont->SetScaleFactors(m_fFontScaleX * scale, m_fFontScaleY * scale);
	float marginX = 2.0f;
	float marginY = 2.0f;
	ScaleToScreen(NULL, &marginX, NULL, &marginY, FALSE);
	float lineHeight = GetFontHeight(m_pFont);
	float itemY = marginY + y;
	float itemX = marginX + x;
	float spacing = (float)(UINT)m_nItemSpacing;
	float iconWidth = 0.0f;
	float iconHeight = 0.0f;
	if (!m_bNoIcons)
	{
		iconWidth = iconHeight = spacing + 34.0f;
		ScaleToScreen(NULL, &iconWidth, NULL, &iconHeight, FALSE);
		// (Compared with the unscaled size.)
		if (lineHeight < spacing + 34.0f)
			lineHeight = spacing + 34.0f;
	}
	// The text after the icon, or (right-aligned) at the area's end.
	float textX = (m_dwTextAlign & XBFONT_RIGHT) ? width - spacing : iconWidth;
	BOOL sinkDrew = FALSE;
	// (With icons the room check uses the icon's height.)
	float iconSpace = m_bNoIcons ? 0.0f : iconHeight;

	int first = m_nFirstVisible;
	int last = m_nLastVisible;
	if (m_bFixed)
	{
		int fixRow = m_nFixAt - 1;
		int blank = fixRow - m_nSelected < 0 ? 0 : fixRow - m_nSelected;
		first = m_nSelected - fixRow;
		if (first < 0)
			first = 0;
		last = (m_nVisibleItems - blank) + first;
		if ((UINT)last >= (UINT)(m_nItemCount - 1))
			last = m_nItemCount - 1;
		if (last < fixRow)
		{
			last = fixRow;
			if (fixRow >= m_nLastVisible)
				last = m_nLastVisible;
		}
	}
	if (m_nScrollDirection != 0)
	{
		if (GetTickCount() - m_dwScrollStart > 5)
		{
			m_fScrollSpeed -= 1.0f;
			m_dwScrollStart = GetTickCount();
		}
		if (m_nScrollDirection != 0 && m_fScrollSpeed <= 0.0f)
			m_nScrollDirection = 0;
		else
			itemX = (float)m_nScrollDirection * m_fScrollSpeed + itemX;
	}
	for (int shown = last - first; shown < m_nSelected - m_nFirstVisible; shown++)
		last++;

	float itemWidth = 0.0f;
	int index = first;
	for (std::vector<CMenuItem *>::iterator it = m_items.begin() + first; it != m_items.end(); ++it, index++)
	{
		if (index >= first)
		{
			if (index > last && last != -1)
				break;
			CMenuItem *item = *it;
			if (item != NULL)
			{
				if (m_bNoIcons != 1 || index == m_nSelected)
					item->LoadIcon();
				DWORD color = item->IsLabel() ? g_colorScheme.CAPTION_TEXTCOLOR : g_colorScheme.MENU_TEXTCOLOR;
				if (m_nScrollDirection != 0 && m_nMaxVisibleItems != 1 && (index == first || index == last))
					color = HalfAlpha(g_colorScheme.MENU_TEXTCOLOR);
				if (m_bShowText)
					itemWidth = m_pFont->GetTextWidth(item->m_pszText) + spacing;
				if (iconSpace + itemX >= right)
				{
					last = index;
					break;
				}
				if (index == m_nSelected)
				{
					m_fContextMenuX = textX + itemX;
					m_fContextMenuY = lineHeight * 0.5f + itemY;
					if (!m_pSink->DrawItem(index, &itemX, &itemY, item->m_pszText, g_colorScheme.MENU_ACTIVETEXTCOLOR,
					                       0.0f, TRUE, item, first, last))
					{
						sinkDrew = TRUE;
					}
					else
					{
						if (m_bDirty)
						{
							item->m_rcHit[0] = (int)itemX;
							item->m_rcHit[1] = (int)itemY;
							item->m_rcHit[3] = (int)lineHeight;
							item->m_rcHit[2] = (int)((itemWidth - 2.0f) + textX);
						}
						if (g_colorScheme.MENU_ACTIVEBORDERCOLOR != 0 || g_colorScheme.MENU_ACTIVEBACKCOLOR != 0)
							DrawRect(itemX + 1.0f, itemY, (itemWidth - 2.0f) + textX, lineHeight,
							         g_colorScheme.MENU_ACTIVEBORDERCOLOR, g_colorScheme.MENU_ACTIVEBACKCOLOR, 0.9f, 1);
						if (m_bShowText)
						{
							SetFontCursor(m_pFont, m_fContextMenuX, m_fContextMenuY);
							DrawFontText(m_pFont, m_nScrollDirection == 0 ? g_colorScheme.MENU_ACTIVETEXTCOLOR : color,
							             item->m_pszText, m_dwTextAlign, (right - itemX) - iconSpace);
						}
					}
				}
				else if (!m_pSink->DrawItem(index, &itemX, &itemY, item->m_pszText, color, 0.0f, FALSE, item, first,
				                            last))
				{
					sinkDrew = TRUE;
				}
				else if (m_bShowText)
				{
					SetFontCursor(m_pFont, textX + itemX, lineHeight * 0.5f + itemY);
					DrawFontText(m_pFont, color, item->m_pszText, m_dwTextAlign, (right - itemX) - iconSpace);
				}
				if (!m_bNoIcons && item->m_pszText[0] != 0)
				{
					if (!sinkDrew)
						DrawTexture(ItemIcon(this, item), spacing + itemX, spacing + itemY, iconWidth - 2.0f,
						            iconHeight - 2.0f, 1.0f, NULL, 0.0f, NULL);
					itemX = iconWidth + itemX;
				}
				KeepSelectedIcon(this, item, index, s_nRowIconOf);
				itemX = (float)(UINT)(m_nItemSpacing * 2) + itemWidth + itemX;
				if (itemX >= right)
					break;
			}
		}
		if (index > last || (m_nMaxVisibleItems != 0 && index > m_nMaxVisibleItems))
		{
			right = itemX;
			break;
		}
	}

	// <Menu Rotate>: the first items follow when there's room. (The test
	// uses the unscaled area.)
	if (m_bWrap && first > 0 && m_fAreaX + m_fAreaWidth > itemX && index - 1 >= m_nLastVisible)
	{
		int wrapIndex = 0;
		for (std::vector<CMenuItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it, wrapIndex++)
		{
			if (wrapIndex < 0 || wrapIndex >= first || *it == NULL)
				continue;
			CMenuItem *item = *it;
			DWORD color = item->IsLabel() ? g_colorScheme.CAPTION_TEXTCOLOR : g_colorScheme.MENU_TEXTCOLOR;
			if (m_nScrollDirection != 0 && m_nMaxVisibleItems != 1 && wrapIndex == last)
				color = HalfAlpha(g_colorScheme.MENU_TEXTCOLOR);
			if (m_bShowText)
				itemWidth = m_pFont->GetTextWidth(item->m_pszText) + spacing;
			if (iconSpace + itemX >= right)
				break;
			if (!m_pSink->DrawItem(wrapIndex, &itemX, &itemY, item->m_pszText, color, 0.0f, FALSE, item, first,
			                       last))
			{
				sinkDrew = TRUE;
			}
			else if (m_bShowText)
			{
				SetFontCursor(m_pFont, textX + itemX, lineHeight * 0.5f + itemY);
				// (The icon room here is a fixed 34 pixels.)
				DrawFontText(m_pFont, color, item->m_pszText, m_dwTextAlign,
				             (right - itemX) - (float)(m_bNoIcons ? 0 : 34));
			}
			if (!m_bNoIcons && item->m_pszText[0] != 0)
			{
				if (!sinkDrew)
					DrawTexture(ItemIcon(this, item), spacing + itemX, spacing + itemY, iconWidth - 2.0f,
					            iconHeight - 2.0f, 1.0f, NULL, 0.0f, NULL);
				itemX = iconWidth + itemX;
			}
			KeepSelectedIcon(this, item, wrapIndex, s_nRowWrapIconOf);
			itemX = (float)(UINT)(m_nItemSpacing * 2) + itemWidth + itemX;
			if (itemX >= right)
				break;
		}
	}
	g_pd3dDevice->SetScissors(0, FALSE, NULL);

	if (m_bShowArrows)
	{
		// Pulsing arrows left and right of the row.
		float arrowWidth = 10.0f;
		float arrowHeight = 10.0f;
		float arrowY = lineHeight * 0.5f + y;
		ScaleToScreen(NULL, &arrowWidth, NULL, &arrowHeight, FALSE);
		arrowY = arrowY - arrowHeight * 0.5f;
		s_fRowArrowAlpha = s_fRowArrowStep * 0.025f + s_fRowArrowAlpha;
		if (first > 0)
			DrawTriangle((x - arrowWidth) - spacing, arrowY, arrowWidth, arrowHeight, 3,
			             g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR,
			             s_fRowArrowAlpha);
		if (last < m_nItemCount - 1)
		{
			float areaWidth = m_fAreaWidth;
			ScaleToScreen(NULL, &areaWidth, NULL, NULL, FALSE);
			DrawTriangle(spacing + areaWidth + x, arrowY, arrowWidth, arrowHeight, 4,
			             g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR,
			             s_fRowArrowAlpha);
		}
		if (s_fRowArrowAlpha <= 0.0f)
			s_fRowArrowStep = 1.0f;
		else if (s_fRowArrowAlpha >= 1.0f)
			s_fRowArrowStep = -1.0f;
	}
	m_pFont->SetScaleFactors(m_fFontScaleX, m_fFontScaleY);
}

// During a smooth scroll an item glides from the previous position to
// its own (the first from (0, the area's top)): t runs down from 1.
// Quirk: the area's top is in screen pixels, the positions in skin ones.
static void GlidePosition(const SSkinItemPos *positions, UINT slot, float areaY, int direction, float t,
                          float *x, float *y)
{
	float fromX = slot == 0 ? 0.0f : (float)positions[slot - 1].x;
	float fromY = slot == 0 ? areaY : (float)positions[slot - 1].y;
	float toX = (float)positions[slot].x;
	float toY = (float)positions[slot].y;
	float f = direction == 1 ? t : 1.0f - t;
	*x = f * (toX - fromX) + fromX;
	*y = f * (toY - fromY) + fromY;
}

// 0x0010D055
void CMenu::RenderItems()
{
	if (!m_bReady)
		return;
	if (m_nItemCount == 0)
	{
		SetFontCursor(m_pFont, m_fAreaWidth * 0.5f + m_fAreaX, m_fAreaY);
		DrawFontText(m_pFont, g_colorScheme.DIALOG_DISABLEDTEXT, L"( empty )", XBFONT_CENTER_X, m_fAreaWidth);
		return;
	}
	float y = m_fAreaY;
	float x = m_fAreaX;
	float width = m_fAreaWidth;
	float height = m_fAreaHeight;
	ScaleToScreen(&x, &width, &y, &height, FALSE);
	// (Here m_fTextScale is scaled against the skin size, not 640x480.)
	float textScaleX = m_fTextScale;
	float textScaleY = m_fTextScale;
	ScaleToScreen(NULL, &textScaleX, NULL, &textScaleY, FALSE);
	textScaleX = (float)floor(textScaleX * 100.0f) * 0.01f;
	textScaleY = (float)floor(textScaleY * 100.0f) * 0.01f;
	float x1 = textScaleX < 1.0f ? 1.0f : textScaleX;
	float y1 = textScaleY < 1.0f ? 1.0f : textScaleY;
	float scale = y1 > x1 ? x1 : y1;
	m_pFont->SetScaleFactors(m_fFontScaleX * scale, m_fFontScaleY * scale);
	float lineHeight = GetFontHeight(m_pFont);
	float iconWidth = 0.0f;
	float iconHeight = 0.0f;
	if (!m_bNoIcons)
	{
		iconWidth = iconHeight = (float)(UINT)m_nItemSpacing + 34.0f;
		ScaleToScreen(NULL, &iconWidth, NULL, &iconHeight, FALSE);
		if (iconHeight > lineHeight)
			lineHeight = iconHeight;
	}
	BOOL sinkDrew = FALSE;

	while ((UINT)(m_nSelected - m_nFirstVisible) > (UINT)m_nLastVisible)
		m_nFirstVisible++;
	int first = m_nFirstVisible;
	int last = m_nLastVisible;
	if (last > first + m_nSkinVisibleItems)
		last = first + m_nSkinVisibleItems;
	if (m_bFixed)
	{
		int fixRow = m_nFixAt - 1;
		int blank = fixRow - m_nSelected < 0 ? 0 : fixRow - m_nSelected;
		first = m_nSelected - fixRow;
		if (first < 0)
			first = 0;
		last = first + (m_nVisibleItems - blank);
		if ((UINT)last >= (UINT)(m_nItemCount - 1))
			last = m_nItemCount - 1;
		if (last < fixRow)
		{
			last = fixRow;
			if (fixRow >= m_nLastVisible)
				last = m_nLastVisible;
		}
	}
	const SSkinItemPos *positions = m_pItemPositions;
	float itemX = (float)positions[0].x;
	float itemY = (float)positions[0].y;
	// (A line height for m_nMaxVisibleItems is worked out here and not used.)

	D3DRECT clip;
	if (m_nScrollDirection != 0)
	{
		clip = ClipToScreen(x, y, width, height);
		g_pd3dDevice->SetScissors(1, FALSE, &clip);
	}
	UINT slot = 0;
	// Scrolling up: one more item glides in from above.
	if (m_nScrollDirection == 1 && first > 0)
		first--;

	int index = first;
	for (std::vector<CMenuItem *>::iterator it = m_items.begin() + first; it != m_items.end(); ++it, index++)
	{
		if (index >= first)
		{
			if (index > last && last != -1)
				break;
			if (m_nScrollDirection != 0)
			{
				// Quirk: the glide slows by a step per item drawn, not by time.
				m_fScrollSpeed -= 1.0f;
				if (m_fScrollSpeed <= 0.0f)
				{
					m_nScrollDirection = 0;
					m_fScrollSpeed = 0.0f;
				}
				else
				{
					GlidePosition(positions, slot, y, m_nScrollDirection,
					              m_fScrollSpeed / (m_fScrollRemaining + 1.0f), &itemX, &itemY);
				}
			}
			CMenuItem *item = *it;
			if (item != NULL)
			{
				if (m_bNoIcons != 1 || index == m_nSelected)
					item->LoadIcon();
				const SSkinItemPos &pos = positions[slot];
				float itemWidth = pos.width > 0 ? (float)pos.width : 0.0f;
				float itemHeight = pos.height > 0 ? (float)pos.height : 0.0f;
				DWORD align = pos.align;
				DWORD color = item->IsLabel() ? g_colorScheme.CAPTION_TEXTCOLOR : g_colorScheme.MENU_TEXTCOLOR;
				if (m_nScrollDirection != 0 && m_nMaxVisibleItems != 1 && (index == first || index == last))
					color = HalfAlpha(g_colorScheme.MENU_TEXTCOLOR);
				ScaleToScreen(&itemX, &itemWidth, &itemY, &itemHeight, FALSE);
				if (itemWidth != 0.0f)
					itemWidth = itemWidth - iconWidth;   // the text's room

				if (index == m_nSelected)
				{
					m_fContextMenuX = iconWidth + itemX;
					m_fContextMenuY = lineHeight * 0.5f + itemY;
					if (!m_pSink->DrawItem(index, &itemX, &itemY, item->m_pszText, g_colorScheme.MENU_ACTIVETEXTCOLOR,
					                       itemWidth, TRUE, item, first, last))
					{
						sinkDrew = TRUE;
					}
					else
					{
						if (g_colorScheme.MENU_ACTIVEBORDERCOLOR != 0 || g_colorScheme.MENU_ACTIVEBACKCOLOR != 0)
						{
							if (itemWidth == 0.0f && m_bShowText)
								itemWidth = m_pFont->GetTextWidth(item->m_pszText) + 2.0f;
							// The bar: the position's box, aligned like the text.
							int rect[4] = {pos.x, pos.y, pos.width, pos.height};
							if (align & XBFONT_RIGHT)
								rect[0] -= rect[2];
							else if (align & XBFONT_CENTER_X)
								rect[0] += rect[2] / -2;
							if (rect[3] > 0 && (align & XBFONT_CENTER_Y))
							{
								rect[1] -= rect[3] / 2;
								m_fContextMenuY = (m_fContextMenuY - lineHeight * 0.5f) + (float)(rect[3] / 2);
							}
							ScaleRectToScreen(rect);
							if (rect[3] == 0)
								rect[3] = (int)lineHeight + 2;
							if (rect[2] == 0)
								rect[2] = (int)(itemWidth + iconWidth + 2.0f);
							DrawRect((float)rect[0], (float)rect[1], (float)rect[2] - 2.0f, (float)rect[3],
							         g_colorScheme.MENU_ACTIVEBORDERCOLOR, g_colorScheme.MENU_ACTIVEBACKCOLOR, 0.9f, 1);
						}
						if (m_bShowText)
						{
							float textPixels = (m_bScrollText && m_nScrollDirection == 0)
							                       ? m_pFont->GetTextWidth(item->m_pszText)
							                       : 0.0f;
							DWORD textColor = m_nScrollDirection == 0 ? g_colorScheme.MENU_ACTIVETEXTCOLOR : color;
							if (m_bScrollText && textPixels > itemWidth)
							{
								// <Menu Scroll>, clipped to the line on the display.
								DWORD now = GetTickCount();
								BOOL step = m_dwMarqueeTick < now;
								if (step)
									m_dwMarqueeTick = m_pSettings->m_nScrollSpeed + now;
								float displayWidth = 640.0f;
								float displayHeight = 480.0f;
								GetDisplaySize(&displayWidth, &displayHeight, FALSE);
								D3DRECT line;
								line.x1 = (long)(iconWidth + itemX);
								line.y1 = (long)itemY;
								float right = itemWidth + itemX;
								line.x2 = (long)(right > displayWidth ? displayWidth : right);
								float lineBottom = (float)line.y1 + lineHeight;
								line.y2 = (long)(lineBottom > displayHeight ? displayHeight : lineBottom);
								g_pd3dDevice->SetScissors(1, FALSE, &line);
								if (step)
								{
									if ((float)line.x1 > (textPixels + itemX) - m_fMarqueeOffset)
										m_fMarqueeOffset = iconWidth - width;
									else
										m_fMarqueeOffset = m_fMarqueeOffset + 1.0f;
								}
								SetFontCursor(m_pFont, itemX - m_fMarqueeOffset, m_fContextMenuY);
								DrawFontText(m_pFont, textColor, item->m_pszText, XBFONT_CENTER_Y, itemWidth);
								// Quirk: this also ends the scroll clipping.
								g_pd3dDevice->SetScissors(0, FALSE, NULL);
							}
							else
							{
								// (0x0010CD40, here a no-op: no text size given.)
								SetFontCursor(m_pFont, m_fContextMenuX, m_fContextMenuY);
								DrawFontText(m_pFont, textColor, item->m_pszText, align | XBFONT_CENTER_Y, itemWidth);
								m_fMarqueeOffset = 0.0f;
							}
						}
					}
				}
				else if (!m_pSink->DrawItem(index, &itemX, &itemY, item->m_pszText, color, itemWidth, FALSE, item,
				                            first, last))
				{
					sinkDrew = TRUE;
				}
				else if (m_bShowText)
				{
					float textX = iconWidth + itemX;
					float textY = itemY;
					// A position without a height, or not centred, puts the
					// text's middle on the line's.
					if (itemHeight == 0.0f || !(align & XBFONT_CENTER_Y))
					{
						textY = lineHeight * 0.5f + textY;
						align |= XBFONT_CENTER_Y;
					}
					SetFontCursor(m_pFont, textX, textY);
					DrawFontText(m_pFont, color, item->m_pszText, align, itemWidth);
				}

				if (!m_bNoIcons && item->m_pszText[0] != 0 && !sinkDrew)
				{
					// (Here a sub-list's items get the folder icon only
					// without one of their own, at any depth.)
					IDirect3DTexture8 *icon = item->m_pTexture != NULL ? item->m_pTexture : m_pTexture70;
					DrawTexture(icon, itemX + 1.0f, itemY + 1.0f, iconWidth - 2.0f, iconHeight - 2.0f, 1.0f, NULL, 0.0f,
					            NULL);
				}
				KeepSelectedIcon(this, item, index, s_nPosIconOf);
				if (m_bDirty)
				{
					// Quirk: an empty size is filled into the skin's
					// position itself.
					SSkinItemPos *editable = (SSkinItemPos *)&positions[slot];
					if (editable->height == 0)
						editable->height = (int)lineHeight;
					if (editable->width == 0)
						editable->width = (int)itemWidth;
					item->m_rcHit[0] = editable->x;
					item->m_rcHit[1] = editable->y;
					item->m_rcHit[2] = editable->width;
					item->m_rcHit[3] = editable->height;
				}
				// The next position. (Quirk: read even past the last one.)
				slot++;
				itemX = (float)positions[slot].x;
				itemY = (float)positions[slot].y;
			}
		}
		if (index > last || slot >= (UINT)m_nSkinVisibleItems || (m_nMaxVisibleItems != 0 && index > m_nMaxVisibleItems))
			break;
	}

	// <Menu Rotate>: the first items fill the remaining positions. Quirk:
	// at skin coordinates (unscaled), with 32x32 icons.
	if (m_bWrap && first > 0 && slot < (UINT)m_nSkinVisibleItems && index - 1 >= m_nLastVisible)
	{
		int wrapIndex = 0;
		for (std::vector<CMenuItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it, wrapIndex++)
		{
			if (wrapIndex >= 0 && wrapIndex < first)
			{
				if (m_nScrollDirection != 0 && m_fScrollSpeed > 0.0f)
					GlidePosition(positions, slot, y, m_nScrollDirection, m_fScrollSpeed / (m_fScrollRemaining + 1.0f),
					              &itemX, &itemY);
				CMenuItem *item = *it;
				if (item != NULL)
				{
					const SSkinItemPos &pos = positions[slot];
					float itemWidth = (float)pos.width - iconWidth;
					if (itemWidth < 0.0f)
						itemWidth = 0.0f;
					DWORD align = pos.align | XBFONT_CENTER_Y;
					DWORD color = item->IsLabel() ? g_colorScheme.CAPTION_TEXTCOLOR : g_colorScheme.MENU_TEXTCOLOR;
					if (m_nScrollDirection != 0 && m_nMaxVisibleItems != 1 && wrapIndex == last)
						color = HalfAlpha(g_colorScheme.MENU_TEXTCOLOR);
					if (!m_pSink->DrawItem(wrapIndex, &itemX, &itemY, item->m_pszText, color, itemWidth, FALSE, item,
					                       first, last))
					{
						sinkDrew = TRUE;
					}
					else if (m_bShowText)
					{
						SetFontCursor(m_pFont, iconWidth + itemX, lineHeight * 0.5f + itemY);
						if (pos.width != 0)
							align |= XBFONT_TRUNCATED;
						DrawFontText(m_pFont, color, item->m_pszText, align, itemWidth);
					}
					if (!m_bNoIcons && item->m_pszText[0] != 0 && !sinkDrew)
					{
						IDirect3DTexture8 *icon = item->m_pTexture != NULL ? item->m_pTexture : m_pTexture70;
						DrawTexture(icon, itemX + 1.0f, itemY + 1.0f, 32.0f, 32.0f, 1.0f, NULL, 0.0f, NULL);
					}
					KeepSelectedIcon(this, item, wrapIndex, s_nPosWrapIconOf);
					slot++;
					itemX = (float)positions[slot].x;
					itemY = (float)positions[slot].y;
				}
			}
			// (One more position than the first pass allows.)
			if (wrapIndex > last || slot > (UINT)m_nSkinVisibleItems ||
			    (m_nMaxVisibleItems != 0 && wrapIndex > m_nMaxVisibleItems))
				break;
		}
	}
	g_pd3dDevice->SetScissors(0, FALSE, NULL);
	m_pFont->SetScaleFactors(m_fFontScaleX, m_fFontScaleY);
}
