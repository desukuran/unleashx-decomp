// Dialogs the menu opens. TODO: decompile
#pragma once

#include <string>

#include "../common/Platform.h"

class CMenuEventSink;
class CMenuItem;
class CXBFont;

// A pop-up list of options (the recent list, context menus). 0x94 bytes.
class COptionMenu
{
public:
	COptionMenu();                                        // 0x000AEADE
	virtual ~COptionMenu();
	virtual HRESULT Create(CXBFont *font, const wchar_t *title, float x, float y, float width,
	                       float height, int maxVisible);  // vtable slot 1
	void AddOption(const wchar_t *text, int value);       // 0x000AEA79
	void Show(int selected);                              // 0x000ADE9E
	virtual void ProcessInput(int *action, const struct SGamepadState *pad);   // vtable slot 2
	BOOL IsOpen() const;                                  // 0x000AD930
	int GetSelection() const;                             // 0x000AD946

	BYTE m_unknown04[0x7C];
	BOOL m_bChosen;                                       // +0x80
};

// The passcode pad (eight button presses). 0x120 bytes.
class CPassCode
{
public:
	CPassCode();                                          // 0x00102F62? TODO: verify
	virtual ~CPassCode();
	void SetTitle(const wchar_t *title);                  // 0x0002209E
	void Init();                                          // 0x0010341C
	void SetCode(const char *code);                       // 0x00103100
	void Show();                                          // 0x00102805
	virtual void ProcessInput();                          // vtable slot 0 TODO: verify
	BOOL IsClosed() const { return m_bCancelled || m_unknown50; }   // 0x0002208B
	BOOL IsCorrect();                                     // 0x00021FCE

	BYTE m_unknown04[0x2C];
	std::string m_strCode;                                // +0x30 what was entered
	BOOL m_bCancelled;                                    // +0x4C
	BOOL m_unknown50;                                     // +0x50
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
	virtual void ProcessInput();                          // vtable slot 2
	virtual BOOL IsOpen();                                // vtable slot 5

	BYTE m_unknown004[0x290];
	CMenuEventSink *m_pSink;                              // +0x294
};
