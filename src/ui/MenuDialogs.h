// Dialogs the menu opens. TODO: decompile
#pragma once

#include <string>

#include "../app/XBApp.h"
#include "../common/Platform.h"
#include "../system/SystemInfo.h"

class CMenuEventSink;
class CSound;
class CMenuItem;
class CXBFont;

// A pop-up list of options (the recent list, context menus). 0x94 bytes.
class COptionMenu
{
public:
	COptionMenu();                                        // 0x000AEADE
	virtual ~COptionMenu();
	virtual HRESULT Create(CXBFont *font, const wchar_t *title, float x, float y, float width,
	                       float height, int maxVisible);  // vtable slot 1 (0x000AE9AD)
	void AddOption(const wchar_t *text, int value);       // 0x000AEA79
	void Show(int selected);                              // 0x000ADE9E
	virtual void ProcessInput(int *action, struct SGamepadState **pad);   // vtable slot 2 (0x000AE6F1)
	virtual void Render();                                // vtable slot 3 (0x000ADF2C)
	BOOL IsOpen() const;                                  // 0x000AD930
	// Greys out (or enables) the entry at index. (index in eax, this in edi)
	void SetOptionEnabled(int index, BOOL enabled);       // 0x000AD997
	int GetSelection() const;                             // 0x000AD946

	BYTE m_unknown04[0x54];
	int m_nSelection;                                     // +0x58
	BYTE m_unknown5C[0x20];
	BOOL m_bCancelled;                                    // +0x7C
	BOOL m_bChosen;                                       // +0x80
	BYTE m_unknown84[0x10];                               // (0x94 bytes in all)
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

// A small pop-up menu (the START menu of an item). 0x78 bytes.
class CDropDown
{
public:
	CDropDown();                                          // 0x000D8404
	virtual ~CDropDown();
	void AddItem(const wchar_t *text);                    // 0x000D8169
	void Create(CXBFont *font, float x, float y);         // 0x000D80D2
	void SetEnabled(int index, BOOL enabled);             // 0x00021F97
	void Show(float x, float y);                          // 0x000D6F8A
	void ProcessInput(int *action);                       // 0x000D7539
	BOOL IsOpen() const { return !m_bCancelled && !m_bChosen; }   // 0x00013584
	void Render();                                        // 0x000D70A7
	int GetCount() const;                                 // 0x000193A5 (the items vector at +0x14)

	BYTE m_unknown04[0x20];
	int m_nSelection;                                     // +0x24 -1 = none
	BYTE m_unknown28[0x18];
	BOOL m_bCancelled;                                    // +0x40
	BOOL m_bChosen;                                       // +0x44
};

// Edits a menu item's text, icon, action... 0x298 bytes.
class CMenuItemEditor
{
public:
	CMenuItemEditor();                                    // 0x0010A4E5
	virtual ~CMenuItemEditor();
	void Init(CXBFont *font, int unknown);                // 0x0010A05B
	void Edit(CMenuItem *item);                           // 0x0010986F
	virtual void ProcessInput(int *action, struct SGamepadState **pad);   // vtable slot 2
	virtual BOOL IsOpen();                                // vtable slot 5

	BYTE m_unknown004[0x290];
	CMenuEventSink *m_pSink;                              // +0x294
};
