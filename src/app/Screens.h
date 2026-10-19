// The full-screen tools CUnleashX switches to (CUnleashX::SetMode). Only the
// interface the app code uses so far; all TODO: decompile.
#pragma once

#include <string>

#include "../common/Platform.h"
#include "XBApp.h"
#include "../ui/UpDown.h"

class CXBFontEx;
class CXBFont;
class CSound;
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

// The Control Panel: ten icons, each opening a settings page
// (SettingsPages.h). 0xA0 bytes, no vtable. See SettingsMenu.cpp.
class CSettingsMenu
{
public:
	enum { PAGE_COUNT = 10, NO_PAGE = 10 };

	CSettingsMenu();                                      // 0x000BEBFD (this in edx)
	~CSettingsMenu();                                     // 0x000BEBCF (this in eax)
	// Loads CtlPanel.xpr. (this in eax)
	HRESULT Init(CXBFont *font);                          // 0x000C18A3
	// Lays the panel out again and opens it. (this in ebx)
	void Show();                                          // 0x0002B530
	// Always returns 1. (this in ecx, action in edx, pad in eax)
	int HandleInput(int *action, SGamepadState **pad);    // 0x000C62EE
	// 1 when closed. (this in ebx)
	HRESULT Render();                                     // 0x000C5E8A

	CXBPackedResource m_resource;                         // +0x00 CtlPanel.xpr
	IDirect3DTexture8 *m_pIcons[PAGE_COUNT];              // +0x18
	class CSettingsBase *m_pPages[PAGE_COUNT];            // +0x40 created on first use
	IDirect3DTexture8 *m_pSelectedIcon;                   // +0x68 drawn faintly behind the list
	int m_nSelected;                                      // +0x6C
	int m_nFirstVisible;                                  // +0x70
	int m_nVisibleCount;                                  // +0x74
	int m_nLastVisible;                                   // +0x78
	float m_fX;                                           // +0x7C
	float m_fY;                                           // +0x80
	float m_fWidth;                                       // +0x84
	float m_fHeight;                                      // +0x88
	CXBFont *m_pFont;                                     // +0x8C
	CSound *m_pSoundMove;                                 // +0x90 "Scroll"
	CSound *m_pSoundSelect;                               // +0x94 "Select"
	BOOL m_bClosed;                                       // +0x98
	int m_nOpenPage;                                      // +0x9C NO_PAGE when none
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
	~CIPAddressEditor();                                  // 0x000C0AB6 (this in eax)
	// E_FAIL when already initialized. (this in eax)
	HRESULT Init(CXBFont *font);                          // 0x000C1D45
	// Opens it showing m_strAddress, and lays it out. (this in ebx)
	void Show();                                          // 0x000C066E
	HRESULT Render();                                     // 0x000BF86F (this in edi)
	// The original takes this in ecx and address in eax.
	void SetAddress(const wchar_t *address);              // 0x000C1A55
	// "a.b.c.d" from the spinners. The original takes this in esi.
	void GetAddress(wchar_t *address);                    // 0x000BFC30
	// A accepts, B/BACK cancels (and resets the spinners). The original
	// takes this in eax and action in edi.
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x000C09EE

	CXBFont *m_pFont;                                     // +0x00 MainFont, else Init's font
	CXBFont *m_pButtonFont;                               // +0x04 XboxDings (the A/B glyphs)
	BOOL m_bInitialized;                                  // +0x08
	BOOL m_bOpen;                                         // +0x0C
	CUpDown m_spinners[4];                                // +0x10
	int m_nFocus;                                         // +0x160
	CSound *m_pSoundMove;                                 // +0x164 "Scroll"
	CSound *m_pSoundAccept;                               // +0x168 "Select"
	CSound *m_pSoundBack;                                 // +0x16C "Back"
	std::wstring m_strTitle;                              // +0x170
	std::string m_strAddress;                             // +0x18C
	BOOL m_bCancelled;                                    // +0x1AC
	float m_fY;                                           // +0x1B0
	float m_fX;                                           // +0x1B4
	float m_fWidth;                                       // +0x1B8
	float m_fHeight;                                      // +0x1BC
	DWORD m_unknown1C0;                                   // +0x1C0 unused
};

// The settings pages' file pop-up ("Browse XBE"). 0x180 bytes. Most
// methods take this in a register (noted).
class CFileBrowser
{
public:
	CFileBrowser();                                       // 0x00127A32
	~CFileBrowser();                                      // 0x00127013
	void Create(CXBFont *font, const wchar_t *title);     // 0x0012C378
	void Render();                                        // 0x00129B10
	void SetTitle(const wchar_t *title);                  // 0x000113D9 (this in eax, title in esi)
	void SetOption11C(int value);                         // 0x000112BA (this in esi) TODO: what it is
	// Only files with these extensions; NULL clears the list. (this in eax)
	void AddFilter(const char *extension);                // 0x001281F6
	void SetStartPath(const char *path);                  // 0x0001142F (this in ebx, path in esi)
	void Show(BOOL unknown);                              // 0x0001124E (this in ebx)
	void ProcessInput(int *action, SGamepadState **pad);  // 0x0012AF3F (this in ebx)
	BOOL IsOpen();                                        // 0x0001123B (this in eax)
	// The chosen file, "" if cancelled. (this in eax)
	std::string GetSelectedPath();                        // 0x000112D9

	BYTE m_unknown00[0x48];
	BOOL m_bOpen;                                         // +0x48
	BOOL m_bCancelled;                                    // +0x4C
	BYTE m_unknown50[0x120 - 0x50];
	int m_unknown120;                                     // +0x120
	int m_unknown124;                                     // +0x124
	wchar_t m_szTitle[40];                                // +0x128
	BYTE m_unknown178[0x180 - 0x178];
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
