// The Control Panel pages (CSettingsMenu's icons). Each builds its list in
// Init, changes values on A/START in HandleInput, and writes Config.xml (and
// for some settings the EEPROM) in Save.
//
// HandleInput returns S_FALSE (1) when there was nothing left to do (the page
// is closed, or the base class used the input), S_OK otherwise.
#pragma once

#include "../xml/XmlDocument.h"
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

// AutoRun Settings ("QuickLaunch"): an XBE for each gamepad button and
// remote control digit, in <Settings><Shortcut><GP_A>path</GP_A>...
// 0xB8 bytes, vtable 0x002567EC.
class CQuickLaunch : public CSettingsBase
{
public:
	enum
	{
		ITEM_GAMEPAD_HEADER,
		ITEM_GP_A,              // ... ITEM_GP_BLACK = 7: A, B, X, Y, Start, White, Black
		ITEM_GP_BLACK = 7,
		ITEM_IR_HEADER,
		ITEM_IR_0,              // ... ITEM_IR_9 = 18
		ITEM_IR_9 = 18,
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CQuickLaunch();                                           // 0x000C1D0B
	virtual ~CQuickLaunch();                                  // 0x000C1CF7
	virtual HRESULT Init(CXBFont *font);                      // 0x000C45AB
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000D5734

	BOOL Save();                                              // 0x000CA63B
};

// The screen position editor inside CScreenAdjust: moves and sizes the
// visible area (CAppSettings::m_viewport) with the d-pad. 0x58 bytes.
// TODO: decompile (0x001018DC-0x00102514).
class CScreenAdjuster
{
public:
	CScreenAdjuster();                                        // 0x0010246E (this in ebx)
	~CScreenAdjuster();                                       // 0x00101F35 (this in esi)
	// Also finds the "XboxDings" icon font. Returns 0. (this in esi, font
	// in eax)
	HRESULT Init(CXBFont *font);                              // 0x00101F0E
	// Takes the current screen area. (this in eax)
	void Reset();                                             // 0x001018DC
	void Render();                                            // 0x001018FC (this in ebx)
	void HandleInput(int *action, SGamepadState **pad);       // 0x00102165 (this in edx)

	BOOL m_bOpen;                   // +0x00 set by Reset, cleared when the editor closes
	BYTE m_unknown04[0x58 - 0x04];
};

// Screen Adjust: a full-screen editor rather than a list. 0x110 bytes,
// vtable 0x002568AC.
class CScreenAdjust : public CSettingsBase
{
public:
	CScreenAdjust() {}                                        // 0x000C3A09
	virtual ~CScreenAdjust();                                 // 0x000C1BB8
	// Quirk: returns E_FAIL (not S_OK) when already initialized.
	virtual HRESULT Init(CXBFont *font);                      // 0x000C1B87
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000C39E6
	virtual HRESULT Render();                                 // 0x000C05CF
	virtual void Reset();                                     // 0x0002DE35
	virtual BOOL IsOpen();                                    // 0x0002DE49 the editor's flag

	CScreenAdjuster m_adjuster;     // +0xB8
};

class CInternetSession;

// Live Update: checks <server>Version.xml for a newer build, downloads it
// (a RAR with its signature removed), unpacks it next to UnleashX and
// launches UXUpdate.xbe to finish. 0x214 bytes, vtable 0x002568EC.
class CLiveUpdate : public CSettingsBase
{
public:
	enum
	{
		ITEM_AUTO_CHECK,
		ITEM_CHECK,             // check for a release
		ITEM_CHECK_WIP,         // check for a work-in-progress build
		ITEM_SERVER,
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CLiveUpdate();                                            // 0x000C1C87
	virtual ~CLiveUpdate();                                   // 0x000C1C39
	virtual HRESULT Init(CXBFont *font);                      // 0x000C422E
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000D4F97
	// Also runs the check/download/install steps.
	virtual HRESULT Render();                                 // 0x000D65CD
	virtual void Reset();                                     // 0x000C1C20

	BOOL Save();                                              // 0x000CA280
	// Unpacks the download (thread, param is the page). Returns 0 when
	// UXUpdate.xbe was unpacked.
	static unsigned __stdcall InstallThread(void *param);      // 0x000D5EF9

	CInternetSession *m_pSession;   // +0xB8
	BOOL m_bDownloading;            // +0xBC the update itself (not Version.xml)
	BOOL m_bAsked;                  // +0xC0 "download this version?" was shown
	BOOL m_bBusy;                   // +0xC4 a check or download is running
	BOOL m_bWIP;                    // +0xC8 the WIP line was chosen
	BOOL m_bWIPAlternate;           // +0xCC never set: would read <WIP_A>
	BOOL m_bUpdateAvailable;        // +0xD0
	BOOL m_bUpdateReady;            // +0xD4 unpacked; OK launches UXUpdate.xbe
	std::string m_strServer;        // +0xD8 ends in '/'
	CXmlDocument m_versionDoc;      // +0xF4 Version.xml
	CXmlElement *m_pMirror;         // +0x210 the <URL> being tried after a 404
};

// Network Settings: the network, FTP server, FTP client and HTTP proxy.
// 0xB8 bytes, vtable 0x0025684C.
class CNetworkSetting : public CSettingsBase
{
public:
	enum
	{
		ITEM_CURRENT_IP,        // read-only, refreshed every few seconds
		ITEM_LINK_STATUS,
		ITEM_MAC_ADDRESS,
		ITEM_ENABLE,            // <Network Enable>
		ITEM_TYPE,              // DHCP / Static
		ITEM_IP,                // IP address pop-up for these five
		ITEM_SUBNET,
		ITEM_GATEWAY,
		ITEM_DNS1,
		ITEM_DNS2,
		ITEM_FTP_HEADER,
		ITEM_FTP_ENABLE,
		ITEM_FTP_USER,          // keyboard for user ... SYST
		ITEM_FTP_PASSWORD,
		ITEM_FTP_PORT,
		ITEM_FTP_MAX_USERS,
		ITEM_FTP_ALLOW_ANON,
		ITEM_FTP_MIN_PASV_PORT,
		ITEM_FTP_MAX_PASV_PORT,
		ITEM_FTP_SYST,
		ITEM_FTP_BOOST,
		ITEM_AUTO_DETECT,       // <Network><AutoDetect>
		ITEM_CLIENT_HEADER,
		ITEM_CLIENT_MIN_PORT,
		ITEM_CLIENT_MAX_PORT,
		ITEM_CLIENT_DETECT_PEERS,
		ITEM_HTTP_HEADER,
		ITEM_PROXY_ENABLE,
		ITEM_PROXY,             // IP address pop-up
		ITEM_PROXY_PORT,
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CNetworkSetting() {}                                      // 0x000C1B2E
	virtual ~CNetworkSetting();                               // 0x000C1B1A
	virtual HRESULT Init(CXBFont *font);                      // 0x000CF410
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000CEC23
	// Enables the lines that depend on the switch at index (0 = all of
	// them, from the settings).
	virtual void SetItemEnabled(int index, BOOL enabled);     // 0x000BED3F

	BOOL Save();                                              // 0x000C66FD
	// Shows the address settings (FALSE) or the configuration in use
	// (TRUE) in the IP ... DNS 2 lines. (this in esi)
	void ShowAddresses(BOOL current);                         // 0x000C0107
};

// Checks a value typed on the Network page for the line index. On failure
// text is reset to the default and message says why. (index in eax, text
// in edi)
BOOL ValidateSettingValue(int index, wchar_t *text, char *message);   // 0x000BEF1A

// The HDD page's lock/unlock question. No vtable; 0x20 bytes. Most methods
// take this in a register (noted).
class CHddLockDialog
{
public:
	CHddLockDialog();                                         // 0x001196A6 (this in eax)
	~CHddLockDialog();                                        // 0x001196D6 (this in esi)
	HRESULT Init(CXBFont *font);                              // 0x001196EB (this in esi, font in ecx)
	// Asks to lock or unlock (or says it isn't supported). (this in eax)
	void Show();                                              // 0x00119824
	// Returns 1 when done, E_FAIL when the question was declined.
	// (this in edx, action in eax, pad in ecx)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x001198A4
	HRESULT Render();                                         // 0x001196C3 (this in eax)
	BOOL IsOpen() { return m_bOpen; }                         // 0x001196A2
	BOOL WasChanged() { return m_bChanged; }                  // 0x0011969E
	// Creates the message box on first use. (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x00119712

	CMessageBox *m_pMessageBox;     // +0x00
	CXBFont *m_pFont;               // +0x04
	CSound *m_pSoundSelect;         // +0x08 "Select"
	BOOL m_bDone;                   // +0x0C
	BOOL m_bChanged;                // +0x10 the lock was changed
	BOOL m_bOpen;                   // +0x14
	BOOL m_bUnlock;                 // +0x18 the disk is locked: offer to unlock
	BOOL m_bMessageBoxOpen;         // +0x1C
};

// Harddisk Settings: drives F and G, hard disk and DVD drive details, SMART
// values and the DVD media check. 0xC0 bytes, vtable 0x0025688C.
class CHDDSetting : public CSettingsBase
{
public:
	enum
	{
		ITEM_DRIVE_F,
		ITEM_DRIVE_G,
		ITEM_HDD_HEADER,
		ITEM_MODEL,
		ITEM_SERIAL,
		ITEM_STATUS,            // A: lock or unlock the disk
		ITEM_SPACE_C,           // total/free, refreshed every 5 seconds
		ITEM_SPACE_E,
		ITEM_SPACE_F,
		ITEM_SPACE_G,
		ITEM_SPACE_X,
		ITEM_SPACE_Y,
		ITEM_SPACE_Z,
		ITEM_SMART_HEADER,
		ITEM_INFO,
		ITEM_TRANSFER_MODE,
		ITEM_TEMPERATURE,
		ITEM_SMART_RAW_READ_ERROR,   // the SMART lines follow in the disk's order
		ITEM_SMART_SPIN_UP_TIME,
		ITEM_SMART_START_STOP,
		ITEM_SMART_REALLOCATED,
		ITEM_SMART_SEEK_ERROR,
		ITEM_SMART_POWER_ON,
		ITEM_SMART_SPIN_RETRY,
		ITEM_SMART_POWER_CYCLE,
		ITEM_SMART_CRC_ERROR,
		ITEM_SMART_WRITE_ERROR,
		ITEM_DVD_HEADER,
		ITEM_DVD_MODEL,
		ITEM_DVD_SERIAL,
		ITEM_MEDIA_CHECK,       // Normal / Extended
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CHDDSetting();                                            // 0x000C1B6D
	virtual ~CHDDSetting();                                   // 0x000C1B3A
	virtual HRESULT Init(CXBFont *font);                      // 0x000D0AD7
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000D0159
	virtual HRESULT Render();                                 // 0x000C8367

	BOOL Save();                                              // 0x000C7EB2

	CHddLockDialog *m_pLockDialog;  // +0xB8
	BOOL m_bLockDialogOpen;         // +0xBC
};

// Sounds and Audio Settings: music playback, volumes and the sound effect
// files. 0xB8 bytes, vtable 0x0025680C.
class CMusicSetting : public CSettingsBase
{
public:
	enum
	{
		ITEM_PLAY_MUSIC,        // Off / Xbox / Custom / Both (m_nValue holds the mode)
		ITEM_RANDOM,
		ITEM_GLOBAL,
		ITEM_MUSIC_PATH,        // folder pop-up; X clears it
		ITEM_SURROUND,          // 5.1 (also the EEPROM AC3 flag)
		ITEM_MUSIC_VOLUME,      // LEFT/RIGHT, 0-100 in m_nValue, drawn as a bar
		ITEM_SOUND_HEADER,
		ITEM_SOUND_ENABLE,
		ITEM_SOUND_VOLUME,      // as the music volume
		ITEM_EFFECTS_HEADER,
		ITEM_EFFECT_FIRST,      // one line per SoundEffect: a .wav (X clears it)
		ITEM_EFFECT_LAST = ITEM_EFFECT_FIRST + 9,
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	CMusicSetting() {}                                        // 0x000C1B0E
	virtual ~CMusicSetting();                                 // 0x000C1AFA
	virtual HRESULT Init(CXBFont *font);                      // 0x000C2CC1
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000D4A57
	virtual HRESULT Render();                                 // 0x000C261F

	BOOL Save();                                              // 0x000CE1B9
};
