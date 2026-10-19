// The Control Panel pages (CSettingsMenu's icons). Each builds its list in
// Init, changes values on A/START in HandleInput, and writes Config.xml (and
// for some settings the EEPROM) in Save.
//
// HandleInput returns S_FALSE (1) when there was nothing left to do (the page
// is closed, or the base class used the input), S_OK otherwise.
#pragma once

#include "SettingsBase.h"

// Video Settings. 0xBC bytes, vtable 0x0025692C.
class CVideoSetting : public CSettingsBase
{
public:
	// Item types (CSettingItem::m_nType)
	enum
	{
		ITEM_STANDARD,          // NTSC M / NTSC J / PAL, written to the EEPROM
		ITEM_FORMAT,            // Normal / Widescreen
		ITEM_480P,
		ITEM_720P,
		ITEM_1080I,
		ITEM_PAL60,
		ITEM_FAST_REFRESH,
		ITEM_TEXTURE_COMPRESSION,
		ITEM_TEST,              // tries the mode for 10 seconds
		ITEM_QUALITY_HEADER,
		ITEM_COLOR,             // 16 bit / 32 bit
		ITEM_SOFTEN,
		ITEM_FILTER_LEVEL,      // 0..5
		ITEM_VIDEO_MODE_HEADER,
		ITEM_AUTO_SWITCH,
		ITEM_MULTI_REGION,      // Off / NTSC M / NTSC J / PAL
		ITEM_FORCE_PAL60,
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CVideoSetting();                                          // 0x000C1BED
	virtual ~CVideoSetting();                                 // 0x000C1BD9
	virtual HRESULT Init(CXBFont *font);                      // 0x000C3A26
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000C9182
	// Quirk: ignores index; enables or disables the two lines under Auto
	// Switch (the only caller passes ITEM_AUTO_SWITCH).
	virtual void SetItemEnabled(int index, BOOL enabled);     // 0x000BEEAC

	// Writes the changed lines; FALSE on an error (a message is shown).
	// The original takes this on the stack.
	BOOL Save();                                              // 0x000C8387
	// ITEM_TEST: switches to the chosen mode for 10 seconds.
	void TestVideoMode();

	DWORD m_unknownB8;              // +0xB8
};
