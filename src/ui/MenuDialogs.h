// Dialogs the menu opens: the option menu (OptionMenu.cpp), the drop-down
// menu (DropDown.cpp) and the passcode pad (PassCode.cpp).
#pragma once

#include <string>
#include <vector>

#include "../app/XBApp.h"
#include "../common/Platform.h"
#include "../system/SystemInfo.h"

class CAppSettings;
class CMenuEventSink;
class CSound;
class CMenuItem;
class CXBFont;

// A pop-up list of options in a dialog window: a caption bar with the
// title and a scrolling list (the recent list, the file managers' menus,
// the music player's menus). It slides down into place when shown. Up and
// down move over the disabled options; A (or a pointer click) chooses, B
// or BACK cancels. See OptionMenu.cpp. 0x94 bytes.
class COptionMenu
{
public:
	// wrap: up/down wrap around at the ends. Everything passes TRUE except
	// the recent list and the skin server list. (this in esi, wrap in ecx)
	COptionMenu(BOOL wrap = TRUE);                        // 0x000AEADE
	virtual ~COptionMenu();                               // 0x000ADE12 (deleting 0x00030F45)
	// The font and the window; a zero position centers it and a zero size
	// fits the options (on the first Render). The first call sets it up,
	// later ones only change the title. maxVisible reserves room for that
	// many options (0 = 10). E_FAIL without a font.
	virtual HRESULT Create(CXBFont *font, const wchar_t *title, float x, float y, float width,
	                       float height, int maxVisible);  // slot 1, 0x000AE9AD
	// An option, enabled unless text is empty. (this in edi)
	HRESULT AddOption(const wchar_t *text, int value);    // 0x000AEA79
	// Opens the menu (sliding in) on the first enabled option, or (clear)
	// empty for new options. (this in edi)
	void Show(BOOL clear);                                // 0x000ADE9E
	// (The original returns S_OK.)
	virtual void ProcessInput(int *action, struct SGamepadState **pad);   // slot 2, 0x000AE6F1
	// (The original returns E_FAIL, and closes the menu, when it's empty.)
	virtual void Render();                                // slot 3, 0x000ADF2C
	BOOL IsOpen() const;                                  // 0x000AD930 (this in eax)
	// Greys out (or enables) the option at index. (index in eax, this in edi)
	void SetOptionEnabled(int index, BOOL enabled);       // 0x000AD997
	// The selected option's index, -1 when cancelled or empty. (this in esi)
	int GetSelection() const;                             // 0x000AD946
	// The selected option's value (AddOption's), -1 when cancelled or
	// empty. (this in edi)
	int GetSelectedValue() const;                         // 0x000AD960
	// Removes the options (an inlined vector clear; the count at +0x84
	// stays). (options in esi)
	void ClearOptions() { m_options.clear(); }            // 0x00029AAD

	// One option. 0x34 bytes.
	struct SOption
	{
		SOption(const std::wstring &text, BOOL enabled, int value);   // 0x00030F11 (this in eax)

		std::wstring text;                                // +0x00
		BOOL enabled;                                     // +0x1C
		int value;                                        // +0x20
		int unknown24[3];                                 // +0x24 never set
		int unknown30;                                    // +0x30 cleared
	};

	int m_nFirst;                                         // +0x04 the first option shown
	int m_nLast;                                          // +0x08 the last one shown
	int m_nPageSize;                                      // +0x0C options that fit, 0 = not known yet
	int m_listRect[4];                                    // +0x10 x, y, width, height of the list (pointer clicks)
	DWORD m_dwSlideTime;                                  // +0x20 the last slide step
	BOOL m_bWrap;                                         // +0x24
	CAppSettings *m_pSettings;                            // +0x28
	std::wstring m_strTitle;                              // +0x2C
	std::vector<SOption> m_options;                       // +0x48
	int m_nSelection;                                     // +0x58
	int m_unknown5C;                                      // +0x5C 1, never read
	float m_fY;                                           // +0x60
	float m_fX;                                           // +0x64
	float m_fWidth;                                       // +0x68
	float m_fHeight;                                      // +0x6C
	float m_fBorder;                                      // +0x70 10
	float m_fLineSpacing;                                 // +0x74 1
	float m_fLineHeight;                                  // +0x78 the font's
	BOOL m_bCancelled;                                    // +0x7C
	BOOL m_bChosen;                                       // +0x80
	int m_nOptionCount;                                   // +0x84 AddOption's count (only Show(TRUE) resets it)
	CXBFont *m_pFont;                                     // +0x88
	CXBFont *m_pTitleFont;                                // +0x8C the dialog font, else m_pFont
	BOOL m_bSlideIn;                                      // +0x90 Show sets it, the slide's end clears it

private:
	// The selected option is disabled or has no text. (this in ecx)
	BOOL IsSelectionDisabled() const;                     // 0x000ADE61
};

// The passcode pad: the code is typed with gamepad buttons (A B X Y WHITE
// and the d-pad as letters, BLACK deletes, the triggers move the cursor)
// or the remote (arrows and digits); START or the remote's SELECT accepts,
// BACK cancels. Up to 16 presses, shown as stars. See PassCode.cpp.
// 0x120 bytes.
class CPassCode
{
public:
	CPassCode();                                          // 0x0010302B (this in edi)
	// vtable: ProcessInput, Render, then the (deleting) destructor.
	// (pad is the CGamepadInput)
	virtual HRESULT ProcessInput(int *action, struct SGamepadState **pad);   // slot 0, 0x00102A03
	virtual HRESULT Render();                             // slot 1, 0x0010317B
	virtual ~CPassCode();                                 // slot 2, 0x00026522 (body 0x000264E2)
	// (this in esi)
	void SetTitle(const wchar_t *title);                  // 0x0002209E
	// symbols: the button-glyph font, NULL for XboxDings. Fails without a
	// font or when already set up; shows the pad either way. (this in esi,
	// font in eax, symbols in ecx)
	HRESULT Init(CXBFont *font, CXBFont *symbols);       // 0x0010341C
	// The code to expect: upper-cased, at most 15 characters. (this in esi)
	void SetCode(const char *code);                       // 0x00103100
	// Places the pad and starts a new entry. (this on the stack)
	void Show();                                          // 0x00102805
	BOOL IsClosed() const { return m_bClosed || m_bCancelled; }   // 0x0002208B (this in eax)
	// The entry is the code (or its MD5). (this in ebx)
	BOOL IsCorrect();                                     // 0x00021FCE

	int m_unknown04;                       // +0x004
	CXBFont *m_pFont;                      // +0x008
	CXBFont *m_pSymbols;                   // +0x00C button glyphs (XboxDings)
	std::string m_strCode;                 // +0x010
	int m_unknown2C;                       // +0x02C
	std::string m_strEntered;              // +0x030 the entry as narrow text (IsCorrect)
	BOOL m_bCancelled;                     // +0x04C
	BOOL m_bClosed;                        // +0x050 accepted or cancelled
	float m_fY;                            // +0x054
	float m_fX;                            // +0x058
	float m_fWidth;                        // +0x05C
	float m_fHeight;                       // +0x060
	float m_fBoxHeight;                    // +0x064 the entry field
	float m_fTextWidth;                    // +0x068
	int m_unknown6C;                       // +0x06C
	float m_fTitleHeight;                  // +0x070
	std::wstring m_strInput;               // +0x074
	UINT m_nCursor;                        // +0x090
	CSound *m_pClickSound;                 // +0x094
	BOOL m_bClickSound;                    // +0x098
	int m_unknown9C;                       // +0x09C
	CXBStopWatch m_cursorTimer;            // +0x0A0
	std::wstring m_strTitle;               // +0x0C0 "Passcode"
	float m_fLineHeight;                   // +0x0DC
	float m_fScaleX;                       // +0x0E0
	float m_fScaleY;                       // +0x0E4
	CMD5 m_md5;                            // +0x0E8

private:
	void PlayClick();                                     // 0x00102757 (this in eax)
	void DrawTextBox();                                   // 0x00102765 (this in eax)
	void DrawFrame();                                     // 0x001027B8 (this in eax)
	void DrawText(float x, float y);                      // 0x00102C48 (this on the stack)
};

// A small pop-up menu without a frame (an item's START menu, the file
// manager's sort menu, the FTP queue's menu). Disabled items are hidden.
// It unrolls downwards when shown. See DropDown.cpp. 0x78 bytes.
class CDropDown
{
public:
	CDropDown();                                          // 0x000D8404 (this in esi)
	virtual ~CDropDown();                                 // 0x000D8032 (deleting 0x0002DBA1)
	// An enabled item; E_FAIL for an empty text.
	HRESULT AddItem(const wchar_t *text);                 // 0x000D8169
	// The font (NULL = SmallFont), the position and the screen limits.
	// Does nothing when m_bCreated is set (nothing sets it). (this in esi,
	// font in eax)
	HRESULT Create(CXBFont *font, float x, float y);      // 0x000D80D2
	// Shows (enabled) or hides an item; the size is measured again.
	// (this in esi, index in edi, enabled in ebx)
	void SetEnabled(int index, BOOL enabled);             // 0x00021F97
	// Opens the menu at x, y (kept on the screen). (this in ebx)
	HRESULT Show(float x, float y);                       // 0x000D6F8A
	// (this in ecx, pad in eax)
	HRESULT ProcessInput(int *action, struct SGamepadState **pad);   // 0x000D7539
	BOOL IsOpen() const { return !m_bCancelled && !m_bChosen; }   // 0x00013584 (this in eax)
	// (this in eax)
	HRESULT Render();                                     // 0x000D70A7
	int GetCount() const { return (int)m_items.size(); }  // 0x000193A5 (the items in eax)

	// One item. 0x20 bytes.
	struct SItem
	{
		SItem(const std::wstring &text, BOOL enabled);    // 0x0002DB5C (this in eax)

		std::wstring text;                                // +0x00
		BOOL enabled;                                     // +0x1C
	};

	CSound *m_pSound;                                     // +0x04 "Scroll"
	CXBFont *m_pFont;                                     // +0x08
	BOOL m_bCreated;                                      // +0x0C never set
	BOOL m_bShown;                                        // +0x10 Show has laid out the page
	std::vector<SItem> m_items;                           // +0x14
	int m_nSelection;                                     // +0x24 -1 = none
	int m_nLastMove;                                      // +0x28 1 down, -1 up, 0 after 2s idle (never read)
	float m_fMaxX;                                        // +0x2C the screen's width - 10
	float m_fMaxY;                                        // +0x30 the screen's height - 10
	UINT m_nFirst;                                        // +0x34 the first item shown
	UINT m_nLast;                                         // +0x38 the last one shown
	UINT m_nPageSize;                                     // +0x3C items that fit
	BOOL m_bCancelled;                                    // +0x40
	BOOL m_bChosen;                                       // +0x44
	BOOL m_bSlideIn;                                      // +0x48
	int m_listRect[4];                                    // +0x4C x, y, width, y + height (pointer clicks)
	CAppSettings *m_pSettings;                            // +0x5C
	float m_fY;                                           // +0x60
	float m_fX;                                           // +0x64
	float m_fWidth;                                       // +0x68 0 = measure again
	float m_fHeight;                                      // +0x6C 0 = measure again
	float m_fMarginX;                                     // +0x70 10, scaled by every Show (never read)
	float m_fMarginY;                                     // +0x74

private:
	// The selected item is hidden. (this in edi)
	BOOL IsSelectionDisabled() const;                     // 0x000D7072
};

// The item editor (CMenuItemEditor) is in src/app/MenuItemEditor.h.
