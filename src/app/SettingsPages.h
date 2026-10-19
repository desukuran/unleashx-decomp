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

// Date/Time Settings: the clock (opens the clock pop-up), 24-hour format,
// setting the clock at startup and the SNTP time server. 0xB8 bytes, vtable
// 0x0025682C.
class CDateTimeSetting : public CSettingsBase
{
public:
	enum
	{
		ITEM_CLOCK,             // the current date and time, refreshed every second
		ITEM_24H_FORMAT,
		ITEM_AUTO_SET_CLOCK,
		ITEM_SNTP_HEADER,
		ITEM_SNTP_ENABLE,
		ITEM_SNTP_SERVER,       // IP address pop-up
		ITEM_SNTP_PORT,         // keyboard
		ITEM_SNTP_ALLOW_HTTP,
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CDateTimeSetting() {}                                     // 0x000C1C14
	virtual ~CDateTimeSetting();                              // 0x000C1C00
	virtual HRESULT Init(CXBFont *font);                      // 0x000D1BBB
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000D16A4
	// Quirk: ignores index; enables or disables the three SNTP lines.
	virtual void SetItemEnabled(int index, BOOL enabled);     // 0x000BEECD

	BOOL Save();                                              // 0x000C9B4B
};

// Checks a value typed for the line index (only the SNTP port is checked: it
// must be 1..65535). On failure text is reset to "13" and message says why.
// The original takes text in edi.
BOOL ValidateSNTPValue(wchar_t *text, int index, char *message);   // 0x000BF191

// AutoLaunch Settings: what to start when a game, DVD movie, audio CD or
// data disc is inserted. 0xB8 bytes, vtable 0x0025686C.
class CAutoLaunch : public CSettingsBase
{
public:
	enum
	{
		ITEM_GAMES,             // <Games AutoLaunch>
		ITEM_DVD,               // <DVD AutoLaunch>
		ITEM_DVD_PLAYER,        // <DVD>xbe</DVD>, file pop-up; X clears it
		ITEM_AUDIO_CD,
		ITEM_AUDIO_CD_PLAYER,
		ITEM_DATA,
		ITEM_DATA_LAUNCHER,     // the "File Manager"
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CAutoLaunch();                                            // 0x000C1D32
	virtual ~CAutoLaunch();                                   // 0x000C1D1E
	virtual HRESULT Init(CXBFont *font);                      // 0x000C4EC9
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000D5AFF
	// (Render, 0x000CB399, just calls CSettingsBase::Render.)

	BOOL Save();                                              // 0x000CAA5D
};
