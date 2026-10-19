// The full-screen tools CUnleashX switches to (CUnleashX::SetMode). Only the
// interface the app code uses so far; all TODO: decompile.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "XBApp.h"
#include "../ui/UpDown.h"

class CXBFontEx;
class CXBFont;
class CAppSettings;
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
	MODE_SYSINFO = 7,      // CXboxInfo (no action opens it)
	MODE_TEXTEDITOR = 8,
	MODE_MUSIC = 9,
};

// Base of the full-screen tools (the text editor, the music player...).
// The defaults below are the base class's own (empty) ones.
class CAppFrame
{
public:
	virtual ~CAppFrame() {}                                               // 0x000111EB
	virtual BOOL Init(CXBFontEx * /*font*/) { return TRUE; }              // 0x000111E1
	virtual BOOL IsOpen() { return FALSE; }                               // 0x00068291
	virtual BOOL Render() { return TRUE; }                                // 0x000111E7
	virtual HRESULT ProcessInput(int * /*action*/, SGamepadState ** /*pad*/) { return S_OK; }   // 0x000326FF
	virtual void Show() {}                                                // 0x00013D20
};

// The music player (mode 9). TODO: decompile
class CAudioGUI : public CAppFrame
{
public:
	CAudioGUI();                                          // 0x00134640
	virtual ~CAudioGUI();                                 // 0x00011502 (deleting)
	virtual BOOL Init(CXBFontEx *font);                   // 0x00134283
	virtual BOOL IsOpen();                                // 0x00130DD5
	virtual BOOL Render();                                // 0x00134853
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x00135A99
	virtual void Show();                                  // 0x00133A5D
};

// The screen saver (text, video or slideshow). TODO: decompile
class CScreenSaver
{
public:
	virtual ~CScreenSaver();
	// (this in ecx, action in eax)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x00139D04
	HRESULT Render();                                     // 0x001398F7 (this in ecx)
	// Neither the video nor the slideshow runs (only the text). (this in eax)
	BOOL IsTextOnly() const;                              // 0x0002B806
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
	void ProcessInput(int *action, SGamepadState **pad);  // 0x0012C91D TODO: decompile
	void Render();                                        // 0x00127052 TODO: decompile (this in eax)

	BYTE m_unknown004[0x8B0 - 0x4];
	BOOL m_bOpen;                                         // +0x8B0
	BYTE m_unknown8B4[0x8DC - 0x8B4];
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
	void ProcessInput(int *action, SGamepadState **pad);  // 0x0011E249 TODO: decompile
	void Render();                                        // 0x0011C8F4 TODO: decompile (this in eax)

	BYTE m_unknown04[0x74 - 0x4];
	BOOL m_bOpen;                                         // +0x74
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
	// (this in ecx, pad in eax)
	void ProcessInput(int *action, SGamepadState **pad);  // 0x000F67FD TODO: decompile
	void Render();                                        // 0x000F8685 TODO: decompile (this in esi)
	BOOL IsOpen() const { return m_bOpen && !m_bClosing; }   // 0x0002B7ED

	BYTE m_unknown04[0x54];
	std::string m_strSkinPath;                            // +0x58
	BYTE m_unknown74[0x90 - 0x74];
	BOOL m_bOpen;                                         // +0x90
	BOOL m_bClosing;                                      // +0x94
};

// The text editor (mode 8). Its vtable's Init slot is 0x00024830.
class CNotePad : public CAppFrame
{
public:
	CNotePad();                                           // 0x00103A0C? TODO: verify
	virtual ~CNotePad();                                  // 0x000248EB (deleting)
	virtual BOOL IsOpen();                                // 0x00024845
	virtual BOOL Render();                                // 0x0010496B
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x00105438
	using CAppFrame::Init;
	void Init(int, int, int);                             // 0x00104399
	void NewFile();                                       // 0x00103D4D
	HRESULT Open(const char *path);                       // 0x00104181
	// Loads a file for viewing (the file operation log). TODO: decompile
	BOOL Load(const char *path);                          // 0x00103B58
};

// The "SetClock" screen: six CUpDown spinners (the date in the language's
// order, then hour, minute, AM/PM), started from the current time. Also the
// date/time pop-up of the settings pages. 0x2A4 bytes.
class CClockEditor
{
public:
	CClockEditor();                                       // 0x000C66A7
	~CClockEditor();                                      // 0x000C147B (this in eax)
	// E_FAIL when already initialized.
	HRESULT Init(CXBFontEx *font);                        // 0x000C1FAC
	// Opens it showing the current time, and lays it out.
	void Show();                                          // 0x000C0B13
	// Draws even when closed. (this in edi)
	HRESULT Render();                                     // 0x000BFC70
	// A sets the clock. (this in eax)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x000C1102

	enum
	{
		SPINNER_HOUR = 3,
		SPINNER_MINUTE = 4,
		SPINNER_AMPM = 5,
	};

	CXBFont *m_pFont;                                     // +0x00 MainFont, else Init's font
	CXBFont *m_pButtonFont;                               // +0x04 XboxDings
	BOOL m_bInitialized;                                  // +0x08
	BOOL m_bOpen;                                         // +0x0C
	CUpDown m_spinners[6];                                // +0x10
	int m_nMonthSpinner;                                  // +0x208 which of 0-2 has the month,
	int m_nDaySpinner;                                    // +0x20C the day
	int m_nYearSpinner;                                   // +0x210 and the year
	wchar_t m_szDateSeparator[2];                         // +0x214 "/" or "."
	// The x of spinner 0, separator 0, spinner 1, ... separator 4, spinner 5.
	float m_fPositions[11];                               // +0x218
	int m_nFocus;                                         // +0x244
	CSound *m_pSoundMove;                                 // +0x248 "Scroll"
	CSound *m_pSoundAccept;                               // +0x24C "Select"
	CSound *m_pSoundBack;                                 // +0x250 "Back"
	wchar_t m_szTime[30];                                 // +0x254 "January 1, 2004  12:00 AM"
	float m_fY;                                           // +0x290
	float m_fX;                                           // +0x294
	float m_fWidth;                                       // +0x298
	float m_fHeight;                                      // +0x29C
	DWORD m_unknown2A0;                                   // +0x2A0 unused
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

// A line of message box text, or a button. 0x34 bytes.
struct SMessageLine
{
	SMessageLine(const std::wstring &text, float height, int unknown);   // 0x00030702

	std::wstring text;              // +0x00
	float height;                   // +0x1C of the text
	int unknown20;                  // +0x20 always 0
	int rect[4];                    // +0x24 a button's x, y, width, height (for the pointer), set when first drawn
};

// The message box (OK / Cancel / Abort / Retry / Ignore / Yes / No): a
// caption, the text word-wrapped to the width, and a row of buttons. It
// slides down into place when shown. No vtable; 0x158 bytes.
class CMessageBox
{
public:
	CMessageBox();                                        // 0x000B10D4 (this in esi)
	~CMessageBox();                                       // 0x000B07F9 (this in eax; 0x00016217 deletes)
	// width 0 = 360; maxLines 0 = 10 (only reserves room). The font is
	// passed in eax. The callers pass 0 for the rest and 10.
	HRESULT Create(CXBFont *font, const wchar_t *title, float x, float y, float width, float height,
	               int maxLines);                         // 0x000B084D
	// A button's caption (button: MSGBOX_OK...MSGBOX_NO); NULL or "" = the
	// English default. (button in eax)
	void SetButtonText(int button, const wchar_t *text);  // 0x000AFB3D
	// buttons: MSGBOX_BUTTONS_*. (buttons in eax, this in ecx)
	void Show(const char *text, const wchar_t *title, int buttons = 0);   // 0x000B0F74
	// Takes all input: Left/Right pick a button, A chooses, B/BACK cancel;
	// the pointer can pick and click. (this in eax)
	HRESULT ProcessInput(int *action);                    // 0x000AF925
	BOOL IsOpen() const;                                  // 0x000AF8E9 (this in eax)
	int GetResult();                                      // 0x000AFA59 MSGBOX_* (this in eax)
	void SetSelection(int button);                        // 0x000AF8CF (this in esi, button in edi)
	HRESULT Render();                                     // 0x000AFC64
	// Word-wraps text into m_lines, widening the box for long words.
	void SetText(const char *text);                       // 0x000B09A5
	// Unused.
	void AddLine(const wchar_t *text);                    // 0x000B0E87 (this in eax)
	void SetLineText(int line, const wchar_t *text);      // 0x000B076A -1 = the last (line in eax, this in edi)

	int m_nFirstLine;                                     // +0x000
	int m_nLastLine;                                      // +0x004
	int m_nVisibleLines;                                  // +0x008 found by Render
	int m_nButtons;                                       // +0x00C MSGBOX_BUTTONS_*
	float m_fButtonWidth;                                 // +0x010 the widest caption; found by Render
	CAppSettings *m_pSettings;                            // +0x014
	std::wstring m_strTitle;                              // +0x018
	std::wstring m_buttonText[7];                         // +0x034 by MSGBOX_OK...MSGBOX_NO
	std::vector<SMessageLine> m_buttons;                  // +0x0F8 the buttons shown
	std::vector<SMessageLine> m_lines;                    // +0x108 the wrapped text
	int m_nSelected;                                      // +0x118 the button
	BOOL m_unknown11C;                                    // +0x11C 1
	BOOL m_bCancelled;                                    // +0x120
	BOOL m_bChosen;                                       // +0x124
	int m_unknown128;                                     // +0x128
	CXBFont *m_pFont;                                     // +0x12C
	CXBFont *m_pTitleFont;                                // +0x130 CAppSettings::m_pDialogFont, else m_pFont
	BOOL m_bSliding;                                      // +0x134 sliding into place
	DWORD m_dwSlideTime;                                  // +0x138
	float m_fY;                                           // +0x13C 0 = centered
	float m_fX;                                           // +0x140 0 = centered
	float m_fWidth;                                       // +0x144
	float m_fHeight;                                      // +0x148 0 = fit the text
	float m_fPadding;                                     // +0x14C 10, scaled
	float m_fLineSpacing;                                 // +0x150 1
	float m_fLineHeight;                                  // +0x154

private:
	void SetTitle(const wchar_t *title);                  // 0x000B07DE (this in eax)
	// Opens it; clear also empties the text and buttons and the position.
	// (this in edi)
	void Reset(BOOL clear);                               // 0x000AFBFD
	void AddButton(const wchar_t *text);                  // 0x000B0F05 (this in edi)
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
	// Turns the Xbox off (unless the SMC refuses). TODO: decompile
	HRESULT PowerOff();                                   // 0x000B3282
};
extern CTrayMonitor g_trayMonitor;                       // 0x00278658
