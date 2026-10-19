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
};

// Edits a menu item's text, icon, action... 0x298 bytes.
class CMenuItemEditor
{
public:
	CMenuItemEditor();                                    // 0x0010A4E5
	virtual ~CMenuItemEditor();
	void Init(CXBFont *font, int unknown);                // 0x0010A05B
	void Edit(CMenuItem *item);                           // 0x0010986F

	BYTE m_unknown004[0x290];
	CMenuEventSink *m_pSink;                              // +0x294
};
