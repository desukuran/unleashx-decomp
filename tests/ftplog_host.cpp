// Host stand-ins for the FTP log window (src/filemgr/FtpLog.cpp is Xbox
// only: it draws and uses the menus). CDirFTP only calls Add().
#include <cstdio>

#include "../src/filemgr/FtpLog.h"

COptionMenu::COptionMenu(BOOL) {}
COptionMenu::~COptionMenu() {}
HRESULT COptionMenu::Create(CXBFont *, const wchar_t *, float, float, float, float, int) { return S_OK; }
void COptionMenu::ProcessInput(int *, SGamepadState **) {}
void COptionMenu::Render() {}

CFTPLog::CFTPLog()
	: m_nScroll(0), m_pFont(NULL), m_pCaptionFont(NULL), m_pMessageBox(NULL), m_bMessageShown(FALSE),
	  m_bMenuOpen(FALSE), m_bKeyboardOpen(FALSE), m_nLastLine(0), m_nTop(0), m_nVisible(0), m_bOpen(FALSE)
{
}
CFTPLog::~CFTPLog() {}
void CFTPLog::Add(const char *text, int type) { printf("[ftp %d] %s\n", type, text); }
