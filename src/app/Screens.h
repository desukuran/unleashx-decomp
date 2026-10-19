// The full-screen tools CUnleashX switches to (CUnleashX::SetMode). Only the
// interface the app code uses so far; all TODO: decompile.
#pragma once

#include "../thirdparty/IoSupport.h"

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "XBApp.h"
#include "../ui/UpDown.h"
#include "../system/FileOps.h"
#include "../ui/FileBrowser.h"
#include "../ui/MenuDialogs.h"
#include "../ui/VirtualKeyboard.h"
#include "GameManager.h"

class CXBFontEx;
class CXBFont;
class CAppSettings;
class CSound;
class CMessageBox;
class CInternetSession;
class CXmlDocument;
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
	MODE_SYSINFO = 7,      // CXboxInfo (Y or the remote's INFO in the menu)
	MODE_TEXTEDITOR = 8,
	MODE_MUSIC = 9,
	MODE_REBOOT = 10,      // reboot when the message box closes (after an update)
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

// The music player (mode 9): CAudioGUI, src/musicplayer/AudioGUI.h.

// A slideshow of a picture folder (and its subfolders), in random order:
// the screen saver's and the file manager's. A thread lists the pictures;
// another loads each next one, which then fades in with one of a dozen
// transitions. 0xD0 bytes.
class CSlideShow : public CAppFrame
{
public:
	explicit CSlideShow(const char *path);               // 0x000F5C9D (this in ebx)
	virtual ~CSlideShow();                                // 0x000F5602
	virtual BOOL IsOpen() { return m_bOpen; }             // 0x00029095
	virtual BOOL Render();                                // 0x000F4F6C
	// A/START pause, B/BACK close, Y switches the transitions, up/down
	// change the delay, left/right go back/forward. (Sleeps 10-50 ms.)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x000F56B7
	// Starts over (restarts the picture timer).
	virtual void Show();                                  // 0x000F4DF1
	// Ends the loader thread and frees the pictures. (this in esi)
	void Stop();                                          // 0x000F4E50
	// Seconds per picture (milliseconds over 1000 are converted; 2 or
	// less is ignored). (seconds in eax)
	void SetDelay(DWORD seconds);                         // 0x000F4DD5
	// FALSE: pictures just replace each other. (this in eax, value in ecx)
	void SetTransitions(BOOL transitions) { m_bTransitions = transitions; }   // 0x000F4DD1

	int m_nTransition;                     // +0x04 the effect of the picture fading in (0-11)
	std::string m_strPath;                 // +0x08 the folder
	IDirect3DTexture8 *m_pLoading;         // +0x24 the picture being loaded
	IDirect3DTexture8 *m_pTextures[2];     // +0x28 the current and the previous picture
	int m_nCurrent;                        // +0x30 which of the two is current
	UINT m_nIndex;                         // +0x34 in m_files
	BOOL m_bFading;                        // +0x38 the current one is fading in
	std::vector<SFileListItem> m_files;    // +0x3C shuffled
	float m_fX[2];                         // +0x4C per texture
	float m_fWidth[2];                     // +0x54
	float m_fY[2];                         // +0x5C
	float m_fHeight[2];                    // +0x64
	UINT m_nDelay;                         // +0x6C seconds, 5
	BOOL m_bTransitions;                   // +0x70
	int m_unknown74;                       // +0x74
	CXBStopWatch m_timer;                  // +0x78 the fade, then the delay
	HANDLE m_hLoader;                      // +0x98 -1 = none
	BOOL m_bReady;                         // +0x9C the list is done
	BOOL m_bStretch;                       // +0xA0 fill the screen (never set)
	float m_fFixedX;                       // +0xA4 a fixed place, if the size is set (never set)
	float m_fFixedY;                       // +0xA8
	float m_fFixedWidth;                   // +0xAC
	float m_fFixedHeight;                  // +0xB0
	int m_unknownB4[2];                    // +0xB4
	float m_fScale[2];                     // +0xBC the zoom transitions' x and y scale
	BOOL m_bOpen;                          // +0xC4
	BOOL m_bSkip;                          // +0xC8 load the next one now
	BOOL m_bPaused;                        // +0xCC

private:
	void Reset();                                         // 0x000F4EB4 (this in esi)
	// Starts loading the next picture. (this in edi)
	void LoadNext();                                      // 0x000F563B
	// Lists and shuffles the pictures, then Show(). (thread)
	static unsigned __stdcall ListThread(void *param);          // 0x000F5BA1
	// Loads the next picture that loads. (thread)
	static unsigned __stdcall LoaderThread(void *param);        // 0x000F527E
};
class CWMVideoPlayer;

// The screen saver: bouncing text, a video, or a slideshow (by what the
// source is). 0x90 bytes, no vtable.
class CScreenSaver
{
public:
	CScreenSaver();                                       // 0x00139E2C (this in edi)
	~CScreenSaver();                                      // 0x00139D64 (this in edi)
	// font NULL: "MainFont". (this on the stack)
	HRESULT Init(CXBFont *font);                          // 0x00139DA4
	// The text to show, a video file (.wmv/.xmv), a picture or a picture
	// folder. (this on the stack)
	void SetSource(const wchar_t *source);                // 0x00139EB2
	void Start();                                         // 0x0013A17A (this in edi)
	void Stop();                                          // 0x0013A26C (this in esi)
	// Any action ends it; otherwise the video or slideshow gets the input.
	// (this in ecx, action in eax)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x00139D04
	HRESULT Render();                                     // 0x001398F7 (this in ecx)
	// Neither the video nor the slideshow runs (only the text). (this in eax)
	BOOL IsTextOnly() const { return !m_bVideo && !m_bSlideShow; }   // 0x0002B806

private:
	void CloseMedia();                                    // 0x00139CE3 (this in eax)

	BOOL m_bActive;                    // +0x00
	BOOL m_bDimmed;                    // +0x04 the screen is fully dimmed
	BOOL m_bInitialized;               // +0x08
	float m_fFontScaleX;               // +0x0C the font's own scale, restored after drawing
	float m_fFontScaleY;               // +0x10
	BOOL m_bSounds;                    // +0x14 the bounce sound plays
	float m_fNextMove;                 // +0x18 milliseconds
	std::wstring m_strText;            // +0x1C the source
	CXBFont *m_pFont;                  // +0x38
	BYTE m_reserved3C[4];              // +0x3C
	CXBStopWatch m_timer;              // +0x40 since Start
	CSound *m_pBounceSound;            // +0x60 "Scroll"
	CSlideShow *m_pSlideShow;          // +0x64
	std::vector<int> m_unused68;       // +0x68
	BOOL m_bSlideShow;                 // +0x78
	CWMVideoPlayer *m_pVideo;          // +0x7C
	BOOL m_bVideo;                     // +0x80
	BOOL m_bMusicWasPlaying;           // +0x84
	CAppSettings *m_pSettings;         // +0x88
	BYTE m_reserved8C[4];              // +0x8C
};

// The file manager: src/filemgr/FileManager.h.
class CFileManager;

// The game saves manager (mode 3): CGameManager, GameManager.h.

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


// A skin offered by an online skin site (from its RSS feed). 0x2C8 bytes.
struct SOnlineSkin
{
	// (this in esi)
	SOnlineSkin(const char *title, const char *author, const char *link, const char *thumb);   // 0x0002825D

	BOOL selected;                  // +0x000 ticked for download
	std::string link;               // +0x004 the skin's zip
	std::string thumb;              // +0x020 its preview (.jpg only)
	std::string localThumb;         // +0x03C the preview once downloaded
	wchar_t title[0x105];           // +0x058
	wchar_t author[0x33];           // +0x262
};

// The skin chooser (mode 6, SkinBrowser.cpp): "Default", the built-in skins
// and the skin folders (with skin.xml or a zipped skin), with a preview;
// and the "Download Skin" list of an online skin site. No vtable; 0xD0
// bytes.
class CSkinBrowser
{
public:
	CSkinBrowser();                                       // 0x000F6597 (this in esi)
	~CSkinBrowser();                                      // 0x000F5D35 (this in edi)
	// checked/unchecked: the online list's tick boxes. E_FAIL without a
	// font. (checked in eax, unchecked in ecx, this in ebx)
	HRESULT Init(IDirect3DTexture8 *checked, IDirect3DTexture8 *unchecked, CXBFontEx *font);   // 0x000F5DEC
	// Lays it out and lists the skins again. (this in ebx)
	void Show();                                          // 0x000F6791
	// (this in ecx, pad in eax)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x000F67FD
	HRESULT Render();                                     // 0x000F8685 (this in esi)
	BOOL IsOpen() const { return m_bOpen && !m_bClosing; }   // 0x0002B7ED (this in eax)

	std::vector<SOnlineSkin> m_online;    // +0x00
	std::vector<SFileItem> m_skins;       // +0x10
	CInternetSession *m_pSession;         // +0x20 the feed, then each download
	CXmlDocument *m_pFeed;                // +0x24
	BOOL m_bOnline;                       // +0x28 showing the online list
	BOOL m_bDownloading;                  // +0x2C
	BOOL m_bPreviewOnly;                  // +0x30 downloading just a preview (BLACK)
	BOOL m_bCancel;                       // +0x34
	int m_nDownloadIndex;                 // +0x38 -1 before the first
	UINT m_nDownloaded;                   // +0x3C skins this time
	CMessageBox *m_pMessageBox;           // +0x40
	BOOL m_bMessageShown;                 // +0x44
	BOOL m_bInstalling;                   // +0x48 the install thread runs
	COptionMenu *m_pServerMenu;           // +0x4C "Select Server"
	BOOL m_bServerMenuOpen;               // +0x50
	CAppSettings *m_pSettings;            // +0x54
	std::string m_strSkinPath;            // +0x58 <Skin Path>
	float m_fY;                           // +0x74
	float m_fX;                           // +0x78
	float m_fWidth;                       // +0x7C
	float m_fHeight;                      // +0x80
	CXBFont *m_pFont;                     // +0x84
	CXBFont *m_pCaptionFont;              // +0x88 the settings' dialog font, else m_pFont
	CXBFont *m_pSymbolFont;               // +0x8C XboxDings
	BOOL m_bOpen;                         // +0x90
	BOOL m_bClosing;                      // +0x94
	BOOL m_bKeyboardOpen;                 // +0x98 (never set)
	CSound *m_pSoundMove;                 // +0x9C "Scroll"
	int m_nSelected;                      // +0xA0 -1: the current skin's
	int m_nTop;                           // +0xA4
	int m_nBottom;                        // +0xA8
	int m_nVisible;                       // +0xAC 0 until the first Render
	BOOL m_bSkinChanged;                  // +0xB0 START saves it to Config.xml
	IDirect3DTexture8 *m_pPreview;        // +0xB4 320x240
	IDirect3DTexture8 *m_pChecked;        // +0xB8
	IDirect3DTexture8 *m_pUnchecked;      // +0xBC
	IDirect3DTexture8 *m_pServerLogo;     // +0xC0
	float m_fCheckSize;                   // +0xC4 the tick box's height
	BOOL m_bListed;                       // +0xC8 the list thread is done
	DWORD m_dwListTick;                   // +0xCC when it was started

private:
	// Lists the skins. (thread; this on the stack)
	static unsigned __stdcall ListThread(void *param);    // 0x000F66EC
	HRESULT ListSkins();                                  // 0x000F6098
	// Starts the list thread. (this in esi)
	void StartList();                                     // 0x000F673A
	void ShowServerMenu();                                // 0x000F5EE9 (this in ebx)
	// (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x000F5F78
	// The online list, downloads and installs. (this on the stack)
	HRESULT RenderOnline();                               // 0x000F76A4
	// Loads the highlighted skin's preview (when previews are on).
	void UpdatePreview();                                 // (inlined in ProcessInput)
	void ScrollToSelection();                             // (inlined in ProcessInput)
};

class CMessageBox;
class CInternetSession;
class CXmlDocument;
struct SKeystroke;

// The text editor (mode 8, NotePad.cpp): a window with the file name, the
// text (one std::wstring per line), a "Ln/Col" and size status line and an
// embedded on-screen keyboard. START opens the menu (New, Open, Save, Save
// As, Exit); file names are typed on the global keyboard. 0x328 bytes.
class CNotePad : public CAppFrame
{
public:
	CNotePad();                                           // 0x00104605 (this in ebx)
	virtual ~CNotePad();                                  // 0x00103FB6 (deleting 0x000248EB)
	// Init(font, NULL, NULL, NULL).
	virtual BOOL Init(CXBFontEx *font);                   // 0x00024830
	virtual BOOL IsOpen() { return m_bActive; }           // 0x00024845
	virtual BOOL Render();                                // 0x0010496B
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x00105438

	// Sets up the fonts (the dialog font is the settings' one, else font),
	// the keyboard (its key texture, else "Keyboard" from resource or the
	// app's own .xpr; the IR remote if given) and the START menu. Only the
	// first call does anything but reopening it. (font in eax, this in esi)
	HRESULT Init(CXBFont *font, IDirect3DTexture8 *keys, CXBPackedResource *resource, SIRRemote *ir);   // 0x00104399
	// An empty document, "New Text Document". (this in esi)
	void NewFile();                                       // 0x00103D4D
	// Loads a text file. Only fails for an empty path. (this on the stack)
	BOOL Open(const char *path);                          // 0x00104181
	// Shows text that isn't a file (the file operation log). FALSE if
	// it's empty. (this on the stack)
	BOOL Load(const char *text);                          // 0x00103B58

	// What a message box or the file name keyboard answers (m_nPending).
	enum
	{
		PENDING_NONE = 1,
		PENDING_NEW = 2,             // "save changes?" before New
		PENDING_OPEN = 3,            // ... before Open
		PENDING_SAVE = 4,            // (never set)
		PENDING_SAVE_AS = 5,         // a file name to save as
		PENDING_SAVE_EXIT = 6,       // a file name, then exit
		PENDING_SAVE_OPEN = 7,       // a file name, then Open
		PENDING_SAVE_NEW = 8,        // a file name, then New
		PENDING_EXIT = 9,            // "save changes?" before exiting
	};

	// The embedded keyboard's keys. (RTTI CNotePad::KeyboardCharEvent)
	class KeyboardCharEvent : public CVirtualKeyboardEvent
	{
	public:
		virtual void OnChar(wchar_t ch);                  // 0x001038AE
		virtual void OnDelete();                          // 0x001037E7
		virtual void OnBackspace();                       // 0x00103950
		virtual void OnKey(int virtualKey);               // 0x0010473E

		CNotePad *m_pOwner;                               // +0x04
	};

	UINT m_nCol;                           // +0x004 the cursor
	std::vector<std::wstring> m_lines;     // +0x008
	UINT m_nLine;                          // +0x018 the cursor
	UINT m_nColOffset;                     // +0x01C only ever reset to 0
	BOOL m_bKeyboardOpen;                  // +0x020 typing a file name (g_virtualKeyboard)
	int m_unknown24;                       // +0x024
	CVirtualKeyboard m_keyboard;           // +0x028 embedded
	int m_unknown174;                      // +0x174
	BOOL m_bShowKeyboard;                  // +0x178 WHITE toggles it
	float m_fY;                            // +0x17C the window
	float m_fX;                            // +0x180
	float m_fWidth;                        // +0x184
	float m_fHeight;                       // +0x188
	BOOL m_bModified;                      // +0x18C
	std::string m_strPath;                 // +0x190 "" until saved
	UINT m_nChars;                         // +0x1AC the size: the characters plus 2 per line break
	UINT m_nLastLine;                      // +0x1B0 the lines - 1
	UINT m_nTopLine;                       // +0x1B4
	UINT m_nVisibleLines;                  // +0x1B8 19 until the first Render
	COptionMenu m_menu;                    // +0x1BC START
	CMessageBox *m_pMessageBox;            // +0x250
	BOOL m_bMessageShown;                  // +0x254
	BOOL m_bMenuOpen;                      // +0x258
	BOOL m_bKeyboardFocus;                 // +0x25C the right stick's click: everything goes to the keyboard
	CXBFont *m_pFont;                      // +0x260 the text
	CXBFont *m_pDialogFont;                // +0x264 the title
	BOOL m_bActive;                        // +0x268 FALSE = closed
	float m_fKeyboardHeight;               // +0x26C 0 when hidden
	CXBStopWatch m_cursorTimer;            // +0x270 the cursor's blink
	wchar_t m_szTitle[0x2C];               // +0x290 the file name
	CXBStopWatch m_stickTimer;             // +0x2E8 the left stick's repeat (keyboard shown)
	float m_unknown308;                    // +0x308
	int m_nPending;                        // +0x30C PENDING_*
	CFileBrowser *m_pFileBrowser;          // +0x310 "Open Text File"
	BOOL m_bFileBrowserOpen;               // +0x314
	SKeystroke *m_pKeystroke;              // +0x318 the debug keyboard's
	KeyboardCharEvent m_keyEvents;         // +0x31C
	CAppSettings *m_pSettings;             // +0x324

private:
	// Clears the text and the cursor. (this in edi)
	void Reset();                                         // 0x00103910
	// The 640x480 window, scaled to the screen. (this in eax)
	void ResetWindow();                                   // 0x0002484C
	// m_nChars and m_nLastLine from the lines. (this in esi)
	void Recount();                                       // 0x001036F6
	// Puts the cursor back on the line. (this in eax)
	void ClampColumn();                                   // 0x001036D5
	void MoveUp();                                        // 0x00103741 (this in eax)
	void MoveLeft();                                      // 0x00103765 (this in eax)
	void MoveDown();                                      // 0x001037AD (this in eax)
	void MoveRight();                                     // 0x00103867 (this in eax)
	// Enter: splits the line at the cursor.
	void SplitLine();                                     // 0x00103A4F (this on the stack)
	// The text as narrow text (each line through "%S"), CRLF between
	// lines. (out in ebx, this in edi)
	void GetText(std::string *out);                       // 0x00103F23
	// (path in ecx, this on the stack)
	BOOL Save(const char *path);                          // 0x0010403F
	// Opens g_virtualKeyboard for a file name: path, else the first free
	// C:\FileNNNN.txt. (this on the stack)
	void AskFileName(const wchar_t *title, const char *path);   // 0x00103E0D
	void ShowOpenBrowser();                               // 0x0010538A (this in edi)
	// (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x001044BE
	void ShowSaveError(const char *path);                 // (inlined)
	std::wstring *CurrentLine() { return m_lines.empty() ? NULL : &m_lines[m_nLine]; }
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
	// The button chosen with A (not a cancel), else -1. (this in esi)
	int GetChosenButton();                                // 0x000AF902
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

// The DVD tray and power: CIoSupport (src/thirdparty/IoSupport.h).
