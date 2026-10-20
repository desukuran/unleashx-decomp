// CNotePad: the text editor (mode 8, the "TextEditor" action and the file
// manager's text files). See Screens.h.
//
// Controls: the d-pad (and the left stick, with the keyboard hidden) moves
// the cursor, the triggers move up and down, B deletes, X is backspace, Y a
// line break, BLACK a tab, WHITE shows or hides the keyboard, START the menu,
// BACK exits. With the keyboard shown the left stick walks its keys and A
// types; clicking the right stick gives the keyboard all of the input. A
// debug keyboard types directly (Enter, the arrows, Home/End, PgUp/PgDn,
// the Windows keys for the menu, Esc to exit).
#include <cmath>
#include <cstring>
#include <cwchar>
#include <fstream>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../input/XBInput.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"
#include "UnleashX.h"

// A DrawText flag the editor's lines use. TODO: verify what it does
#define XBFONT_CLIPPED 0x10

// Past this column the view scrolls sideways (see Render).
#define NOTEPAD_SCROLL_COLUMN 40
#define NOTEPAD_MARGIN 3.0f

// The START menu.
enum
{
	MENU_NEW = 0,
	MENU_OPEN = 1,
	MENU_SAVE = 2,
	MENU_SAVE_AS = 3,
	MENU_EXIT = 4,
};

// Debug keyboard keys (OnKey).
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_DELETE 0x2E
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C

static const char *const s_szSaveChanges =
	"You have made changes to your document.\nDo you want to save the changes?";

// The part of a path after the last backslash.
static std::string GetFileNamePart(const std::string &path)
{
	int slash = StrReverseFind(path, "\\", -1);
	std::string name = path;
	if (slash != -1)
		name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
	return name;
}

// A line of UTF-8 as a wide line (inlined in Load and Open). FALSE if out
// of memory.
static BOOL ConvertLine(const std::string &text, std::wstring &line)
{
	unsigned int length = (unsigned int)text.size();
	wchar_t *buffer = new wchar_t[length + 1];
	if (buffer == NULL)
		return FALSE;
	line.erase();
	if (length != 0)
	{
		StrUtf8ToWide(text.c_str(), buffer, length + 1);
		// Quirk: takes as many characters as the line had bytes, so text
		// with multibyte characters gets trailing junk.
		line.assign(buffer, length);
	}
	delete[] buffer;
	return TRUE;
}

// 0x00104605 (this in ebx)
CNotePad::CNotePad()
{
	m_bShowKeyboard = FALSE;
	m_fY = 5.0f;
	m_fX = 5.0f;
	m_bKeyboardOpen = FALSE;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_bModified = FALSE;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_bMenuOpen = FALSE;
	m_pFont = NULL;
	m_bActive = FALSE;
	m_pDialogFont = NULL;
	m_fKeyboardHeight = 0.0f;
	wcscpy(m_szTitle, L"New Text Document");
	m_nLastLine = 0;
	Reset();
	m_cursorTimer.Resume();
	m_keyEvents.m_pOwner = this;
	m_stickTimer.Stop();
	m_unknown308 = 0.0f;
	m_bKeyboardFocus = FALSE;
	m_nPending = PENDING_NONE;
	m_pFileBrowser = NULL;
	m_bFileBrowserOpen = FALSE;
	m_pKeystroke = GetKeystroke();   // (the original stores the address directly)
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00103FB6
CNotePad::~CNotePad()
{
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	if (m_pFileBrowser != NULL)
	{
		delete m_pFileBrowser;
		m_pFileBrowser = NULL;
	}
	Reset();
	m_pFont = NULL;
}

// 0x00024830
BOOL CNotePad::Init(CXBFontEx *font)
{
	// (S_OK, so this says FALSE.)
	return (BOOL)Init(font, NULL, NULL, NULL);
}

// 0x00104399 (font in eax, this in esi)
HRESULT CNotePad::Init(CXBFont *font, IDirect3DTexture8 *keys, CXBPackedResource *resource, SIRRemote *ir)
{
	if (m_pFont == NULL)
	{
		m_pFont = font;
		m_pDialogFont = m_pSettings->m_pDialogFont;
		if (m_pDialogFont == NULL)
			m_pDialogFont = font;

		m_keyboard.m_bEmbedded = TRUE;
		if (resource == NULL && g_pApp != NULL)
			resource = &g_pApp->m_resource;
		m_keyboard.Create(font, font, keys, resource, 0.0f, 0.0f, &m_keyEvents);
		if (ir != NULL)
			m_keyboard.m_pIR = ir;

		m_menu.Create(font, L"Select", 0.0f, 0.0f, 0.0f, 0.0f, 10);
		m_menu.AddOption(L"New Document", 0);
		m_menu.AddOption(L"Open Document...", 0);
		m_menu.AddOption(L"Save", 0);
		m_menu.AddOption(L"Save As...", 0);
		m_menu.AddOption(L"Exit", 0);
		ResetWindow();
	}
	m_bActive = TRUE;
	return S_OK;
}

// 0x0002484C (this in eax)
void CNotePad::ResetWindow()
{
	m_fY = 5.0f;
	m_fX = 5.0f;
	m_fWidth = 630.0f;
	m_fHeight = 470.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
}

// 0x00103910 (this in edi)
void CNotePad::Reset()
{
	m_nCol = 0;
	m_lines.clear();
	m_nLine = 0;
	m_nLastLine = 0;
	m_nTopLine = 0;
	m_nColOffset = 0;
	m_nChars = 0;
	m_bModified = FALSE;
	m_bMenuOpen = FALSE;
	m_nVisibleLines = 19;
}

// 0x001036F6 (this in esi)
void CNotePad::Recount()
{
	m_nChars = 0;
	for (std::vector<std::wstring>::iterator it = m_lines.begin(); it != m_lines.end(); ++it)
		m_nChars += (UINT)it->size() + 2;
	if (m_nChars > 1)
		m_nChars -= 2;
	m_nLastLine = (UINT)m_lines.size();
	if (m_nLastLine != 0)
		m_nLastLine--;
}

// 0x001036D5 (this in eax)
void CNotePad::ClampColumn()
{
	std::wstring &line = m_lines[m_nLine];
	if (line.size() < m_nCol + m_nColOffset)
	{
		m_nColOffset = 0;
		m_nCol = (UINT)line.size();
	}
}

// 0x00103741 (this in eax)
void CNotePad::MoveUp()
{
	if (m_nLine == 0)
		return;
	m_nLine--;
	// (See OnKey's PgUp: a wrapped m_nTopLine counts all the way down.)
	while (m_nLine < m_nTopLine)
		m_nTopLine--;
	ClampColumn();
}

// 0x00103765 (this in eax)
void CNotePad::MoveLeft()
{
	if (m_nCol == 0)
	{
		// To the end of the line above.
		if (m_nLine != 0)
		{
			MoveUp();
			m_nCol = (UINT)m_lines[m_nLine].size();
		}
		return;
	}
	m_nCol--;
	std::wstring &line = m_lines[m_nLine];
	if (line.size() < m_nCol)
	{
		m_nCol = (UINT)line.size();
		if (m_nCol > NOTEPAD_SCROLL_COLUMN)
			ClampColumn();
	}
}

// 0x001037AD (this in eax)
void CNotePad::MoveDown()
{
	if (m_nLine >= m_nLastLine)
		return;
	m_nLine++;
	while (m_nTopLine + m_nVisibleLines < m_nLine)
		m_nTopLine++;
	ClampColumn();
}

// 0x00103867 (this in eax)
void CNotePad::MoveRight()
{
	m_nCol++;
	std::wstring *line = CurrentLine();
	if (line != NULL && line->size() < m_nCol)
	{
		// To the start of the line below.
		if (m_nLine < m_nLastLine)
		{
			MoveDown();
			m_nCol = 0;
			return;
		}
		m_nCol = (UINT)line->size();
	}
	if (m_nCol > NOTEPAD_SCROLL_COLUMN)
		ClampColumn();
}

// 0x00103A4F (this on the stack)
void CNotePad::SplitLine()
{
	std::wstring rest;
	if (m_lines.size() == 0)
	{
		m_lines.push_back(rest);
	}
	else
	{
		std::wstring *line = CurrentLine();
		if (line != NULL)
		{
			if (m_nLine == m_nLastLine && line->size() <= m_nCol)
			{
				m_lines.push_back(rest);
			}
			else
			{
				// At column 0 an empty line goes in before this one; else
				// the text after the cursor moves to a new line below.
				std::vector<std::wstring>::iterator where = m_lines.begin() + m_nLine;
				if (m_nCol != 0)
				{
					rest = line->substr(m_nCol, line->size() - m_nCol);
					line->resize(m_nCol);
					where = m_lines.begin() + m_nLine + 1;
				}
				m_lines.insert(where, rest);
			}
			m_nCol = 0;
			m_bModified = TRUE;
		}
	}
	Recount();
	MoveDown();
}

// 0x00103B58 (this on the stack)
BOOL CNotePad::Load(const char *text)
{
	m_strPath.erase();
	memset(m_szTitle, 0, sizeof(m_szTitle));
	Reset();
	ResetWindow();
	if (text == NULL || strlen(text) == 0)
		return FALSE;

	std::string line;
	std::string all = text;
	StrRemoveChar(all, '\r');
	if (StrRight(all, 1).compare("\n") != 0)
		all.append(1, '\n');
	int start = 0;
	int end = StrFindChar(all, '\n');
	if (end != -1)
	{
		for (;;)
		{
			line = StrMid(all, start, end - start);
			std::wstring wide;
			if (!ConvertLine(line, wide))
				break;
			m_lines.push_back(wide);
			m_nLastLine++;
			start = end + 1;
			end = (int)all.find('\n', start);
			if (end == -1)
				break;
		}
	}
	if (m_nLastLine != 0)
		m_nLastLine--;
	m_bActive = TRUE;
	Recount();
	return TRUE;
}

// 0x00103D4D (this in esi)
void CNotePad::NewFile()
{
	m_strPath.erase();
	memset(m_szTitle, 0, sizeof(m_szTitle));
	wcscpy(m_szTitle, L"New Text Document");
	// The keyboard comes up unless a debug keyboard is plugged in. (The
	// original reads the int after the keystroke: g_keyboards[0].hDevice.)
	if (m_pKeystroke == NULL || g_keyboards[0].hDevice == NULL)
	{
		m_bShowKeyboard = TRUE;
		m_fKeyboardHeight = m_keyboard.m_fKeyHeight * 4.0f + 3.0f;
	}
	else
	{
		m_bShowKeyboard = FALSE;
		m_fKeyboardHeight = 0.0f;
	}
	Reset();
	m_bActive = TRUE;
	m_lines.push_back(std::wstring());
	ResetWindow();
}

// 0x00103E0D (this on the stack)
void CNotePad::AskFileName(const wchar_t *title, const char *path)
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
		StrFormat(name, "C:\\File%04d.txt", 1);
		for (UINT n = 2; FileExists(name.c_str()) && n < 9999; n++)
			StrFormat(name, "C:\\File%04d.txt", n);
	}
	if (name.size() != 0)
	{
		wchar_t text[262];
		// Quirk: the converted name isn't NUL-terminated.
		MultiByteToWideChar(CP_ACP, 0, name.c_str(), (int)name.size(), text, 0x104);
		g_virtualKeyboard.SetText(text);
	}
}

// 0x00103F23 (out in ebx, this in edi)
void CNotePad::GetText(std::string *out)
{
	if (out == NULL)
		return;
	out->erase();
	std::string line;
	for (std::vector<std::wstring>::iterator it = m_lines.begin(); it != m_lines.end(); ++it)
	{
		// Quirk: an empty last line still gets a line break, so a file that
		// ends in one grows a line every time it's opened and saved.
		if ((int)it->size() < 1)
		{
			out->append("\r\n");
			continue;
		}
		StrFormat(line, "%S", it->c_str());
		StrAppend(*out, line.c_str());
		if (it != m_lines.end() - 1)
			out->append("\r\n");
	}
}

// 0x0010403F (path in ecx, this on the stack)
BOOL CNotePad::Save(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	std::string dir = GetPathDirectory(path);
	if (!CreateDirectoryTree(dir.c_str()))
		return FALSE;
	SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
	std::string text = "";
	SetLastError(0);
	GetText(&text);
	try
	{
		FILE *file = fopen(path, "w+b");
		if (file == NULL)
			return FALSE;
		if (text.size() == 0)
		{
			// (Still "modified".)
			fclose(file);
			return TRUE;
		}
		if (fwrite(text.c_str(), text.size(), 1, file) == 0)
			return FALSE;   // (the file is left open)
		fclose(file);
		m_bModified = FALSE;
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

// 0x00104181 (this on the stack)
BOOL CNotePad::Open(const char *path)
{
	if (path == NULL || strlen(path) == 0)
	{
		Reset();
		ResetWindow();
		return FALSE;
	}

	m_strPath = path;
	std::string name = GetFileNamePart(m_strPath);
	StrUtf8ToWide(name.c_str(), m_szTitle, 0x2B);
	Reset();
	ResetWindow();

	// Quirk: a file that can't be opened just comes up empty (with no
	// lines at all).
	std::ifstream file;
	file.open(path);
	file.seekg(0, std::ios::end);
	m_nChars = (UINT)(int)file.tellg();   // (Recount replaces it below)
	file.seekg(0, std::ios::beg);
	std::string text;
	while (file.rdstate() == 0)
	{
		std::getline(file, text, '\n');
		std::wstring wide;
		if (!ConvertLine(text, wide))
			break;
		m_lines.push_back(wide);
		m_nLastLine++;
	}
	if (m_nLastLine != 0)
		m_nLastLine--;
	file.close();
	m_bActive = TRUE;
	Recount();
	return TRUE;
}

// 0x001044BE (this in edi)
void CNotePad::ShowMessage(const char *text, const wchar_t *title, int buttons)
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

// (inlined everywhere a save fails)
void CNotePad::ShowSaveError(const char *path)
{
	std::string message;
	DWORD error = GetLastError();
	StrFormat(message, "An error occured while saving %s.\nError Code: %d", path, error);
	ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
}

// 0x0010538A (this in edi)
void CNotePad::ShowOpenBrowser()
{
	if (m_pFileBrowser == NULL)
	{
		m_pFileBrowser = new CFileBrowser;
		if (m_pFileBrowser != NULL)
		{
			m_pFileBrowser->Create(m_pFont, L"Open Text File");
			m_pFileBrowser->AddFilter("txt");
			m_pFileBrowser->AddFilter("ini");
			m_pFileBrowser->AddFilter("xml");
		}
	}
	if (m_pFileBrowser == NULL)
	{
		m_bFileBrowserOpen = FALSE;
		return;
	}
	m_pFileBrowser->SetSelectFiles(1);
	m_pFileBrowser->m_bNewFolderItem = 0;
	m_pFileBrowser->Show(TRUE);
	m_bFileBrowserOpen = TRUE;
}

// 0x0010496B
BOOL CNotePad::Render()
{
	DimScreen(0.3f);
	float fontScale = m_pSettings->m_fFontScale;
	m_pDialogFont->SetScaleFactors(fontScale, fontScale);
	m_pFont->SetScaleFactors(fontScale, fontScale);
	float captionHeight = m_pDialogFont->m_fFontHeight * m_pDialogFont->m_fScaleFactorY + 4.0f;
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	float halfLine = lineHeight * 0.5f;

	// The window and its title.
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 2);
	DrawRect(m_fX, m_fY, m_fWidth, captionHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	         1.0f, 1);
	m_pDialogFont->DrawText(m_fX + NOTEPAD_MARGIN + 2.0f,
	                        (m_pDialogFont->m_fFontHeight * m_pDialogFont->m_fScaleFactorY + NOTEPAD_MARGIN) * 0.5f +
	                            m_fY,
	                        g_colorScheme.CAPTION_TEXTCOLOR, m_szTitle, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                        (m_fWidth - NOTEPAD_MARGIN * 2.0f) - 4.0f);

	// The text area, then the status line under it.
	float statusHeight = lineHeight + 2.0f;
	float textHeight = m_fHeight - captionHeight - NOTEPAD_MARGIN - 4.0f - m_fKeyboardHeight - statusHeight - 2.0f;
	DrawRect(m_fX + NOTEPAD_MARGIN, m_fY + captionHeight + 2.0f, m_fWidth - NOTEPAD_MARGIN * 2.0f, textHeight,
	         g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.WindowBackCOLOR, 1.0f, 1);
	wchar_t text[80];
	swprintf(text, 80, L"Ln %d  Col %d", m_nLine, m_nCol);
	float statusY = m_fY + textHeight + captionHeight + 2.0f;
	DrawRect(m_fX + NOTEPAD_MARGIN, statusY, m_fWidth - NOTEPAD_MARGIN * 2.0f, statusHeight,
	         g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	statusY += halfLine;
	m_pFont->DrawText(m_fX + NOTEPAD_MARGIN + 3.0f, statusY, g_colorScheme.DIALOG_TEXTCOLOR, text, XBFONT_CENTER_Y,
	                  0.0f);
	FormatIntW(text, (int)m_nChars);
	wcscat(text, L" Bytes");
	m_pFont->DrawText(m_fX + m_fWidth - NOTEPAD_MARGIN * 2.0f - 2.0f, statusY, g_colorScheme.DIALOG_TEXTCOLOR, text,
	                  XBFONT_RIGHT | XBFONT_CENTER_Y, 0.0f);

	// The lines.
	float y = m_fY + halfLine + captionHeight + 3.0f;
	float x = m_fX + NOTEPAD_MARGIN + 2.0f;
	// Quirk: from the first window drawn only.
	static float s_fTextWidth = (m_fWidth - NOTEPAD_MARGIN * 2.0f) - 4.0f;   // 0x0027A404
	float bottom = m_fY + m_fHeight - m_fKeyboardHeight - 4.0f - statusHeight - 2.0f;
	float cursorY = y - halfLine;
	UINT line = m_nTopLine;
	std::wstring current;
	int firstCol = 0;   // the columns of the cursor's line in view
	int lastCol = 0;
	if (m_lines.size() != 0)
	{
		current = m_lines[m_nLine];
		lastCol = (int)current.size();
		if (m_nCol > NOTEPAD_SCROLL_COLUMN)
		{
			// Scroll right until the text up to the cursor fits, then cut
			// what's left to the width.
			int count = (int)m_nCol;
			while (m_pFont->GetTextWidth(current.substr(firstCol, count).c_str()) > s_fTextWidth - 20.0f)
			{
				firstCol++;
				count--;
			}
			while (m_pFont->GetTextWidth(current.substr(firstCol, lastCol).c_str()) > s_fTextWidth - 5.0f)
				lastCol--;
		}

		SetScissorRect(TRUE, m_fX + 1.0f, m_fY + 1.0f, m_fWidth - 1.0f, (bottom - m_fY) - 1.0f);
		m_pFont->Begin();
		for (std::vector<std::wstring>::iterator it = m_lines.begin() + line; it != m_lines.end(); ++it, ++line)
		{
			if (line == m_nLine)
				cursorY = y - halfLine;
			// Every line scrolls with the cursor's.
			if (firstCol < (int)it->size())
				m_pFont->DrawText(x, y, g_colorScheme.WindowForeCOLOR, it->c_str() + firstCol,
				                  XBFONT_CENTER_Y | XBFONT_CLIPPED, s_fTextWidth);
			y += lineHeight;
			if (bottom < y + halfLine)
			{
				m_nVisibleLines = line - m_nTopLine;
				if (line < m_nLine)
					m_nLine = line;
				break;
			}
		}
		m_pFont->End();
		SetScissorRect(FALSE, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	// The cursor, an I-beam, on for 3/4 of every second.
	if (fmod((double)m_cursorTimer.GetElapsedSeconds(), 1.0) < 0.75)
	{
		float cursorX = x;
		float offset = 0.0f;
		if (m_nCol == 0)
		{
			// Quirk: trims lastCol for nothing.
			if (current.size() != 0)
				while (m_pFont->GetTextWidth(current.substr(0, lastCol).c_str()) > s_fTextWidth - 5.0f)
					lastCol--;
		}
		else if (lastCol != 0)
		{
			std::wstring before = current.substr(firstCol, lastCol).substr(0, m_nCol - firstCol);
			float height;
			m_pFont->GetTextExtent(before.c_str(), &offset, &height, FALSE);
			cursorX = offset + x;
		}

		IDirect3DDevice8 *device = g_pd3dDevice;
		device->SetTexture(0, NULL);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
		device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.WindowForeCOLOR);
		float top = cursorY + 2.0f;
		float cursorBottom = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY + 4.0f + cursorY;
		device->Begin(D3DPT_LINELIST);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX - 2.0f, top, 1.0f, 1.0f);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX + 2.0f, top, 1.0f, 1.0f);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX - 1.0f, top, 1.0f, 1.0f);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX - 1.0f, cursorBottom, 1.0f, 1.0f);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX, top, 1.0f, 1.0f);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX, cursorBottom, 1.0f, 1.0f);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX - 2.0f, cursorBottom, 1.0f, 1.0f);
		device->SetVertexData4f(D3DVSDE_VERTEX, cursorX + 2.0f, cursorBottom, 1.0f, 1.0f);
		device->End();
	}

	if (m_bShowKeyboard)
		m_keyboard.Render();

	// The pop-ups.
	if (m_bMenuOpen)
	{
		m_menu.Render();
	}
	else if (m_bFileBrowserOpen)
	{
		if (m_pFileBrowser != NULL)
			m_pFileBrowser->Render();
		else
			m_bFileBrowserOpen = FALSE;
	}
	else if (m_bMessageShown)
	{
		if (m_pMessageBox != NULL)
			m_pMessageBox->Render();
		else
			m_bMessageShown = FALSE;
	}
	else if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.Render();
	}
	return FALSE;
}

// 0x00105438
HRESULT CNotePad::ProcessInput(int *action, SGamepadState **pad)
{
	// A message box: the "save changes?" answers (errors just close).
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
		if (m_bMessageShown)
			return S_OK;

		int result;
		switch (m_nPending)
		{
		case PENDING_NEW:
			result = m_pMessageBox->GetResult();
			if (result == MSGBOX_YES)
			{
				if (m_strPath.size() == 0)
				{
					m_nPending = PENDING_SAVE_NEW;
					AskFileName(NULL, NULL);
					return S_OK;
				}
				if (Save(m_strPath.c_str()))
				{
					m_bModified = FALSE;
					NewFile();
				}
				else
				{
					ShowSaveError(m_strPath.c_str());
				}
			}
			else if (result == MSGBOX_NO)
			{
				NewFile();
				m_bModified = FALSE;
			}
			break;

		case PENDING_OPEN:
			result = m_pMessageBox->GetResult();
			if (result == MSGBOX_YES)
			{
				if (m_strPath.size() == 0)
				{
					m_nPending = PENDING_SAVE_OPEN;
					AskFileName(NULL, NULL);
					return S_OK;
				}
				if (Save(m_strPath.c_str()))
				{
					m_bModified = FALSE;
					ShowOpenBrowser();   // (m_nPending stays PENDING_OPEN)
					return S_OK;
				}
				ShowSaveError(m_strPath.c_str());
			}
			else if (result == MSGBOX_NO)
			{
				ShowOpenBrowser();
				m_bModified = FALSE;
			}
			break;

		case PENDING_EXIT:
			result = m_pMessageBox->GetResult();
			if (result == MSGBOX_YES)
			{
				if (m_strPath.size() == 0)
				{
					m_nPending = PENDING_SAVE_EXIT;
					AskFileName(NULL, NULL);
					return S_OK;
				}
				if (Save(m_strPath.c_str()))
				{
					m_bModified = FALSE;
					m_bActive = FALSE;
				}
				else
				{
					ShowSaveError(m_strPath.c_str());
				}
			}
			else if (result == MSGBOX_NO)
			{
				m_bModified = FALSE;
				m_bActive = FALSE;
			}
			break;

		default:
			return S_OK;
		}
		m_nPending = PENDING_NONE;
		return S_OK;
	}

	// "Open Text File".
	if (m_bFileBrowserOpen)
	{
		m_pFileBrowser->ProcessInput(action, pad);
		if (m_pFileBrowser->IsOpen())
			return S_OK;
		m_bFileBrowserOpen = FALSE;
		if (!m_pFileBrowser->m_bCancelled && !Open(m_pFileBrowser->GetSelectedPath().c_str()))
		{
			std::string message;
			DWORD error = GetLastError();
			StrFormat(message, "An error occured while trying to open '%s'.\nError Code: %d",
			          m_pFileBrowser->GetSelectedPath().c_str(), error);
			ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
		}
		m_nPending = PENDING_NONE;
		return S_OK;
	}

	// The START menu.
	if (m_bMenuOpen)
	{
		m_menu.ProcessInput(action, pad);
		m_bMenuOpen = m_menu.IsOpen();
		if (m_bMenuOpen || !m_menu.m_bChosen)
			return S_OK;
		switch (m_menu.GetSelection())
		{
		case MENU_NEW:
			if (!m_bModified)
			{
				NewFile();
				return S_OK;
			}
			m_nPending = PENDING_NEW;
			ShowMessage(s_szSaveChanges, L"Text Editor", MSGBOX_BUTTONS_YESNOCANCEL);
			break;

		case MENU_OPEN:
			if (m_bModified)
			{
				ShowMessage(s_szSaveChanges, L"Text Editor", MSGBOX_BUTTONS_YESNOCANCEL);
				m_nPending = PENDING_OPEN;
			}
			else
			{
				ShowOpenBrowser();
			}
			break;

		case MENU_SAVE:
			if (!m_bModified)
				break;
			if (m_strPath.size() == 0)
			{
				m_nPending = PENDING_SAVE_AS;
				AskFileName(NULL, NULL);
			}
			else if (Save(m_strPath.c_str()))
			{
				m_bModified = FALSE;
			}
			else
			{
				ShowSaveError(m_strPath.c_str());
			}
			break;

		case MENU_SAVE_AS:
			AskFileName(L"Save As", m_strPath.c_str());
			m_nPending = PENDING_SAVE_AS;
			m_bModified = TRUE;
			break;

		case MENU_EXIT:
			if (!m_bModified)
			{
				m_bActive = FALSE;
				break;
			}
			m_nPending = PENDING_EXIT;
			ShowMessage(s_szSaveChanges, L"Text Editor", MSGBOX_BUTTONS_YESNOCANCEL);
			break;
		}
		return S_OK;
	}

	// A file name typed on the global keyboard.
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return S_OK;
		if (g_virtualKeyboard.m_bCancelled || wcslen(g_virtualKeyboard.GetText()) == 0)
		{
			m_nPending = PENDING_NONE;
			return S_OK;
		}
		std::string path = g_virtualKeyboard.GetResult();
		std::string message;
		switch (m_nPending)
		{
		case PENDING_SAVE:
		case PENDING_SAVE_AS:
			if (m_nPending == PENDING_SAVE_AS && FileExists(path.c_str()))
			{
				// (No way to overwrite.)
				StrFormat(message, "'%s' Already Exists.", path.c_str());
				ShowMessage(message.c_str(), L"ERROR", MSGBOX_BUTTONS_OK);
			}
			else if (!Save(path.c_str()))
			{
				ShowSaveError(path.c_str());
			}
			else if (m_nPending == PENDING_SAVE_AS)
			{
				m_strPath = path;
				std::string name = GetFileNamePart(m_strPath);
				memset(m_szTitle, 0, sizeof(m_szTitle));
				StrUtf8ToWide(name.c_str(), m_szTitle, 0x2B);
			}
			break;

		case PENDING_SAVE_NEW:
			if (Save(path.c_str()))
				NewFile();
			else
				ShowSaveError(path.c_str());
			break;

		case PENDING_SAVE_OPEN:
			if (Save(path.c_str()))
				ShowOpenBrowser();
			else
				ShowSaveError(path.c_str());
			break;

		case PENDING_SAVE_EXIT:
			if (Save(path.c_str()))
			{
				m_bModified = FALSE;
				m_bActive = FALSE;
				return S_OK;
			}
			ShowSaveError(path.c_str());
			break;
		}
		m_nPending = PENDING_NONE;
		return S_OK;
	}

	// The editor itself.
	if (m_bShowKeyboard && *action == INPUT_RTHUMB_CLICK)
	{
		m_bKeyboardFocus = !m_bKeyboardFocus;
		return S_OK;
	}
	if (!m_bKeyboardFocus)
	{
		if (m_pKeystroke != NULL)
		{
			if (m_pKeystroke->ascii > 0x1F)
			{
				m_keyEvents.OnChar((wchar_t)m_pKeystroke->ascii);
				m_pKeystroke->ascii = 0;
				return S_OK;
			}
			BYTE key = m_pKeystroke->virtualKey;
			if (key != 0 && key < 0x5D)
			{
				m_keyEvents.OnKey(key);
				m_pKeystroke->ascii = 0;   // sic: the character, not the key
				*action = INPUT_NONE;
				return S_OK;
			}
		}

		// With the keyboard hidden the left stick works like the d-pad.
		if (!m_bShowKeyboard && *action == INPUT_LTHUMB && *pad != NULL)
		{
			if ((*pad)->fY1 > 0.0f)
				*action = INPUT_UP;
			else if ((*pad)->fY1 < 0.0f)
				*action = INPUT_DOWN;
			else if ((*pad)->fX1 > 0.0f)
				*action = INPUT_RIGHT;
			else if ((*pad)->fX1 < 0.0f)
				*action = INPUT_LEFT;
		}

		switch (*action)
		{
		case INPUT_START:
			// (START also reaches the keyboard below, if it's shown.)
			m_bMenuOpen = TRUE;
			m_menu.Show(0);
			break;
		case INPUT_B:
			m_keyEvents.OnDelete();
			return S_OK;
		case INPUT_BACK:
			*action = INPUT_NONE;
			if (!m_bModified)
			{
				m_bActive = FALSE;
				m_bMenuOpen = FALSE;
			}
			else
			{
				m_nPending = PENDING_EXIT;
				ShowMessage(s_szSaveChanges, L"Text Editor", MSGBOX_BUTTONS_YESNOCANCEL);
			}
			break;
		case INPUT_X:
			m_keyEvents.OnBackspace();
			return S_OK;
		case INPUT_Y:
			SplitLine();
			*action = INPUT_NONE;
			break;
		case INPUT_WHITE:
			m_bShowKeyboard = !m_bShowKeyboard;
			if (m_bShowKeyboard)
				m_fKeyboardHeight = m_keyboard.m_fKeyHeight * 4.0f + 3.0f;
			else
				m_fKeyboardHeight = 0.0f;
			*action = INPUT_NONE;
			break;
		case INPUT_BLACK:
			m_keyEvents.OnChar(L'\t');
			return S_OK;
		case INPUT_LTRIGGER:
		case INPUT_UP:
			MoveUp();
			*action = INPUT_NONE;
			break;
		case INPUT_RTRIGGER:
		case INPUT_DOWN:
			MoveDown();
			*action = INPUT_NONE;
			break;
		case INPUT_LEFT:
			MoveLeft();
			*action = INPUT_NONE;
			break;
		case INPUT_RIGHT:
			MoveRight();
			*action = INPUT_NONE;
			break;
		case INPUT_LTHUMB:
			if (!m_bShowKeyboard)
				return S_OK;
			// The stick walks the keyboard's keys, at most every 0.15 s.
			if (m_stickTimer.IsRunning() && m_stickTimer.GetElapsedSeconds() < 0.15f)
				return S_OK;
			m_stickTimer.Start();
			if (*pad != NULL)
			{
				if ((*pad)->fX1 > 0.0f)
					*action = INPUT_RIGHT;
				else if ((*pad)->fX1 < 0.0f)
					*action = INPUT_LEFT;
				else if ((*pad)->fY1 > 0.0f)
					*action = INPUT_UP;
				else if ((*pad)->fY1 < 0.0f)
					*action = INPUT_DOWN;
			}
			break;
		}
	}

	// The embedded keyboard gets the rest (only "no action" while hidden).
	if (!m_bShowKeyboard && *action != INPUT_NONE)
		return S_OK;
	m_keyboard.ProcessInput(action);
	return S_OK;
}

// 0x001038AE
void CNotePad::KeyboardCharEvent::OnChar(wchar_t ch)
{
	CNotePad *editor = m_pOwner;
	std::wstring *line = editor->CurrentLine();
	if ((ch > 0x1F || ch == L'\t') && line != NULL)
	{
		line->insert(editor->m_nCol, 1, ch);
		editor->m_nCol++;
		editor->Recount();
		if (editor->m_nCol > NOTEPAD_SCROLL_COLUMN)
			editor->ClampColumn();
		editor->m_bModified = TRUE;
	}
}

// 0x001037E7
void CNotePad::KeyboardCharEvent::OnDelete()
{
	CNotePad *editor = m_pOwner;
	if (editor->m_lines.size() == 0)
		return;
	std::wstring *line = editor->CurrentLine();
	if (line == NULL || line->size() == 0 || (int)line->size() <= (int)editor->m_nCol)
	{
		// At the end of a line: joins the next one.
		if (editor->m_nLine < editor->m_nLastLine)
		{
			editor->m_nLine++;
			editor->m_nCol = 0;
			OnBackspace();
			editor->Recount();
			editor->ClampColumn();
		}
	}
	else
	{
		line->erase(editor->m_nCol, 1);
		editor->Recount();
	}
	editor->m_bModified = TRUE;
}

// 0x00103950
void CNotePad::KeyboardCharEvent::OnBackspace()
{
	CNotePad *editor = m_pOwner;
	if (editor->m_lines.size() == 0)
		return;
	std::wstring *line = editor->CurrentLine();
	if ((int)editor->m_nCol < 1)
	{
		// At the start of a line: joins it to the one above.
		if (editor->m_nLine == 0)
			return;
		std::wstring &above = editor->m_lines[editor->m_nLine - 1];
		editor->m_nCol = (UINT)above.size();
		if (line->size() != 0)
			above.append(line->c_str());
		// (Quirk: clamped to the line being removed.)
		if (editor->m_nCol > NOTEPAD_SCROLL_COLUMN)
			editor->ClampColumn();
		editor->m_lines.erase(editor->m_lines.begin() + editor->m_nLine);
		editor->Recount();
		editor->MoveUp();
		editor->m_bModified = TRUE;
	}
	else if (line != NULL && (int)editor->m_nCol <= (int)line->size())
	{
		editor->m_nCol--;
		line->erase(editor->m_nCol, 1);
		editor->Recount();
		editor->m_bModified = TRUE;
	}
}

// 0x0010473E
void CNotePad::KeyboardCharEvent::OnKey(int virtualKey)
{
	CNotePad *editor = m_pOwner;
	std::wstring *line;
	UINT oldLine;
	switch ((BYTE)virtualKey)
	{
	case VK_END:
		line = editor->CurrentLine();
		if (line != NULL && line->size() != 0)
			editor->m_nCol = (UINT)line->size();
		break;
	case VK_BACK:
		OnBackspace();
		break;
	case VK_TAB:
		OnChar(L'\t');
		break;
	case VK_RETURN:
		editor->SplitLine();
		break;
	case VK_ESCAPE:
		if (editor->m_bModified)
		{
			editor->m_nPending = PENDING_EXIT;
			editor->ShowMessage(s_szSaveChanges, L"Text Editor", MSGBOX_BUTTONS_YESNOCANCEL);
		}
		else
		{
			editor->m_bActive = FALSE;
			editor->m_bMenuOpen = FALSE;
		}
		break;
	case VK_SPACE:
		OnChar(L' ');
		break;
	case VK_PRIOR:
		if (editor->m_nLine <= editor->m_nVisibleLines || editor->m_nVisibleLines == 0)
			break;
		// Quirk: m_nTopLine can wrap below 0 here; MoveUp then counts it
		// back down one at a time.
		editor->m_nTopLine -= editor->m_nVisibleLines;
		editor->m_nLine += 1 - editor->m_nVisibleLines;
		editor->MoveUp();
		break;
	case VK_NEXT:
		if (editor->m_nVisibleLines == 0)
			break;
		oldLine = editor->m_nLine;
		editor->m_nLine += editor->m_nVisibleLines - 1;
		if (editor->m_nLine > editor->m_nLastLine)
			editor->m_nLine = editor->m_nLastLine;
		if (editor->m_nVisibleLines + editor->m_nTopLine > editor->m_nLastLine)
			editor->m_nTopLine = editor->m_nLastLine;
		else
			editor->m_nTopLine += editor->m_nVisibleLines;
		if (oldLine != editor->m_nLine)
			editor->MoveDown();
		break;
	case VK_HOME:
		editor->m_nCol = 0;
		break;
	case VK_LEFT:
		editor->MoveLeft();
		break;
	case VK_UP:
		editor->MoveUp();
		break;
	case VK_RIGHT:
		editor->MoveRight();
		break;
	case VK_DOWN:
		editor->MoveDown();
		break;
	case VK_DELETE:
		OnDelete();
		break;
	case VK_LWIN:
	case VK_RWIN:
		editor->m_bMenuOpen = TRUE;
		editor->m_menu.Show(0);
		break;
	}
}
