// CMenuItemEditor: the menu's item editor, a Control Panel style page for
// the selected menu entry. It edits the entry's title, icon (AltIcon),
// preview video, password and passcode, and saves them to Config.xml or,
// for a game found by the game-folder scan, to items.xml. For an XBE it
// can download the game's icon or preview from xbox-skins.net by title
// ID. See MenuItemEditor.cpp and docs/MENU.md.
#pragma once

#include <string>

#include "../system/SystemInfo.h"
#include "../xml/XmlDocument.h"
#include "SettingsBase.h"

class CAppSettings;
class CDropDown;
class CInternetSession;
class CMenuEventSink;
class CMenuItem;
class CPassCode;
struct IDirect3DTexture8;

// 0x298 bytes, vtable 0x0024F728.
class CMenuItemEditor : public CSettingsBase
{
public:
	// The lines (CSettingItem::m_nType is the index)
	enum
	{
		ITEM_TITLE,             // keyboard; must not be empty
		ITEM_ICON,              // file pop-up (png|jpg|bmp|xbx); START: clear / download
		ITEM_PREVIEW,           // file pop-up (xmv|wmv); START: clear / download
		ITEM_PASSWORD,          // keyboard, masked
		ITEM_PASSCODE,          // the passcode pad
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_EXIT,
	};

	// What the download is fetching (m_nDownloadStep).
	enum
	{
		DOWNLOAD_INFO = 0,      // <title ID>.xml, which says what there is
		DOWNLOAD_PREVIEW = 1,
		DOWNLOAD_ICON = 2,
		DOWNLOAD_IDLE = 3,
	};

	CMenuItemEditor();                                    // 0x0010A4E5 (this in esi)
	virtual ~CMenuItemEditor();                           // 0x001098D2 (deleting 0x00023446)
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // slot 2, 0x0010AE75
	virtual HRESULT Render();                             // slot 3, 0x0010BEA5
	// Loads the item's current values. (Edit calls it.)
	virtual void Reset();                                 // slot 4, 0x00109CE7

	// Builds the page (once) and sets the item, if any. E_FAIL without a
	// font. (font in eax, this in edx, item in ecx)
	HRESULT Init(CXBFont *font, CMenuItem *item);         // 0x0010A05B
	using CSettingsBase::Init;                            // (slot 1 isn't overridden)
	// Edits item. (item in eax)
	void Edit(CMenuItem *item) { m_pItem = item; Reset(); }   // 0x0010986F
	// The menu calls these (CSettingsBase's).
	HRESULT ProcessInput(int *action, SGamepadState **pad) { return HandleInput(action, pad); }

	// Writes the changed lines; TRUE if nothing needed saving. (this on
	// the stack)
	BOOL Save();                                          // 0x0010A61C
	// Moves a finished download into the game's folder (icon.png or
	// preview.xmv/wmv, unpacking a zip/rar preview). (this on the stack)
	BOOL InstallDownload();                               // 0x0010B6F2
	static unsigned __stdcall InstallThread(void *param); // 0x0010BE83

	CMenuItem *m_pItem;                    // +0x0B8
	CInternetSession *m_pSession;          // +0x0BC
	BOOL m_bDownloading;                   // +0x0C0
	int m_nDownloadStep;                   // +0x0C4 DOWNLOAD_*
	CPassCode *m_pPassCode;                // +0x0C8
	BOOL m_bPassCodeOpen;                  // +0x0CC
	CDropDown *m_pDropDown;                // +0x0D0 START on the icon/preview line
	BOOL m_bDropDownOpen;                  // +0x0D4
	BOOL m_bIsXbe;                         // +0x0D8 the item's Action is an .xbe
	std::string m_strTitleId;              // +0x0DC <Item ID=...>, else read from the XBE ("%08x")
	std::string m_strTitle;                // +0x0F8 the title as UTF-8
	std::string m_strAction;               // +0x114 <Item Action=...>
	CXmlDocument m_info;                   // +0x130 the downloaded <title ID>.xml
	int m_unknown24C;                      // +0x24C cleared while downloading
	UINT m_nValueX;                        // +0x250 where the selected line's value is drawn (the pop-up's place)
	UINT m_nValueY;                        // +0x254
	IDirect3DTexture8 *m_pLogo;            // +0x258 <media>\xboxskins.jpg, shown with the download
	CMD5 m_md5;                            // +0x25C unused
	CAppSettings *m_pSettings;             // +0x290
	CMenuEventSink *m_pSink;               // +0x294 the menu's; stops and replays the preview
};

// The install thread's handle, INVALID_HANDLE_VALUE when none.
extern HANDLE g_hItemInstallThread;        // 0x0026C728
// Set while a download is installed (one at a time).
extern BOOL g_bItemInstalling;             // 0x00278D04
