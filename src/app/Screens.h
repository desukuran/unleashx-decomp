// The full-screen tools CUnleashX switches to (CUnleashX::SetMode). Only the
// interface the app code uses so far; all TODO: decompile.
#pragma once

#include <string>

#include "../common/Platform.h"

class CXBFontEx;
class CXBFont;
struct IDirect3DTexture8;
struct SGamepadState;

// Screen modes (CUnleashX::m_nMode)
enum AppMode
{
	MODE_MENU = 1,
	MODE_FILEMANAGER = 2,
	MODE_GAMEMANAGER = 3,
	MODE_SETTINGS = 5,
	MODE_SKINS = 6,
	MODE_FTPSITES = 7,
	MODE_TEXTEDITOR = 8,
	MODE_MUSIC = 9,
};

class CFileManager
{
public:
	CFileManager();                                       // 0x00122BA8? TODO: verify
	virtual ~CFileManager();
	HRESULT Init(int unknown1, int unknown2, IDirect3DTexture8 *folderIcon,
	             IDirect3DTexture8 *fileIcon);            // 0x001294E2
	void OpenPath(const char *path);                      // 0x00128C01
	void Reset();                                         // 0x0002B4C9
	void SetBackground(IDirect3DTexture8 *texture);       // 0x0002B4A1

	BYTE m_unknown004[0x8D8];
	BOOL m_bInitialized;                                  // +0x8DC
	int m_unknown8E0;                                     // +0x8E0
	BOOL m_bSafeBorder;                                   // +0x8E4
	BOOL m_bDrawBorder;                                   // +0x8E8
	IDirect3DTexture8 *m_pBackground;                     // +0x8EC
};

// Game saves / game list manager ("SavesManager", "GameManager"). 0x210 bytes.
class CGameManager
{
public:
	CGameManager();                                       // 0x0011C649
	virtual ~CGameManager();
	HRESULT Init(CXBFontEx *font);                        // 0x0011E048
	void Show();                                          // 0x0011BAFA
};

// The settings menu. 0xA0 bytes.
class CSettingsMenu
{
public:
	CSettingsMenu();                                      // 0x000BEBFD
	virtual ~CSettingsMenu();
	HRESULT Init(CXBFontEx *font);                        // 0x000C18A3
	void Show();                                          // 0x0002B530
};

// The skin chooser. 0xD0 bytes.
class CSkinBrowser
{
public:
	CSkinBrowser();                                       // 0x000F6597
	virtual ~CSkinBrowser();
	HRESULT Init(IDirect3DTexture8 *checked, IDirect3DTexture8 *unchecked,
	             CXBFontEx *font);                        // 0x000F5DEC
	void Show();                                          // 0x000F6791

	BYTE m_unknown04[0x54];
	std::string m_strSkinPath;                            // +0x58
};

class CNotePad
{
public:
	CNotePad();                                           // 0x00103A0C? TODO: verify
	virtual ~CNotePad();
	void Init(int, int, int);                             // 0x00104399
	void NewFile();                                       // 0x00103D4D
	HRESULT Open(const char *path);                       // 0x00104181
};

// The "SetClock" screen: six CUpDown spinners (month, day, year, hour, ...),
// started from the current time. Also the date/time pop-up of the settings
// pages. 0x2A4 bytes.
class CClockEditor
{
public:
	CClockEditor();                                       // 0x000C66A7
	~CClockEditor();                                      // 0x000C147B
	HRESULT Init(CXBFontEx *font);                        // 0x000C1FAC
	void Show();                                          // 0x000C0B13
	void Render();                                        // 0x000BFC70
	void ProcessInput(int *action, SGamepadState **pad);  // 0x000C1102

	BYTE m_unknown00[0x0C];
	BOOL m_bOpen;                                         // +0x0C
	BYTE m_unknown10[0x2A4 - 0x10];
};

// The settings pages' IP address pop-up: four CUpDown spinners (0-255).
// 0x1C4 bytes.
class CIPAddressEditor
{
public:
	CIPAddressEditor();                                   // 0x000C662B
	~CIPAddressEditor();                                  // 0x000C0AB6
	HRESULT Init(CXBFont *font);                          // 0x000C1D45
	void Show();                                          // 0x000C066E reads the address set before
	void Render();                                        // 0x000BF86F
	// The original takes address in eax.
	void SetAddress(const wchar_t *address);              // 0x000C1A55
	// "a.b.c.d" from the spinners. The original takes this in esi.
	void GetAddress(wchar_t *address);                    // 0x000BFC30
	// A accepts, B/BACK cancels (and resets the spinners). The original
	// takes this in eax and action in edi.
	void ProcessInput(int *action, SGamepadState **pad);  // 0x000C09EE

	BYTE m_unknown00[0x0C];
	BOOL m_bOpen;                                         // +0x0C
	BYTE m_unknown10[0x170 - 0x10];
	std::wstring m_strTitle;                              // +0x170
	std::string m_strAddress;                             // +0x18C
	BOOL m_bCancelled;                                    // +0x1AC
	BYTE m_unknown1B0[0x1C4 - 0x1B0];
};

// The settings pages' file pop-up ("Browse XBE"). 0x180 bytes.
class CFileBrowser
{
public:
	CFileBrowser();                                       // 0x00127A32
	~CFileBrowser();                                      // 0x00127013
	void Create(CXBFont *font, const wchar_t *title);     // 0x0012C378
	void Render();                                        // 0x00129B10
};

// Message box buttons, in the order they're added, and GetResult() values.
enum MessageBoxResult
{
	MSGBOX_OK = 0,
	MSGBOX_CANCEL = 1,
	MSGBOX_ABORT = 2,
	MSGBOX_RETRY = 3,
	MSGBOX_IGNORE = 4,
	MSGBOX_YES = 5,
	MSGBOX_NO = 6,
	MSGBOX_NONE = 7,
};
// Show()'s button sets: the Windows MB_ values.
#define MSGBOX_BUTTONS_OK 0
#define MSGBOX_BUTTONS_OKCANCEL 1
#define MSGBOX_BUTTONS_ABORTRETRYIGNORE 2
#define MSGBOX_BUTTONS_YESNOCANCEL 3
#define MSGBOX_BUTTONS_YESNO 4
#define MSGBOX_BUTTONS_RETRYCANCEL 5
#define MSGBOX_BUTTONS_CANCEL 6

// The message box (OK / Cancel / Abort / Retry / Ignore / Yes / No). 0x158 bytes.
class CMessageBox
{
public:
	CMessageBox();                                        // 0x000B10D4
	virtual ~CMessageBox();
	// The font is passed in eax. The callers pass 0 for the rest and 10.
	HRESULT Create(CXBFont *font, const wchar_t *title, float x, float y, float width, float height,
	               int maxVisible);                       // 0x000B084D
	void AddButton(const wchar_t *text);                  // 0x000AFB3D
	// buttons: MSGBOX_BUTTONS_*
	void Show(const char *text, const wchar_t *title, int buttons = 0);   // 0x000B0F74
	void ProcessInput(int *action);                       // 0x000AF925
	BOOL IsOpen() const;                                  // 0x000AF8E9
	int GetResult();                                      // 0x000AFA59 MSGBOX_*
	void SetSelection(int button);                        // 0x000AF8CF
	void Render();                                        // 0x000AFC64
};

// The on-screen keyboard; $KBResult$ in an item's arguments is its last text.
class CVirtualKeyboard
{
public:
	std::string GetResult();                              // 0x00014242
	void SetTitle(const wchar_t *title);                  // 0x0001433E
	void SetText(const wchar_t *text);                    // 0x00014302
	// Starts a new input: clears the text and the done state. TODO: verify
	void Open();                                          // vtable slot 3, 0x000DC9D7
	void ProcessInput(int *action);                       // vtable slot 1, 0x000DE58D
	void Render();                                        // vtable slot 2, 0x000DDA5A
	// The typed text, or "" if cancelled.
	const wchar_t *GetText();                             // 0x0001421F
	BOOL IsDone() const;                                  // 0x00014325 OK or cancelled
};
extern CVirtualKeyboard g_virtualKeyboard;               // 0x00278328
extern BOOL g_bKeyboardPassword;                         // 0x0027846C masked input
extern int g_nKeyboardMaxLength;                         // 0x00278470
extern BOOL g_bKeyboardAccepted;                         // 0x0027A5CC TODO: verify (also read for the passcode pad)
// TODO: verify. 0x00278460 is g_virtualKeyboard+0x138: BACK sets it to 1
// (CUnleashX::FrameMove), GetText() returns "" when it's set, and the
// settings pages only use the text when it's 0, so it looks like
// "cancelled" rather than a mode.
extern int g_nKeyboardMode;                              // 0x00278460
extern int g_nKeyboardOwner;                             // 0x00278468 who opened it (settings line), 0 = none

// An argument that is "KBResult" becomes the keyboard's text (trimmed);
// "$KBResult$" inside it is replaced by that text.
void ApplyKeyboardResult(std::string &arg);

// DVD tray state from the SMC.
class CTrayMonitor
{
public:
	int GetTrayState();                                   // 0x000B329C 0x10 = open
	HRESULT CloseTray();                                  // 0x000B32AE
	HRESULT OpenTray();                                   // 0x000B32C8
};
extern CTrayMonitor g_trayMonitor;                       // 0x00278658
