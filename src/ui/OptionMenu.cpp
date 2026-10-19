// COptionMenu: the pop-up option list in a dialog window (the recent list,
// the file managers' and the music player's menus). Xbox only (Direct3D).
#include <cmath>

#include "../audio/Sound.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "Menu.h"
#include "MenuDialogs.h"

// The default size limits of a window that fits its options (on 640x480).
#define OPTION_MENU_MIN_WIDTH 300.0f
#define OPTION_MENU_MIN_HEIGHT 260.0f
#define OPTION_MENU_MAX_WIDTH 560.0f
#define OPTION_MENU_MAX_HEIGHT 430.0f
// Between the rows (the selection bar is this much taller than the font).
#define OPTION_ROW_GAP 3.0f
// Options counted for the height of a window that fits them.
#define OPTION_MENU_MAX_ROWS 19

// 0x00030F11
COptionMenu::SOption::SOption(const std::wstring &text_, BOOL enabled_, int value_)
	: text(text_), enabled(enabled_), value(value_)
{
	unknown30 = 0;
}

// 0x000AEADE
COptionMenu::COptionMenu(BOOL wrap)
{
	m_bWrap = wrap;
	m_fBorder = 10.0f;
	m_nPageSize = 0;
	m_nFirst = 0;
	m_nLast = 0;
	m_unknown5C = 1;
	m_pFont = NULL;
	m_pTitleFont = NULL;
	m_fLineSpacing = 1.0f;
	m_bCancelled = FALSE;
	m_bChosen = FALSE;
	m_nOptionCount = 0;
	m_nSelection = 0;
	m_bSlideIn = TRUE;
	m_dwSlideTime = 0;
	m_pSettings = CAppSettings::GetInstance();
	// (The position, size, line height and list rectangle are left to
	// Create.)
}

// 0x000ADE12
COptionMenu::~COptionMenu()
{
	ClearOptions();
	// (The vector's destructor clears it again.)
}

// 0x000AD930
BOOL COptionMenu::IsOpen() const
{
	return !m_bCancelled && !m_bChosen;
}

// 0x000AD946
int COptionMenu::GetSelection() const
{
	if (m_bCancelled || m_options.size() == 0)
		return -1;
	return m_nSelection;
}

// 0x000AD960
int COptionMenu::GetSelectedValue() const
{
	if (m_bCancelled || m_options.size() == 0 || (unsigned)m_nSelection >= m_options.size())
		return -1;
	return m_options[m_nSelection].value;
}

// 0x000AD997
void COptionMenu::SetOptionEnabled(int index, BOOL enabled)
{
	if (index > -1 && index < (int)m_options.size())
		m_options[index].enabled = enabled;
}

// 0x000ADE61
BOOL COptionMenu::IsSelectionDisabled() const
{
	// (The option is copied to be tested. Not range-checked.)
	SOption option = m_options[m_nSelection];
	return !option.enabled || option.text.size() == 0;
}

// 0x000AE9AD
HRESULT COptionMenu::Create(CXBFont *font, const wchar_t *title, float x, float y, float width, float height,
                            int maxVisible)
{
	if (font == NULL)
		return E_FAIL;
	if (title != NULL)
		m_strTitle = title;
	if (m_pFont == NULL)
	{
		m_pFont = font;
		m_fLineHeight = font->m_fScaleFactorY * font->m_fFontHeight;
		m_pTitleFont = m_pSettings->m_pDialogFont;
		if (m_pTitleFont == NULL)
			m_pTitleFont = font;
		m_fY = y;
		m_fX = x;
		m_fWidth = width;
		m_fHeight = height;
		ClearOptions();
		if (maxVisible == 0)
			maxVisible = 10;
		m_options.reserve(maxVisible);
	}
	// The whole window until Render places the list.
	m_listRect[3] = (int)m_fHeight;
	m_listRect[0] = (int)m_fX;
	m_listRect[2] = (int)m_fWidth;
	m_listRect[1] = (int)m_fY;
	return S_OK;
}

// 0x000AEA79
HRESULT COptionMenu::AddOption(const wchar_t *text, int value)
{
	std::wstring s(text);
	m_options.push_back(SOption(s, s.size() != 0, value));
	m_nOptionCount++;
	return S_OK;
}

// 0x000ADE9E
void COptionMenu::Show(BOOL clear)
{
	if (clear)
	{
		ClearOptions();
		m_nOptionCount = 0;
		m_nPageSize = 0;
		m_nFirst = 0;
		m_nSelection = 0;
	}
	m_bSlideIn = TRUE;
	m_bCancelled = FALSE;
	m_bChosen = FALSE;
	if (m_options.size() != 0 && IsSelectionDisabled())
	{
		// The first enabled option (by the flag only), if any.
		int index = 0;
		for (std::vector<SOption>::iterator it = m_options.begin(); it != m_options.end(); ++it, index++)
		{
			if (it->enabled)
			{
				m_nSelection = index;
				break;
			}
		}
		if ((unsigned)m_nSelection >= m_options.size() && m_options.size() != 0)
			m_nSelection = (int)m_options.size() - 1;
	}
}

// 0x000AE6F1
void COptionMenu::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_LTHUMB && pad != NULL && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}

	if (*action == INPUT_POINTER_CLICK)
	{
		// A click on an enabled option chooses it.
		if (!g_bPointerClicked || !PointInRect(m_listRect, g_nPointerX, g_nPointerY))
		{
			*action = INPUT_NONE;
			return;
		}
		// (The offset is taken as unsigned.)
		unsigned offset = (unsigned)(g_nPointerY - m_listRect[1]);
		int row = (int)floor((double)offset / (m_fLineHeight + OPTION_ROW_GAP));
		int index = m_nFirst + row;
		if ((unsigned)index >= m_options.size())
		{
			*action = INPUT_NONE;
			return;
		}
		int previous = m_nSelection;
		m_nSelection = index;
		if (IsSelectionDisabled())
		{
			m_nSelection = previous;
			*action = INPUT_NONE;
			return;
		}
		*action = INPUT_A;
	}

	switch (*action)
	{
	case INPUT_A:
		if (m_options.size() != 0 && !IsSelectionDisabled())
			m_bChosen = TRUE;
		*action = INPUT_NONE;
		return;

	case INPUT_B:
	case INPUT_BACK:
		m_bCancelled = TRUE;
		*action = INPUT_NONE;
		return;

	case INPUT_LTRIGGER:
	case INPUT_UP:
	{
		if (m_options.size() == 0 || m_options.size() < 2)
		{
			*action = INPUT_NONE;
			return;
		}
		// The previous enabled option (giving up after a full turn). Quirk:
		// the ends are taken from m_nOptionCount, not the list's size.
		int tries = 0;
		do
		{
			if (m_nSelection == 0)
				m_nSelection = m_bWrap ? m_nOptionCount - 1 : 0;
			else
				m_nSelection--;
			if ((int)m_options.size() < tries)
				break;
			tries++;
		} while (IsSelectionDisabled());
		// Then off a disabled option the other way (where the search gave up).
		while ((unsigned)m_nSelection < (unsigned)(m_nOptionCount - 1) && IsSelectionDisabled())
			m_nSelection++;
		break;
	}

	case INPUT_RTRIGGER:
	case INPUT_DOWN:
	{
		if (m_options.size() == 0 || m_options.size() < 2)
		{
			*action = INPUT_NONE;
			return;
		}
		int tries = 0;
		do
		{
			if ((unsigned)m_nSelection == (unsigned)(m_nOptionCount - 1))
				m_nSelection = m_bWrap ? 0 : m_nSelection;
			else
				m_nSelection++;
			if ((int)m_options.size() < tries)
				break;
			tries++;
		} while (IsSelectionDisabled());
		while (m_nSelection != 0 && IsSelectionDisabled())
			m_nSelection--;
		break;
	}

	default:
		*action = INPUT_NONE;
		return;
	}

	// Scroll the selection into the page.
	int selection = m_nSelection;
	if (selection < m_nFirst)
	{
		m_nFirst = selection;
		m_nLast = m_nPageSize - 1 + selection;
		if ((int)m_options.size() - 1 < m_nLast)
		{
			m_nLast = (int)m_options.size() - 1;
			m_nFirst = m_nLast - m_nPageSize + 1;
		}
	}
	if (m_nLast < selection)
	{
		m_nLast = selection;
		m_nFirst = selection - m_nPageSize + 1;
		if (m_nFirst < 0)
		{
			m_nFirst = 0;
			m_nLast = m_nPageSize - 1;
		}
	}
	*action = INPUT_NONE;
}

// 0x000ADF2C
void COptionMenu::Render()
{
	if (m_bCancelled || m_bChosen)
		return;
	if (m_options.size() == 0)
	{
		m_bCancelled = TRUE;
		return;
	}

	m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_pTitleFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;

	if (m_fX == 0.0f || m_fY == 0.0f || m_fWidth == 0.0f || m_fHeight == 0.0f)
	{
		// Fit and center the window (on 640x480, then scaled).
		float height = m_fLineHeight + 10.0f;
		float width = m_fWidth == 0.0f ? m_pFont->GetTextWidth(m_strTitle.c_str()) + 75.0f : m_fWidth;
		if (m_fWidth == 0.0f || m_fHeight == 0.0f)
		{
			unsigned count = 0;
			for (std::vector<SOption>::iterator it = m_options.begin(); it != m_options.end(); ++it, count++)
			{
				if (count < OPTION_MENU_MAX_ROWS)
					height = m_fLineSpacing + height + m_fLineHeight;
				float textWidth = m_pFont->GetTextWidth(it->text.c_str());
				if (textWidth > width)
					width = textWidth;
			}
			width = m_fBorder * 2.0f + width + 6.0f;
			height = m_fLineSpacing + height + m_fLineHeight + m_fLineHeight * 2.0f + m_fBorder * 3.0f + 6.0f;
			if (width < OPTION_MENU_MIN_WIDTH)
				width = OPTION_MENU_MIN_WIDTH;
			if (height < OPTION_MENU_MIN_HEIGHT)
				height = OPTION_MENU_MIN_HEIGHT;
		}
		if (width > OPTION_MENU_MAX_WIDTH)
			width = OPTION_MENU_MAX_WIDTH;
		if (height > OPTION_MENU_MAX_HEIGHT)
			height = OPTION_MENU_MAX_HEIGHT;
		if (m_fWidth == 0.0f)
			m_fWidth = width;
		if (m_fHeight == 0.0f)
			m_fHeight = height;
		if (m_fY == 0.0f)
			m_fY = (480.0f - m_fHeight) * 0.5f;
		if (m_fX == 0.0f)
			m_fX = (640.0f - m_fWidth) * 0.5f;
		ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);

		// The list: below the caption, above the bottom bar.
		m_listRect[3] = (int)(m_fHeight - m_fLineHeight * 2.0f - m_fBorder - OPTION_ROW_GAP);
		m_listRect[0] = (int)(m_fX + m_fBorder + OPTION_ROW_GAP);
		m_listRect[2] = (int)(m_fWidth - (m_fBorder + OPTION_ROW_GAP) * 2.0f);
		m_listRect[1] = (int)(m_fBorder * 2.0f + m_fY + m_fLineHeight + OPTION_ROW_GAP);
	}

	if (m_nPageSize == 0)
	{
		m_nPageSize = (int)((m_fHeight - (m_fBorder * 3.0f + m_fLineHeight * 2.0f)) / (m_fLineHeight + OPTION_ROW_GAP));
		m_nFirst = 0;
		m_nLast = m_nPageSize - 1;
	}

	// How far the window still is above its place while it slides in.
	// Quirk: one for all menus, started at the first menu's bottom edge.
	static float s_fSlideOffset = m_fHeight + m_fY;   // 0x0027A38C

	float offset = 0.0f;
	if (!m_bSlideIn)
	{
		DrawWindowFrame(m_fX, m_fY, m_fWidth, m_fHeight, m_fBorder, m_fLineHeight, m_fLineHeight,
		                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
		                g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 1);
	}
	else
	{
		// Down by 1/24 of the screen's height every 5ms; this frame is drawn
		// at the offset before the step.
		offset = s_fSlideOffset;
		DrawWindowFrame(m_fX, m_fY - offset, m_fWidth, m_fHeight, m_fBorder, m_fLineHeight, m_fLineHeight,
		                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
		                g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 1);
		if (GetTickCount() - m_dwSlideTime > 5)
		{
			float screenHeight = 480.0f;
			GetDisplaySize(NULL, &screenHeight, FALSE);
			s_fSlideOffset -= screenHeight * (1.0f / 24.0f);
			m_dwSlideTime = GetTickCount();
		}
		if (s_fSlideOffset <= 0.0f)
		{
			s_fSlideOffset = m_fHeight + m_fY;
			m_bSlideIn = FALSE;
		}
	}

	// The caption: a bar over 60% of the width, the title (in the dialog
	// font) cut at 75%.
	DrawGradientRect(m_fX + m_fBorder, m_fBorder * 0.5f + m_fY - offset, m_fWidth * 0.6f - m_fBorder * 2.0f,
	                 m_fLineHeight + m_fBorder, g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BACKCOLOR, FALSE,
	                 0);
	float titleHeight = m_pTitleFont->m_fScaleFactorY * m_pTitleFont->m_fFontHeight;
	m_pTitleFont->DrawText(m_fX + m_fBorder + 3.0f, (titleHeight + m_fBorder) * 0.5f + m_fY - offset,
	                       g_colorScheme.CAPTION_TEXTCOLOR, m_strTitle.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                       m_fWidth * 0.75f - m_fBorder * 2.0f - 3.0f);

	// The page of options.
	float x = m_fX + m_fBorder + 3.0f;
	float y = m_fBorder * 2.0f + m_fY + m_fLineHeight + 3.0f - offset;
	float rowWidth = m_fWidth - (m_fBorder + 3.0f) * 2.0f;
	float bottom = m_fHeight + m_fY - m_fLineHeight * 2.0f - m_fBorder - 3.0f;
	int i = m_nFirst;
	for (std::vector<SOption>::iterator it = m_options.begin() + m_nFirst; it != m_options.end(); ++it, i++)
	{
		DWORD color;
		if (i == m_nSelection && !IsSelectionDisabled())
		{
			DrawGradientRect(x - 1.0f, y - 1.0f, rowWidth, m_fLineHeight + 3.0f, g_colorScheme.OPTION_ACTIVEBACKCOLOR,
			                 g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE, g_colorScheme.OPTION_ACTIVEBACKCOLOR);
			color = g_colorScheme.OPTION_ACTIVETEXTCOLOR;
		}
		else if (!it->enabled)
		{
			color = g_colorScheme.DIALOG_DISABLEDTEXT;
		}
		else
		{
			color = g_colorScheme.DIALOG_TEXTCOLOR;
		}
		m_pFont->DrawText(x, y, color, it->text.c_str(), XBFONT_TRUNCATED, rowWidth);
		y += m_fLineHeight + 3.0f;
		if (y > bottom)
		{
			// The page is full: now its size is known. Keep the selection
			// on it.
			if (m_nPageSize == 0)
				m_nPageSize = i - m_nFirst + 1;
			m_nLast = i;
			while ((unsigned)m_nSelection > (unsigned)m_nLast && (unsigned)m_nFirst < (unsigned)m_nSelection)
			{
				m_nFirst++;
				m_nLast++;
			}
			return;
		}
		if (i > m_nLast)
			return;
	}
}
