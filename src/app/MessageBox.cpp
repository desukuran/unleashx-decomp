// CMessageBox: the dashboard's message box. The text is word-wrapped to the
// box (which widens for long words), the box fits itself to the text, is
// centered and slides down into place.
#include <cmath>
#include <cstring>
#include <cwchar>

#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../ui/Menu.h"
#include "Screens.h"

// 0x00030702
SMessageLine::SMessageLine(const std::wstring &text_, float height_, int unknown)
	: text(text_), height(height_), unknown20(unknown)
{
	rect[2] = 0;
	rect[3] = 0;
	// (rect[0] and rect[1] are left as they are.)
}

// 0x000B10D4
CMessageBox::CMessageBox()
{
	m_buttons.reserve(3);
	m_fButtonWidth = 0.0f;
	m_fPadding = 10.0f;
	m_fLineSpacing = 1.0f;
	m_nVisibleLines = 0;
	m_nFirstLine = 0;
	m_nLastLine = 0;
	m_unknown11C = TRUE;
	m_pFont = NULL;
	m_pTitleFont = NULL;
	m_bCancelled = FALSE;
	m_bChosen = FALSE;
	m_unknown128 = 0;
	m_nSelected = 0;
	m_bSliding = TRUE;
	m_dwSlideTime = GetTickCount();
	m_pSettings = CAppSettings::GetInstance();
}

// 0x000B07F9
CMessageBox::~CMessageBox()
{
	m_buttons.clear();
	m_lines.clear();
	m_strTitle.clear();
}

// 0x000B084D
HRESULT CMessageBox::Create(CXBFont *font, const wchar_t *title, float x, float y, float width, float height,
                            int maxLines)
{
	if (font == NULL)
		return E_FAIL;
	if (title != NULL)
		m_strTitle = title;
	m_pFont = font;
	m_fLineHeight = font->m_fFontHeight * font->m_fScaleFactorY;
	m_pTitleFont = m_pSettings->m_pDialogFont;
	if (m_pTitleFont == NULL)
		m_pTitleFont = font;
	m_fY = y;
	m_fX = x;
	m_fWidth = width;
	m_fHeight = height;
	if (width == 0.0f)
		m_fWidth = 360.0f;
	m_fPadding = 10.0f;
	ScaleToScreen(NULL, &m_fPadding, NULL, NULL, TRUE);
	m_lines.clear();
	m_buttons.clear();
	if (maxLines == 0)
		maxLines = 10;
	m_lines.reserve(maxLines);
	m_buttons.reserve(3);
	for (int i = MSGBOX_OK; i <= MSGBOX_NO; i++)
		SetButtonText(i, NULL);
	return S_OK;
}

// 0x000AFB3D
void CMessageBox::SetButtonText(int button, const wchar_t *text)
{
	if (button > MSGBOX_NO)
		return;
	wchar_t name[20];
	memset(name, 0, sizeof(name));
	if (text == NULL || wcslen(text) == 0)
	{
		static const wchar_t *const defaults[7] = {L"OK", L"Cancel", L"Abort", L"Retry", L"Ignore", L"Yes", L"No"};
		if (button >= 0)
			wcscpy(name, defaults[button]);
		text = name;
	}
	size_t length = wcslen(text);
	if (length != 0)
	{
		m_buttonText[button].clear();
		m_buttonText[button].assign(text, length);
	}
}

// 0x000B07DE
void CMessageBox::SetTitle(const wchar_t *title)
{
	m_strTitle.clear();
	m_strTitle = title;
}

// 0x000AFBFD
void CMessageBox::Reset(BOOL clear)
{
	m_bSliding = TRUE;
	m_dwSlideTime = GetTickCount();
	if (clear)
	{
		m_lines.clear();
		m_buttons.clear();
		m_unknown128 = 0;
		m_fHeight = 0.0f;
		m_fY = 0.0f;
		m_fX = 0.0f;
	}
	m_bCancelled = FALSE;
	m_bChosen = FALSE;
}

// 0x000B0F05
void CMessageBox::AddButton(const wchar_t *text)
{
	size_t length = wcslen(text);
	if (length == 0)
		return;
	float width;
	float height;
	m_pFont->GetTextExtentN(text, (int)length, FALSE, &width, &height);
	m_buttons.push_back(SMessageLine(text, height, 0));
	m_fButtonWidth = 0.0f;
}

// 0x000B0E87
void CMessageBox::AddLine(const wchar_t *text)
{
	std::wstring line = text;
	float width;
	float height;
	m_pFont->GetTextExtentN(text, (int)wcslen(text), FALSE, &width, &height);
	m_lines.push_back(SMessageLine(line, height, 0));
}

// 0x000B076A
void CMessageBox::SetLineText(int line, const wchar_t *text)
{
	if (line == -1)
		line = (int)m_lines.size() - 1;
	if (line < 0 || line >= (int)m_lines.size())
		return;
	SMessageLine &entry = m_lines[line];
	entry.text = text;
	float width;
	float height;
	m_pFont->GetTextExtentN(text, (int)wcslen(text), FALSE, &width, &height);
	entry.height = height;
}

// 0x000B09A5
void CMessageBox::SetText(const char *text)
{
	m_lines.clear();
	if (text == NULL)
		return;
	m_lines.reserve(10);

	std::string rest = text;
	StrReplaceAll(rest, "  ", " ");
	rest = StrTrimRight(StrTrimLeft(rest));
	rest.append(" ");
	m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_fWidth = 360.0f;
	ScaleToScreen(NULL, &m_fWidth, NULL, NULL, TRUE);

	wchar_t line[128];
	float width;
	float height;
	int lineEnd = 0;   // the space the current line ends at, once a word didn't fit
	float available = m_fWidth - (m_fPadding + 3.0f) * 2.0f;
	int space = StrFind(rest, " ", 0);
	if (space != -1 && space > 0)
	{
		do
		{
			if (rest.size() == 0)
				goto done;
			// A newline ends the line early (if no earlier word overflowed).
			int newline = StrFindChar(rest, '\n');
			if (newline > 0)
				space = newline;
			std::string head = StrLeft(rest, space);
			memset(line, 0, sizeof(line));
			StrUtf8ToWide(head.c_str(), line, 0x7E);
			if (m_pFont->GetTextWidth(line) > available || newline > 0)
			{
				// The line is up to the last space that fit.
				if (lineEnd == 0)
					lineEnd = space;
				memset(line, 0, sizeof(line));
				StrUtf8ToWide(StrLeft(rest, lineEnd).c_str(), line, 0x7E);
				m_pFont->GetTextExtentN(line, (int)wcslen(line), FALSE, &width, &height);
				if (width > available)
				{
					// One word is too long. Up to 500 pixels it's cut
					// (at a space if there's one in its second half,
					// else to whatever fits); then the box widens.
					if (500.0f >= width)
					{
						int cut = StrReverseFind(rest, " ", space);
						if (cut == 0 || (unsigned)cut < wcslen(line) >> 1)
						{
							do
							{
								line[wcslen(line) - 1] = 0;
								cut = (int)wcslen(line);
								m_pFont->GetTextExtentN(line, cut, FALSE, &width, &height);
							} while (width > available);
						}
						lineEnd = cut;
						StrUtf8ToWide(StrLeft(rest, cut).c_str(), line, 0x7E);
					}
					available = width + 5.0f;
					m_fWidth = ((m_fPadding + 3.0f) * 2.0f + available + 9.0f) * 0.1f * 10.0f;
				}
				m_lines.push_back(SMessageLine(line, height, 0));
				rest = StrMid(rest, lineEnd + 1, (int)rest.size() - (lineEnd + 1));
				lineEnd = 0;
			}
			else
			{
				lineEnd = space;
			}
			space = StrFind(rest, " ", lineEnd + 1);
		} while (space > 0);
	}
	if ((int)rest.size() > 0)
	{
		memset(line, 0, sizeof(line));
		StrUtf8ToWide(rest.c_str(), line, 0x7E);
		m_pFont->GetTextExtentN(line, (int)wcslen(line), FALSE, &width, &height);
		m_lines.push_back(SMessageLine(line, height, 0));
	}
done:
	// A blank line before the buttons.
	m_lines.push_back(SMessageLine(L"", m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY, 0));
}

// 0x000B0F74
void CMessageBox::Show(const char *text, const wchar_t *title, int buttons)
{
	SetTitle(title);
	Reset(TRUE);
	SetText(text);
	m_nButtons = buttons;
	switch (buttons)
	{
	case MSGBOX_BUTTONS_OK:
		m_nSelected = 0;
		AddButton(m_buttonText[MSGBOX_OK].c_str());
		break;
	case MSGBOX_BUTTONS_OKCANCEL:
		AddButton(m_buttonText[MSGBOX_OK].c_str());
		AddButton(m_buttonText[MSGBOX_CANCEL].c_str());
		break;
	case MSGBOX_BUTTONS_ABORTRETRYIGNORE:
		AddButton(m_buttonText[MSGBOX_ABORT].c_str());
		AddButton(m_buttonText[MSGBOX_RETRY].c_str());
		AddButton(m_buttonText[MSGBOX_IGNORE].c_str());
		break;
	case MSGBOX_BUTTONS_YESNOCANCEL:
		AddButton(m_buttonText[MSGBOX_YES].c_str());
		AddButton(m_buttonText[MSGBOX_NO].c_str());
		AddButton(m_buttonText[MSGBOX_CANCEL].c_str());
		break;
	case MSGBOX_BUTTONS_YESNO:
		AddButton(m_buttonText[MSGBOX_YES].c_str());
		AddButton(m_buttonText[MSGBOX_NO].c_str());
		break;
	case MSGBOX_BUTTONS_RETRYCANCEL:
		AddButton(m_buttonText[MSGBOX_RETRY].c_str());
		AddButton(m_buttonText[MSGBOX_CANCEL].c_str());
		break;
	case MSGBOX_BUTTONS_CANCEL:
		AddButton(m_buttonText[MSGBOX_CANCEL].c_str());
		break;
	}
	// (Only the OK box resets the selection.)
}

// 0x000AF8CF
void CMessageBox::SetSelection(int button)
{
	if (button > -1 && button < (int)m_buttons.size())
		m_nSelected = button;
}

// 0x000AF8E9
BOOL CMessageBox::IsOpen() const
{
	return !m_bCancelled && !m_bChosen;
}

// 0x000AFA59
int CMessageBox::GetResult()
{
	if (IsOpen() || m_buttons.size() == 0)
		return MSGBOX_NONE;
	if (m_bCancelled)
		return m_nButtons != MSGBOX_BUTTONS_OK ? MSGBOX_CANCEL : MSGBOX_OK;

	switch (m_nButtons)
	{
	case MSGBOX_BUTTONS_OKCANCEL:
		return m_nSelected != 0 ? MSGBOX_CANCEL : MSGBOX_OK;
	case MSGBOX_BUTTONS_ABORTRETRYIGNORE:
		if (m_nSelected == 0)
			return MSGBOX_ABORT;
		if (m_nSelected == 1)
			return MSGBOX_RETRY;
		return m_nSelected == 2 ? MSGBOX_IGNORE : MSGBOX_NONE;
	case MSGBOX_BUTTONS_YESNOCANCEL:
		if (m_nSelected == 0)
			return MSGBOX_YES;
		if (m_nSelected == 1)
			return MSGBOX_NO;
		return m_nSelected == 2 ? MSGBOX_CANCEL : MSGBOX_NONE;
	case MSGBOX_BUTTONS_YESNO:
		if (m_nSelected == 0)
			return MSGBOX_YES;
		return m_nSelected == 1 ? MSGBOX_NO : MSGBOX_NONE;
	case MSGBOX_BUTTONS_RETRYCANCEL:
		if (m_nSelected == 0)
			return MSGBOX_RETRY;
		return m_nSelected == 1 ? MSGBOX_CANCEL : MSGBOX_NONE;
	case MSGBOX_BUTTONS_CANCEL:
		return MSGBOX_CANCEL;
	default:
		return MSGBOX_OK;
	}
}

// 0x000AF925
HRESULT CMessageBox::ProcessInput(int *action)
{
	for (;;)
	{
		switch (*action)
		{
		case INPUT_NONE:
		case INPUT_POINTER_CLICK:
		{
			// The pointer selects the button under it; a click chooses it.
			if (!g_bPointerClicked && !g_bPointerMoved)
				break;
			size_t i;
			for (i = 0; i < m_buttons.size(); i++)
			{
				if (PointInRect(m_buttons[i].rect, g_nPointerX, g_nPointerY))
					break;
			}
			if (i == m_buttons.size())
				break;
			m_nSelected = (int)i;
			if (!g_bPointerClicked)
				break;
			*action = INPUT_A;
			continue;
		}

		case INPUT_A:
			if (m_buttons.size() != 0)
				m_bChosen = TRUE;
			break;

		case INPUT_B:
		case INPUT_BACK:
			*action = INPUT_NONE;
			m_bCancelled = TRUE;
			break;

		case INPUT_LEFT:
			if (m_buttons.size() != 0 && m_nSelected > 0)
				m_nSelected--;
			break;

		case INPUT_RIGHT:
			if (m_buttons.size() != 0 && (unsigned)m_nSelected < m_buttons.size() - 1)
				m_nSelected++;
			break;
		}
		break;
	}
	// It takes all input.
	*action = INPUT_NONE;
	return S_OK;
}

// 0x000AFC64
HRESULT CMessageBox::Render()
{
	if (m_bCancelled || m_bChosen)
		return S_OK;
	if (m_buttons.size() == 0)
		return E_FAIL;

	DimScreen(0.3f);
	m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_pTitleFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_fLineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;

	// The size (to fit the text) and the position (centered), where not set.
	if (m_fX == 0.0f || m_fY == 0.0f || m_fWidth == 0.0f || m_fHeight == 0.0f)
	{
		float width = m_fWidth;
		float height = 0.0f;
		if (m_fWidth == 0.0f || m_fHeight == 0.0f)
		{
			for (size_t i = 0; i < m_lines.size(); i++)
			{
				SMessageLine line = m_lines[i];
				// (Only the first 16 lines count.)
				if (i <= 15)
					height = m_fLineSpacing + (line.height > m_fLineHeight ? line.height : m_fLineHeight) + height;
				float lineWidth = m_pFont->GetTextWidth(line.text.c_str());
				if (lineWidth > width)
					width = lineWidth;
			}
			width = m_fPadding * 2.0f + width + 6.0f;
			height = (m_fLineSpacing + m_fLineHeight) * 2.0f + height + m_fLineHeight * 2.0f + m_fPadding * 3.0f + 6.0f;
			float minWidth = 360.0f;
			float minHeight = 260.0f;
			ScaleToScreen(NULL, &minWidth, NULL, &minHeight, TRUE);
			if (minWidth > width)
				width = 360.0f > minWidth ? 360.0f : minWidth;
			if (minHeight > height)
				height = 260.0f > minHeight ? 260.0f : minHeight;
		}
		D3DDISPLAYMODE mode;
		g_pd3dDevice->GetDisplayMode(&mode);
		float maxWidth = (float)(mode.Width - 80);
		float maxHeight = (float)(mode.Height - 60);
		if (width > maxWidth)
			width = maxWidth;
		if (height > maxHeight)
			height = maxHeight;
		if (m_fWidth == 0.0f)
			m_fWidth = width;
		if (m_fHeight == 0.0f)
			m_fHeight = height;
		if (m_fY == 0.0f)
			m_fY = ((float)mode.Height - m_fHeight) * 0.5f;
		if (m_fX == 0.0f)
			m_fX = ((float)mode.Width - m_fWidth) * 0.5f;
	}
	if (m_nVisibleLines == 0)
	{
		m_nFirstLine = 0;
		m_nVisibleLines = (int)((m_fHeight - (m_fPadding * 3.0f + m_fLineHeight * 2.0f)) / (m_fLineHeight + 3.0f));
		m_nLastLine = m_nVisibleLines - 1;
	}
	if (m_fButtonWidth == 0.0f)
	{
		for (size_t i = 0; i < m_buttons.size(); i++)
		{
			SMessageLine button = m_buttons[i];
			float width = m_pFont->GetTextWidth(button.text.c_str());
			if (width > m_fButtonWidth)
				m_fButtonWidth = width;
		}
		// At least 86 (or 86 scaled, if that's more).
		m_fButtonWidth = m_fButtonWidth + 6.0f;
		if (86.0f > m_fButtonWidth)
		{
			m_fButtonWidth = 86.0f;
			ScaleToScreen(NULL, &m_fButtonWidth, NULL, NULL, TRUE);
			if (86.0f > m_fButtonWidth)
				m_fButtonWidth = 86.0f;
		}
	}

	// The window, sliding down from above the screen 20 pixels (per 480
	// lines) every few milliseconds.
	static float s_fSlide = m_fHeight + m_fY;   // 0x0027A374
	float slide;
	if (m_bSliding)
	{
		slide = s_fSlide;
		DrawWindowFrame(m_fX, m_fY - slide, m_fWidth, m_fHeight, m_fPadding, m_fLineHeight, m_fLineHeight * 2.0f,
		                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
		                g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 1);
		if (GetTickCount() - m_dwSlideTime > 5)
		{
			float displayHeight = 480.0f;
			GetDisplaySize(NULL, &displayHeight, FALSE);
			s_fSlide = s_fSlide - (float)ceil(displayHeight * (1.0f / 480.0f)) * 20.0f;
			m_dwSlideTime = GetTickCount();
		}
		if (0.0f >= s_fSlide)
		{
			s_fSlide = m_fHeight + m_fY;
			m_bSliding = FALSE;
		}
	}
	else
	{
		slide = 0.0f;
		DrawWindowFrame(m_fX, m_fY, m_fWidth, m_fHeight, m_fPadding, m_fLineHeight, m_fLineHeight * 2.0f,
		                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
		                g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 1);
	}

	// The caption.
	DrawGradientRect(m_fX + m_fPadding, m_fPadding * 0.5f + m_fY - slide, m_fWidth * 0.6f - m_fPadding * 2.0f,
	                 m_fLineHeight + m_fPadding, g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	                 FALSE, 0);
	m_pTitleFont->DrawText(m_fX + m_fPadding + 3.0f,
	                       (m_pTitleFont->m_fFontHeight * m_pTitleFont->m_fScaleFactorY + m_fPadding) * 0.5f + m_fY -
	                           slide,
	                       g_colorScheme.CAPTION_TEXTCOLOR, m_strTitle.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                       m_fWidth * 0.75f - m_fPadding * 2.0f - 3.0f);

	// The text.
	float x = m_fX + m_fPadding + 3.0f;
	float y = m_fPadding * 2.0f + m_fY + m_fLineHeight + 3.0f - slide;
	float textWidth = m_fWidth - (m_fPadding + 3.0f) * 2.0f;
	for (int i = 0; i < (int)m_lines.size(); i++)
	{
		if (i >= m_nFirstLine)
		{
			if (i > m_nLastLine)
				break;
			SMessageLine line = m_lines[i];
			m_pFont->DrawText(x, y, g_colorScheme.DIALOG_TEXTCOLOR, line.text.c_str(), XBFONT_TRUNCATED, textWidth);
			y = (line.height > m_fLineHeight ? line.height : m_fLineHeight) + y + 3.0f;
		}
		if (i > m_nLastLine)
			break;
	}

	// The buttons, centered in a row along the bottom.
	y = m_fHeight + m_fY - m_fPadding - 3.0f - m_fLineHeight - slide;
	float rowWidth = (float)m_buttons.size() * m_fButtonWidth + (float)(((int)m_buttons.size() - 1) * 10);
	x = (textWidth - rowWidth) * 0.5f + m_fX + m_fPadding;
	for (int i = 0; i < (int)m_buttons.size(); i++)
	{
		SMessageLine button = m_buttons[i];
		float boxX = x - 3.0f;
		float boxY = y - 6.0f;
		DWORD color;
		if (i == m_nSelected)
		{
			DrawButtonBox(boxX, boxY, m_fButtonWidth + 6.0f, m_fLineHeight + 12.0f, g_colorScheme.BUTTON_BORDERCOLOR,
			              g_colorScheme.BUTTON_ACTIVEBACKCOLOR, 2);
			color = g_colorScheme.BUTTON_ACTIVETEXTCOLOR;
		}
		else
		{
			DrawButtonBox(boxX, boxY, m_fButtonWidth + 6.0f, m_fLineHeight + 12.0f, g_colorScheme.BUTTON_BORDERCOLOR,
			              g_colorScheme.BUTTON_BACKCOLOR, 1);
			color = g_colorScheme.DIALOG_TEXTCOLOR;
		}
		m_pFont->DrawText(m_fButtonWidth * 0.5f + x, y, color, button.text.c_str(),
		                  XBFONT_CENTER_X | XBFONT_TRUNCATED, textWidth);
		// Where the pointer can click it (where it ends up, without the
		// slide); set once.
		if (button.rect[2] == 0)
		{
			m_buttons[i].rect[0] = (int)boxX;
			m_buttons[i].rect[1] = (int)(y + slide - 6.0f);
			m_buttons[i].rect[2] = (int)(m_fButtonWidth + 6.0f);
			m_buttons[i].rect[3] = (int)(m_fLineHeight + 12.0f);
		}
		x = m_fButtonWidth + x + 13.0f;
	}
	return S_OK;
}
