// The full-screen tools CUnleashX switches to (CUnleashX::SetMode). Only the
// interface the app code uses so far; all TODO: decompile.
#pragma once

#include <string>

#include "../common/Platform.h"

class CXBFontEx;
struct IDirect3DTexture8;

// Screen modes (CUnleashX::m_nMode)
enum AppMode
{
	MODE_MENU = 1,
	MODE_FILEMANAGER = 2,
	MODE_GAMEMANAGER = 3,
	MODE_SETTINGS = 5,
	MODE_SKINS = 6,
	MODE_FTPSITES = 7,
	MODE_TEXTEDITOR = 8,
	MODE_MUSIC = 9,
};

class CFileManager
{
public:
	CFileManager();                                       // 0x00122BA8? TODO: verify
	HRESULT Init(int unknown1, int unknown2, IDirect3DTexture8 *folderIcon,
	             IDirect3DTexture8 *fileIcon);            // 0x001294E2
	void OpenPath(const char *path);                      // 0x00128C01

	BYTE m_unknown000[0x8DC];
	BOOL m_bInitialized;                                  // +0x8DC
	BYTE m_unknown8E0[0x10];
};

// Game saves / game list manager ("SavesManager", "GameManager"). 0x210 bytes.
class CGameManager
{
public:
	CGameManager();                                       // 0x0011C649
	virtual ~CGameManager();
	HRESULT Init(CXBFontEx *font);                        // 0x0011E048
	void Show();                                          // 0x0011BAFA
};

// The settings menu. 0xA0 bytes.
class CSettingsMenu
{
public:
	CSettingsMenu();                                      // 0x000BEBFD
	HRESULT Init(CXBFontEx *font);                        // 0x000C18A3
	void Show();                                          // 0x0002B530
};

// The skin chooser. 0xD0 bytes.
class CSkinBrowser
{
public:
	CSkinBrowser();                                       // 0x000F6597
	HRESULT Init(IDirect3DTexture8 *checked, IDirect3DTexture8 *unchecked,
	             CXBFontEx *font);                        // 0x000F5DEC
	void Show();                                          // 0x000F6791

	BYTE m_unknown00[0x58];
	std::string m_strSkinPath;                            // +0x58
};

class CNotePad
{
public:
	CNotePad();                                           // 0x00103A0C? TODO: verify
	void Init(int, int, int);                             // 0x00104399
	void NewFile();                                       // 0x00103D4D
	HRESULT Open(const char *path);                       // 0x00104181
};

class CDateTimeSetting
{
public:
	CDateTimeSetting();                                   // 0x000C66A7
	virtual ~CDateTimeSetting();
	HRESULT Init(CXBFontEx *font);                        // 0x000C1FAC
	void Show();                                          // 0x000C0B13
};

// The message box (OK / Cancel / Abort / Retry / Ignore / Yes / No). 0x158 bytes.
class CMessageBox
{
public:
	CMessageBox();                                        // 0x000B10D4
	virtual ~CMessageBox();
	HRESULT Create(int, int, int, int, int, int);         // 0x000B084D
	void AddButton(const wchar_t *text);                  // 0x000AFB3D
	void Show(const char *text, const wchar_t *title);    // 0x000B0F74
};

// The on-screen keyboard; $KBResult$ in an item's arguments is its last text.
class CVirtualKeyboard
{
public:
	std::string GetResult();                              // 0x00014242
};
extern CVirtualKeyboard g_virtualKeyboard;               // 0x00278328

// DVD tray state from the SMC.
class CTrayMonitor
{
public:
	int GetTrayState();                                   // 0x000B329C 0x10 = open
	HRESULT CloseTray();                                  // 0x000B32AE
	HRESULT OpenTray();                                   // 0x000B32C8
};
extern CTrayMonitor g_trayMonitor;                       // 0x00278658
