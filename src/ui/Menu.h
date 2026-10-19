// The main menu: the <Menu> tree from Config.xml (merged with the scanned
// game folders), shown as a scrolling list by the skin. Selecting an item
// goes through CMenuEventSink::OnItemSelected (CUnleashX's handler runs the
// action); <List> items open sub-menus.
#pragma once

#include <string>
#include <vector>

#include "../app/XBApp.h"
#include "../common/Platform.h"
#include "../system/SystemInfo.h"

class CAppSettings;
class CMenuItem;
class CMessageBox;
class COptionMenu;
class CPassCode;
class CMenuItemEditor;
class CSound;
class CXBFont;
class CXmlDocument;
class CXmlElement;
struct IDirect3DSurface8;
struct IDirect3DTexture8;

// Callbacks from CMenu (CUnleashX::CMenuEvent implements them).
class CMenuEventSink
{
public:
	virtual ~CMenuEventSink() {}                      // slot 0, 0x0002C65F
	// An <Item> (or a <Command>) was chosen; for a <List>, whether to open
	// it. Returns FALSE when the action failed or left the menu.
	virtual BOOL OnItemSelected(CXmlElement *item) { (void)item; return TRUE; }   // slot 1
	virtual BOOL Unknown2() { return TRUE; }          // slot 2, 0x0002B65C
	virtual void Unknown3() {}                        // slot 3, 0x0002B8DA
	virtual void Unknown4() {}                        // slot 4, 0x0002B8DA
};

// Recently launched XBEs, kept in <data path>\recent.dat as 10 of these.
// 0x138 bytes.
struct SRecentItem
{
	char name[0x2B];         // +0x000
	char path[0x105];        // +0x02B
	int launchCount;         // +0x130
	int region;              // +0x134
};

// Gamepad state of the menu (CGamepadInput). TODO: decompile (0x00021F02)
struct SMenuInput
{
	BYTE data[0xA8];
};

#define MENU_MAX_DEPTH 20

// 0xF44 bytes
class CMenu
{
public:
	CMenu();                                          // 0x00110B3B
	virtual ~CMenu();                                 // 0x0010CF0B

	// doc: the menu document; fonts for normal and selected items (selected
	// defaults to normal). fontItemHeight: see m_bFontItemHeight.
	HRESULT Create(CXmlDocument *doc, CXBFont *font, CXBFont *selectedFont, CMenuEventSink *sink,
	               BOOL fontItemHeight, int unknownF18, IDirect3DTexture8 *texture70,
	               IDirect3DTexture8 *texture74);    // 0x00111F27

	// Fills m_items from a <List> (NULL = the menu root).
	void LoadList(CXmlElement *list);                 // 0x00110F47
	// Switches to list, loading it on a thread.
	void OpenList(CXmlElement *list);                 // 0x001115B2
	void ReloadList();                                // 0x00022635
	CMenuItem *GetSelectedItem();                     // 0x0010CE86

	// Activates an item or list: menu-level actions (AskUser, MessageBox,
	// InputBox, Batch lists) are handled here, the rest goes to the sink.
	BOOL RunItem(CXmlElement *item, BOOL isList);     // 0x0011166B
	// FALSE if the item is locked: the keyboard or passcode screen is shown
	// and the item runs once it's answered. password receives the item's
	// Password (or Passcode).
	BOOL CheckPassword(CXmlElement *item, std::string &password);   // 0x0010FF6A

	void ShowMessage(const char *text, const wchar_t *title, int buttons = 0);   // 0x0010FDB8
	void OpenRecentList();                            // 0x00110DCB
	void OpenItemEditor();                            // 0x0010FEFC
	// Scans the game folders (<List Path=...>) on a thread.
	void ScanFolders();                               // 0x00112364
	void UpdateSoundVolume();                         // 0x00022647
	// Copies the image the previous program left on screen (for fades).
	BOOL CaptureBootImage();                          // 0x0010CE1D

	BOOL FrameMove();                                 // 0x001123B9 TODO: decompile
	void Render();                                    // 0x0010D055 TODO: decompile

	// Internal
	void AddItem(CXmlElement *element, BOOL noSort);  // 0x00110772
	void FindItemIcon(CXmlElement *item);             // 0x001100F6
	void ScanFoldersNow();                            // 0x001120A7
	static void PlaySound(CSound *sound);             // 0x0010CEB2

	float m_fAreaX;                           // +0x008 the menu area (skin), default 50
	float m_fAreaY;                           // +0x00C 50
	float m_fAreaWidth;                       // +0x010 540
	float m_fAreaHeight;                      // +0x014 380
	BYTE m_unknown018[0x4];                   // +0x018
	int m_unknown01C;                         // +0x01C
	float m_fUnknown20;                       // +0x020
	float m_fUnknown24;                       // +0x024
	int m_unknown028;                         // +0x028
	CXBFont *m_pFont;                         // +0x02C
	CXBFont *m_pSelectedFont;                 // +0x030
	CXmlDocument *m_pMenuDoc;                 // +0x034
	CSound *m_pSoundSelect;                   // +0x038
	CSound *m_pSoundBack;                     // +0x03C
	CSound *m_pSoundUp;                       // +0x040
	CSound *m_pSoundDown;                     // +0x044
	BOOL m_bSounds;                           // +0x048
	BOOL m_bReady;                            // +0x04C list fully loaded
	std::vector<CMenuItem *> m_items;         // +0x050
	BOOL m_bLoadedInline;                     // +0x060 no thread: OpenList loaded it itself
	BOOL m_unknown064;                        // +0x064
	IDirect3DSurface8 *m_pBootSurface;        // +0x068
	IDirect3DTexture8 *m_pBootTexture;        // +0x06C
	IDirect3DTexture8 *m_pTexture70;          // +0x070
	IDirect3DTexture8 *m_pTexture74;          // +0x074
	int m_nDepth;                             // +0x078 sub-menu level
	int m_selectedStack[MENU_MAX_DEPTH];      // +0x07C m_nSelected of each parent
	int m_firstVisibleStack[MENU_MAX_DEPTH];  // +0x0CC m_nFirstVisible of each parent
	BOOL m_bWaitingForKeyboard;               // +0x11C password/InputBox keyboard is up
	BOOL m_bMessageShown;                     // +0x120
	BOOL m_unknown124;                        // +0x124 1
	CMessageBox *m_pMessageBox;               // +0x128
	CXmlElement *m_pBatchList;                // +0x12C
	int m_nBatchIndex;                        // +0x130
	BOOL m_bBatchRunning;                     // +0x134
	int m_unknown138;                         // +0x138
	BYTE m_unknown13C[0x4];                   // +0x13C
	CXBStopWatch m_timer;                     // +0x140
	int m_unknown160;                         // +0x160
	int m_unknown164;                         // +0x164
	int m_nSkinVisibleItems;                  // +0x168 0 = fit the area
	int m_unknown16C;                         // +0x16C
	void *m_pUnknown170;                      // +0x170 deleted by the destructor
	int m_unknown174;                         // +0x174
	float m_fUnknown178;                      // +0x178
	float m_fUnknown17C;                      // +0x17C
	BOOL m_bShowPreviews;                     // +0x180 look for preview videos next to XBEs
	CPassCode *m_pPassCode;                   // +0x184
	BOOL m_bWaitingForPassCode;               // +0x188
	CMenuItemEditor *m_pItemEditor;           // +0x18C
	BOOL m_bItemEditorShown;                  // +0x190
	CXmlElement *m_pCommand;                  // +0x194 a <Command> element reused for generated actions
	DWORD m_dwListOpenTime;                   // +0x198
	int m_unknown19C;                         // +0x19C
	int m_unknown1A0;                         // +0x1A0
	std::string m_strUnknown1A4;              // +0x1A4
	COptionMenu *m_pRecentMenu;               // +0x1C0
	BOOL m_bRecentMenuShown;                  // +0x1C4
	SRecentItem m_recent[10];                 // +0x1C8 recent.dat
	IDirect3DTexture8 *m_pIconTexture;        // +0xDF8 icon of the selected item
	int m_unknownDFC;                         // +0xDFC
	int m_unknownE00;                         // +0xE00
	CMD5 m_md5;                               // +0xE04
	BOOL m_bDirty;                            // +0xE38
	CAppSettings *m_pSettings;                // +0xE3C
	CXmlElement *m_pCurrentList;              // +0xE40
	int m_nFirstVisible;                      // +0xE44
	int m_nLastVisible;                       // +0xE48
	int m_nSelected;                          // +0xE4C
	int m_nItemHeight;                        // +0xE50 34 unless the skin sets it
	int m_nVisibleItems;                      // +0xE54
	int m_nItemCount;                         // +0xE58
	CMenuEventSink *m_pSink;                  // +0xE5C
	SMenuInput m_input;                       // +0xE60
	int m_unknownF08;                         // +0xF08 1
	BOOL m_bFontItemHeight;                   // +0xF0C set: lines are as tall as the font
	int m_unknownF10;                         // +0xF10 1
	int m_unknownF14;                         // +0xF14 12
	int m_unknownF18;                         // +0xF18
	int m_nItemSpacing;                       // +0xF1C 2, added above and below each line
	float m_fUnknownF20;                      // +0xF20 1.0
	float m_fUnknownF24;                      // +0xF24 1.0
	float m_fTextScale;                       // +0xF28 grows the text when lines are tall
	int m_nMaxVisibleItems;                   // +0xF2C 0 = no limit
	BYTE m_unknownF30;                        // +0xF30
	BYTE m_unknownF31;                        // +0xF31
	int m_unknownF34;                         // +0xF34
	int m_unknownF38;                         // +0xF38
	DWORD m_dwIconLoadTime;                   // +0xF3C
	float m_fUnknownF40;                      // +0xF40
};

// Passwords already entered for locked items this session.
struct SUnlockedItem
{
	CXmlElement *item;
	std::string password;
};
extern std::vector<SUnlockedItem> g_unlockedItems;   // 0x00277958

// The game folder cache (d:\items.xml, resolved).
extern CXmlDocument *g_pItemCache;                   // 0x002761F4
