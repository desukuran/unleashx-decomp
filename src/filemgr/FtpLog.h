// The FTP log: what CDirFTP sends and receives, shown by the FTP
// manager's "View Log..." in a window that scrolls with the d-pad and has
// a "Close / Clear / Save..." menu (START/WHITE). One global, g_ftpLog.
// See FtpLog.cpp.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "../ui/MenuDialogs.h"

class CMessageBox;
class CXBFont;
struct SGamepadState;

// What a log line is (its color).
enum FtpLogType
{
	FTPLOG_COMMAND = 0,   // a command sent
	FTPLOG_REPLY = 1,     // a reply (green)
	FTPLOG_ERROR = 2,     // an error, and replies 401 and up (red)
	FTPLOG_STATUS = 3,    // the client's own messages
};

// A line. 0x20 bytes.
struct SFtpLogLine
{
	// (this in esi)
	SFtpLogLine(const wchar_t *text, int type);                  // 0x0001E705

	std::wstring text;                 // +0x00
	int type;                          // +0x1C FtpLogType
};

// vtable 0x0024B654, 0x100 bytes.
class CFTPLog
{
public:
	CFTPLog();                                                   // 0x001235D0 (this in edi)
	// Adds text, line by line (at most 1000 lines are kept).
	virtual void Add(const char *text, int type);                // 0 (0x0001E736 -> 0x001216B6)
	virtual ~CFTPLog();                                          // 1 (0x00120DBE; deleting 0x0001EDC9)

	// The font and the menu. Opens the window. (this in esi, font in ebx)
	HRESULT Init(CXBFont *font);                                 // 0x001215FF
	// (this in edi)
	void Clear();                                                // 0x0001E747
	// (this in eax)
	void ResetRect();                                            // 0x0001E769
	// (this in eax)
	HRESULT Render();                                            // 0x0011FCC1
	HRESULT ProcessInput(int *action, SGamepadState **pad);      // 0x001232DD
	void ScrollUp();                                             // 0x0011F299
	void ScrollDown();                                           // 0x0011F261 (this in eax)
	// The lines joined with CRLF. (this in edi, out in ebx)
	void GetText(std::string *out);                              // 0x001215AC
	// The keyboard for "Save As", with path or the first free
	// C:\LogNNNN.txt.
	void ShowSaveKeyboard(const wchar_t *title, const char *path);   // 0x00120CAC
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x00123199 (this in edi)

	std::vector<SFtpLogLine> m_lines;  // +0x04
	int m_nScroll;                     // +0x14 the first line drawn
	float m_fX;                        // +0x18
	float m_fY;                        // +0x1C
	float m_fWidth;                    // +0x20
	float m_fHeight;                   // +0x24
	CXBFont *m_pFont;                  // +0x28
	CXBFont *m_pCaptionFont;           // +0x2C the dialog font, else m_pFont
	COptionMenu m_menu;                // +0x30
	CMessageBox *m_pMessageBox;        // +0xC4
	BOOL m_bMessageShown;              // +0xC8
	BOOL m_bMenuOpen;                  // +0xCC
	BOOL m_bKeyboardOpen;              // +0xD0
	int m_nLastLine;                   // +0xD4 the lines - 1
	int m_nTop;                        // +0xD8
	int m_nVisible;                    // +0xDC lines that fit
	BOOL m_bOpen;                      // +0xE0
	std::string m_strSavePath;         // +0xE4
};

extern CFTPLog g_ftpLog;                // 0x00277720
