// CDropDown: the small frameless pop-up menu (an item's START menu, the file
// manager's sort menu, the FTP queue's menu). Xbox only (Direct3D).
#include <cmath>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "Menu.h"
#include "MenuDialogs.h"

// How far the menu unrolls per frame.
#define DROPDOWN_SLIDE_STEP 20.0f

// Any other input unlights the last move, at most once every 2s (for all
// drop-downs).
static DWORD s_dwLastMoveTimeout;   // 0x00278D58

// 0x0002DB5C
CDropDown::SItem::SItem(const std::wstring &text_, BOOL enabled_) : text(text_), enabled(enabled_)
{
}

// 0x000D8404
CDropDown::CDropDown()
{
	m_pSound = NULL;
	m_pFont = NULL;
	m_bCreated = FALSE;
	m_bShown = FALSE;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_nSelection = 0;
	m_nLastMove = 0;
	m_fMaxX = 0.0f;
	m_fMaxY = 0.0f;
	m_nFirst = 0;
	m_nLast = 0;
	m_nPageSize = 0;
	m_bCancelled = FALSE;
	m_bSlideIn = TRUE;
	m_bChosen = FALSE;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x000D8032
CDropDown::~CDropDown()
{
	m_pSound = NULL;
	m_pFont = NULL;
}

// 0x000D80D2
HRESULT CDropDown::Create(CXBFont *font, float x, float y)
{
	if (m_bCreated)
		return S_OK;
	m_pFont = font;
	if (font == NULL)
		g_fonts.Find("SmallFont", &m_pFont);
	if (m_pFont == NULL)
		return E_FAIL;
	GetScreenSize(&m_fMaxX, &m_fMaxY);
	m_fMaxX -= 10.0f;
	m_fMaxY -= 10.0f;
	m_fX = x;
	m_fY = y;
	m_fMarginX = 10.0f;
	m_fMarginY = 10.0f;
	m_pSound = g_soundEffects.Find("Scroll");
	return S_OK;
}

// 0x000D8169
HRESULT CDropDown::AddItem(const wchar_t *text)
{
	if (text == NULL || wcslen(text) == 0)
		return E_FAIL;
	m_items.push_back(SItem(std::wstring(text), TRUE));
	return S_OK;
}

// 0x00021F97
void CDropDown::SetEnabled(int index, BOOL enabled)
{
	if (index <= -1 || index >= (int)m_items.size())
		return;
	m_items[index].enabled = enabled;
	m_fHeight = 0.0f;
	m_fWidth = 0.0f;
	if (!enabled && index == m_nSelection)
		m_nSelection = 0;
}

// 0x000D7072
BOOL CDropDown::IsSelectionDisabled() const
{
	// (<= size: reads one past the end when the selection is size.)
	if (m_items.size() == 0 || (unsigned)m_nSelection > m_items.size())
		return FALSE;
	return !m_items[m_nSelection].enabled;
}

// 0x000D6F8A
HRESULT CDropDown::Show(float x, float y)
{
	m_bCancelled = FALSE;
	m_bChosen = FALSE;
	// At least 20 from the top left, and inside the screen (the size is
	// the last one measured).
	m_fY = y < 20.0f ? 20.0f : y;
	m_fX = x < 20.0f ? 20.0f : x;
	if (m_fX + m_fWidth > m_fMaxX)
		m_fX = m_fMaxX - m_fWidth;
	if (m_fY + m_fHeight > m_fMaxY)
		m_fY = m_fMaxY - m_fHeight - 10.0f;
	// (Scaled again on every Show.)
	ScaleToScreen(NULL, &m_fMarginX, NULL, &m_fMarginY, TRUE);
	if (!m_bShown)
	{
		m_bShown = TRUE;
		m_nFirst = 0;
		// The rows from the requested y to the screen's bottom.
		m_nPageSize = (int)((m_fMaxY - y) / (m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight + 2.0f));
		m_nLast = m_nPageSize - 1;
		// (Clamped to the count, not the last index.)
		if (m_nLast > m_items.size())
			m_nLast = (UINT)m_items.size();
		m_bSlideIn = TRUE;
	}
	return S_OK;
}

// 0x000D7539
HRESULT CDropDown::ProcessInput(int *action, SGamepadState **pad)
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
		// A click on a shown item chooses it.
		if (!g_bPointerClicked || !PointInRect(m_listRect, g_nPointerX, g_nPointerY))
		{
			*action = INPUT_NONE;
			return S_OK;
		}
		m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
		// (The offset is taken as unsigned.)
		unsigned offset = (unsigned)(g_nPointerY - m_listRect[1]);
		UINT row = (UINT)floor((double)offset / (m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight + 1.0f));
		UINT index = m_nFirst + row;
		if (index >= m_items.size())
		{
			*action = INPUT_NONE;
			return S_OK;
		}
		int previous = m_nSelection;
		m_nSelection = (int)index;
		if (IsSelectionDisabled())
		{
			m_nSelection = previous;
			*action = INPUT_NONE;
			return S_OK;
		}
		*action = INPUT_A;
	}

	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		if (m_items.size() != 0 && !IsSelectionDisabled())
		{
			m_bCancelled = FALSE;
			m_bShown = FALSE;
			m_bChosen = TRUE;
		}
		*action = INPUT_NONE;
		return S_OK;

	case INPUT_B:
	case INPUT_BACK:
		m_bCancelled = TRUE;
		m_bShown = FALSE;
		*action = INPUT_NONE;
		return S_OK;

	case INPUT_LTRIGGER:
	case INPUT_DOWN:
	{
		if (m_items.size() == 0)
		{
			*action = INPUT_NONE;
			return S_OK;
		}
		// The next shown item (giving up after a full turn).
		int tries = 0;
		do
		{
			m_nSelection = m_nSelection == (int)m_items.size() - 1 ? 0 : m_nSelection + 1;
			if ((int)m_items.size() < tries)
				break;
			tries++;
		} while (IsSelectionDisabled());
		PlaySound(m_pSound);
		m_nLastMove = 1;
		break;
	}

	case INPUT_RTRIGGER:
	case INPUT_UP:
	{
		if (m_items.size() == 0)
		{
			*action = INPUT_NONE;
			return S_OK;
		}
		int tries = 0;
		do
		{
			m_nSelection = (m_nSelection == 0 ? (int)m_items.size() : m_nSelection) - 1;
			if ((int)m_items.size() < tries)
				break;
			tries++;
		} while (IsSelectionDisabled());
		PlaySound(m_pSound);
		m_nLastMove = -1;
		break;
	}

	default:
		if (GetTickCount() > s_dwLastMoveTimeout)
		{
			m_nLastMove = 0;
			s_dwLastMoveTimeout = GetTickCount() + 2000;
		}
		*action = INPUT_NONE;
		return S_OK;
	}

	// Scroll the selection into the page.
	UINT selection = (UINT)m_nSelection;
	if (selection < m_nFirst)
	{
		m_nFirst = selection;
		m_nLast = m_nPageSize - 1 + selection;
		if (m_nLast > m_items.size() - 1)
		{
			m_nLast = (UINT)m_items.size() - 1;
			m_nFirst = m_nLast - m_nPageSize + 1;
		}
	}
	if (m_nLast < selection)
	{
		m_nLast = selection;
		m_nFirst = selection - m_nPageSize + 1;
	}
	*action = INPUT_NONE;
	return S_OK;
}

// 0x000D70A7
HRESULT CDropDown::Render()
{
	if (m_bCancelled || m_bChosen)
		return S_OK;
	if (m_items.size() == 0)
		return E_FAIL;

	m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;

	if (m_fHeight == 0.0f || m_fWidth == 0.0f)
	{
		// Measure: the widest item, and the shown items' height as far as
		// the screen allows. Quirk: a size that isn't zero is added to.
		if (m_items.size() == 0)
		{
			// (Never: an empty menu returned above.)
			if (m_pFont->GetTextWidth(L"W ") + 4.0f > m_fWidth)
				m_fWidth = m_pFont->GetTextWidth(L"W ");
		}
		else
		{
			UINT count = 1;
			for (std::vector<SItem>::iterator it = m_items.begin(); it != m_items.end(); ++it, count++)
			{
				float width = m_pFont->GetTextWidth(it->text.c_str()) + 4.0f;
				if (width > m_fWidth)
					m_fWidth = width;
				if (it->enabled && m_fHeight + 4.0f < m_fMaxY)
				{
					m_fHeight = lineHeight + 1.0f + m_fHeight;
					if (m_fY + m_fHeight < m_fMaxY)
						m_nPageSize = count;
				}
			}
			m_fWidth = m_fWidth == 0.0f ? m_pFont->GetTextWidth(L"W ") : m_fWidth + 16.0f;
			m_fHeight += 4.0f;
			m_fWidth += 4.0f;
			if (m_fX + m_fWidth > m_fMaxX)
				m_fX = m_fMaxX - m_fWidth - 10.0f;
			if (m_fHeight + m_fY > m_fMaxY)
				m_fY = m_fMaxY - m_fHeight - 10.0f;
		}
	}

	// How much of the menu is still rolled up while it unrolls. Quirk: one
	// for all drop-downs, started at the first one's height.
	static float s_fSlideOffset = m_fHeight;   // 0x00279508

	if (m_bSlideIn)
	{
		DrawRect(m_fX, m_fY, m_fWidth, m_fHeight - s_fSlideOffset, g_colorScheme.DIALOG_BORDERCOLOR,
		         g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
		s_fSlideOffset -= DROPDOWN_SLIDE_STEP;
		if (s_fSlideOffset <= 0.0f)
		{
			s_fSlideOffset = m_fHeight;
			m_bSlideIn = FALSE;
		}
		// The clickable area, set only while unrolling. Quirk: its height
		// is the menu's bottom edge, so it reaches further down.
		m_listRect[3] = (int)(m_fHeight + m_fY);
		m_listRect[0] = (int)(m_fX + 2.0f);
		m_listRect[2] = (int)(m_fWidth - 4.0f);
		m_listRect[1] = (int)(m_fY + 2.0f);
	}
	else
	{
		DrawRect(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
		         1.0f, 1);
	}

	float x = m_fX + 2.0f;
	float y = m_fY + 2.0f;
	float rowWidth = m_fWidth - 4.0f;
	if (m_nFirst > m_items.size())
		m_nFirst = 0;
	float bottom = m_fHeight + m_fY - (m_bSlideIn ? s_fSlideOffset : 0.0f);
	UINT i = m_nFirst;
	for (std::vector<SItem>::iterator it = m_items.begin() + m_nFirst; it != m_items.end(); ++it, i++)
	{
		if (!it->enabled)
			continue;
		DWORD color;
		if (i == (UINT)m_nSelection && !IsSelectionDisabled())
		{
			DrawRect(x - 1.0f, y - 1.0f, rowWidth + 1.0f, lineHeight + 1.0f, 0, g_colorScheme.OPTION_ACTIVEBACKCOLOR,
			         1.0f, 1);
			color = g_colorScheme.OPTION_ACTIVETEXTCOLOR;
		}
		else
		{
			color = g_colorScheme.DIALOG_TEXTCOLOR;
		}
		m_pFont->DrawText(x, y, color, it->text.c_str(), XBFONT_TRUNCATED, rowWidth);
		y += lineHeight + 1.0f;
		if (y > bottom)
		{
			if (m_nPageSize == 0)
				m_nPageSize = i - m_nFirst + 1;
			m_nLast = i;
			break;
		}
	}
	return S_OK;
}
