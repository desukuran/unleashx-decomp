// CFTPLog: the FTP log and its window. See FtpLog.h.
#include "FtpLog.h"

#include <cstring>
#include <cwchar>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"
#include "FtpManager.h"
#include "../app/Screens.h"

CFTPLog g_ftpLog;   // 0x00277720

// The frame's margin. 0x0026C610
static float s_fLogMargin = 3.0f;
// The lines' width, worked out on the first Render(). 0x0027A37C
static float s_fLogLineWidth;
static BOOL s_bLogLineWidth = FALSE;   // 0x0027A380 bit 0

// The log menu's entries.
enum
{
	LOGMENU_CLOSE = 0,
	LOGMENU_CLEAR = 2,
	LOGMENU_SAVE = 3,
};

// 0x0001E705 (this in esi)
SFtpLogLine::SFtpLogLine(const wchar_t *lineText, int lineType)
	: text(lineText, wcslen(lineText)), type(lineType)
{
}

// 0x001235D0 (this in edi)
CFTPLog::CFTPLog()
{
	m_fY = 5.0f;
	m_fX = 5.0f;
	m_nScroll = 0;
	m_pFont = NULL;
	m_bOpen = FALSE;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_bMenuOpen = FALSE;
	m_bKeyboardOpen = FALSE;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	// (m_pCaptionFont and the counters aren't set: the one object is a
	// global, so they start at 0.)
}

// 0x00120DBE
CFTPLog::~CFTPLog()
{
	Clear();
	delete m_pMessageBox;
	m_pMessageBox = NULL;
}

// 0x0001E747 (this in edi)
void CFTPLog::Clear()
{
	m_lines.clear();
	m_nLastLine = 0;
	m_nTop = 0;
	m_nVisible = 0;
	m_nScroll = 0;
}

// 0x0001E769 (this in eax)
void CFTPLog::ResetRect()
{
	m_fY = 5.0f;
	m_fX = 5.0f;
	m_fWidth = 630.0f;
	m_fHeight = 470.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
}

// 0x001215FF (this in esi, font in ebx)
HRESULT CFTPLog::Init(CXBFont *font)
{
	HRESULT hr = E_FAIL;
	if (font != NULL)
	{
		m_pFont = font;
		if (m_pCaptionFont == NULL)
		{
			m_pCaptionFont = g_pFtpSettings->m_pDialogFont;
			if (m_pCaptionFont == NULL)
				m_pCaptionFont = font;
			hr = S_OK;
			ResetRect();
			m_menu.Create(font, L"Select", 0.0f, 0.0f, 0.0f, 0.0f, 10);
			m_menu.AddOption(L"Close", 0);
			m_menu.AddOption(L" ", 1);
			m_menu.AddOption(L"Clear", 2);
			m_menu.AddOption(L"Save...", 3);
			m_menu.SetOptionEnabled(1, FALSE);
		}
	}
	// (Opens even when it failed.)
	m_bOpen = TRUE;
	return hr;
}

// 0x001216B6 (the vtable slot points at a thunk, 0x0001E736)
void CFTPLog::Add(const char *text, int type)
{
	if (text == NULL)
		return;
	int length = (int)strlen(text);
	if (length == 0)
		return;
	std::string line;
	std::string all = text;
	StrRemoveChar(all, '\r');
	if (StrRight(all, 1).compare("\n") != 0)
		all.append(1, '\n');
	int start = 0;
	int end = StrFindChar(all, '\n');
	while (end != -1)
	{
		line = StrMid(all, start, end - start);
		if (line.size() != 0)
		{
			unsigned size = (unsigned)line.size();
			wchar_t *wide = new wchar_t[size + 2];
			if (wide == NULL)
				break;
			// Quirk: this converts the start of the whole text, not the
			// line, so each line of a multi-line message shows the
			// message's beginning (cut to the line's length).
			StrUtf8ToWide(text, wide, size + 1);
			SFtpLogLine entry(wide, type);
			m_lines.push_back(entry);
			delete[] wide;
			if (m_lines.size() > 1000)
			{
				// (The oldest line goes, and the rest of the text with it.)
				m_lines.erase(m_lines.begin());
				break;
			}
		}
		start = end + 1;
		std::string::size_type next = all.find('\n', start);
		end = next == std::string::npos ? -1 : (int)next;
	}
	m_nLastLine = (int)m_lines.size() - 1;
}

// 0x001215AC (this in edi, out in ebx)
void CFTPLog::GetText(std::string *out)
{
	if (out == NULL)
		return;
	out->clear();
	for (std::vector<SFtpLogLine>::iterator it = m_lines.begin(); it != m_lines.end(); ++it)
	{
		if (it->text.size() == 0)
		{
			out->append("\r\n");
			continue;
		}
		out->append(StrFromWide(it->text.c_str()));
		if (it != m_lines.end() - 1)
			out->append("\r\n");
	}
}

// 0x0011F299
void CFTPLog::ScrollUp()
{
	if (m_nScroll <= 0)
		return;
	m_nScroll--;
	while ((unsigned)m_nScroll < (unsigned)m_nTop)
		m_nTop--;
}

// 0x0011F261 (this in eax). Quirk: it stops 12 lines before the end.
void CFTPLog::ScrollDown()
{
	if ((unsigned)(m_nScroll + 12) >= (unsigned)m_nLastLine)
		return;
	m_nScroll++;
	while ((unsigned)(m_nTop + m_nVisible) < (unsigned)m_nScroll)
		m_nTop++;
}

// 0x00123199 (this in edi)
void CFTPLog::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
		}
		else
		{
			m_pMessageBox->SetButtonText(MSGBOX_OK, GetString(STR_OK));
			m_pMessageBox->SetButtonText(MSGBOX_CANCEL, GetString(STR_CANCEL));
			m_pMessageBox->SetButtonText(MSGBOX_ABORT, GetString(STR_ABORT));
			m_pMessageBox->SetButtonText(MSGBOX_RETRY, GetString(STR_RETRY));
			m_pMessageBox->SetButtonText(MSGBOX_IGNORE, GetString(STR_IGNORE));
			m_pMessageBox->SetButtonText(MSGBOX_YES, GetString(STR_YES));
			m_pMessageBox->SetButtonText(MSGBOX_NO, GetString(STR_NO));
		}
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageShown = TRUE;
	}
}

// 0x00120CAC
void CFTPLog::ShowSaveKeyboard(const wchar_t *title, const char *path)
{
	g_virtualKeyboard.Open();
	m_bKeyboardOpen = TRUE;
	g_virtualKeyboard.SetTitle(title != NULL ? title : L"Enter Filename");
	g_virtualKeyboard.m_nMaxLength = 0x105;
	g_virtualKeyboard.Open();
	g_virtualKeyboard.m_bPassword = FALSE;
	std::string name;
	if (path != NULL && strlen(path) != 0)
	{
		name = path;
	}
	else
	{
		StrFormat(name, "C:\\Log%04d.txt", 1);
		for (unsigned n = 2; FileExists(name.c_str()) && n <= 9998; n++)
			StrFormat(name, "C:\\Log%04d.txt", n);
	}
	if (name.size() != 0)
	{
		wchar_t text[0x104];
		StrUtf8ToWide(name.c_str(), text, 0x104);
		g_virtualKeyboard.SetText(text);
	}
}

// 0x001232DD
HRESULT CFTPLog::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_NONE)
		return S_OK;
	if (m_bMenuOpen)
	{
		m_menu.ProcessInput(action, pad);
		m_bMenuOpen = m_menu.IsOpen();
		if (m_bMenuOpen || !m_menu.m_bChosen)
			return S_OK;
		switch (m_menu.GetSelection())
		{
		case LOGMENU_CLOSE:
			m_bOpen = FALSE;
			break;
		case LOGMENU_CLEAR:
			Clear();
			break;
		case LOGMENU_SAVE:
			ShowSaveKeyboard(L"Save As", m_strSavePath.c_str());
			break;
		}
		return S_OK;
	}
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
		return S_OK;
	}
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen || g_virtualKeyboard.m_bCancelled)
			return S_OK;
		if (wcslen(g_virtualKeyboard.GetText()) == 0)
			return S_OK;
		m_strSavePath = g_virtualKeyboard.GetResult();
		std::string message;
		if (!CreateDirectoryTree(GetPathDirectory(m_strSavePath.c_str()).c_str()))
		{
			StrFormat(message, "Unable to save log.\nError Code: %d", (int)GetLastError());
			ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			return S_OK;
		}
		std::string text;
		GetText(&text);
		if (!WriteTextFile(m_strSavePath.c_str(), text.c_str(), FALSE))
		{
			StrFormat(message, "Unable to save log.\nError Code: %d", (int)GetLastError());
			ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		}
		return S_OK;
	}

	if (*action == INPUT_LTHUMB && pad != NULL && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}
	switch (*action)
	{
	case INPUT_START:
	case INPUT_WHITE:
		m_bMenuOpen = TRUE;
		m_menu.Show(0);
		break;
	case INPUT_B:
	case INPUT_BACK:
		m_bOpen = FALSE;
		break;
	case INPUT_LTRIGGER:
	case INPUT_UP:
		ScrollUp();
		break;
	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		ScrollDown();
		break;
	}
	*action = INPUT_NONE;
	return S_OK;
}

// 0x0011FCC1 (this in eax)
HRESULT CFTPLog::Render()
{
	DimScreen(0.3f);
	float scale = g_pFtpSettings->m_fFontScale;
	m_pFont->SetScaleFactors(scale, scale);
	m_pCaptionFont->SetScaleFactors(scale, scale);
	float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	float header = m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + 4.0f;
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 2);
	DrawRect(m_fX, m_fY, m_fWidth, header, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	m_pCaptionFont->DrawText(m_fX + s_fLogMargin + 2.0f,
	                         (m_pCaptionFont->m_fScaleFactorY * m_pCaptionFont->m_fFontHeight + s_fLogMargin) * 0.5f +
	                             m_fY,
	                         g_colorScheme.CAPTION_TEXTCOLOR, L"FTP Event Log", XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                         (m_fWidth - s_fLogMargin * 2.0f) - 4.0f);
	float listHeight = ((m_fHeight - header) - s_fLogMargin) - 4.0f;
	DrawRect(m_fX + s_fLogMargin, m_fY + header + 2.0f, m_fWidth - s_fLogMargin * 2.0f, listHeight,
	         g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.WindowBackCOLOR, 1.0f, 1);
	float y = m_fY + lineHeight * 0.5f + header + 3.0f;
	float x = m_fX + s_fLogMargin + 2.0f;
	if (!s_bLogLineWidth)
	{
		s_bLogLineWidth = TRUE;
		s_fLogLineWidth = (m_fWidth - s_fLogMargin * 2.0f) - 4.0f;
	}
	float bottom = m_fY + m_fHeight;

	// The lines from m_nScroll; m_nVisible becomes how many fit.
	unsigned index = (unsigned)m_nTop;
	SetScissorRect(TRUE, m_fX + 1.0f, m_fY + 1.0f, m_fWidth - 2.0f, listHeight);
	m_pFont->Begin();
	for (std::vector<SFtpLogLine>::iterator it = m_lines.begin() + m_nScroll; it != m_lines.end(); ++it)
	{
		DWORD color = g_colorScheme.WindowForeCOLOR;
		if (it->type == FTPLOG_REPLY)
			color = 0xFF008000;
		else if (it->type == FTPLOG_ERROR)
			color = 0xFFFF0000;
		// (0x10: a CXBFontEx flag. TODO: verify)
		m_pFont->DrawText(x, y, color, it->text.c_str(), XBFONT_CENTER_Y | 0x10, s_fLogLineWidth);
		y += lineHeight;
		if (bottom < y)
		{
			m_nVisible = (int)(index - (unsigned)m_nTop);
			if (index < (unsigned)m_nScroll)
				m_nScroll = (int)index;
			break;
		}
		index++;
	}
	m_pFont->End();
	SetScissorRect(FALSE, 0.0f, 0.0f, 0.0f, 0.0f);

	if (m_bMenuOpen)
	{
		m_menu.Render();
	}
	else if (m_bMessageShown)
	{
		if (m_pMessageBox == NULL)
			m_bMessageShown = FALSE;
		else
			m_pMessageBox->Render();
	}
	else if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.Render();
	}
	return S_OK;
}
