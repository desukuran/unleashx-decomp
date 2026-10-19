// The game saves manager ("SavesManager" / "GameManager" actions, mode 3):
// a list of the games with saved data on the hard disk (E:\UDATA) and the
// memory units, in the MS dashboard's style, each with a grid of its saves
// and downloaded content (E:\TDATA). A save or a whole game can be copied
// to another drive or deleted. See GameManager.cpp (lists, input) and
// GameManagerRender.cpp; docs/GAME_SAVES.md.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "../common/XboxKernel.h"
#include "../ui/MenuDialogs.h"
#include "XBApp.h"

class CAppSettings;
class CInternetSession;
class CMessageBox;
class CSound;
class CXBFont;
class CXBFontEx;
struct IDirect3DTexture8;
struct SGamepadState;

// A save (or a piece of downloaded content). 0xDC bytes, vtable 0x0024E20C.
class CGameSaveItem
{
public:
	// A save found under a game's folder (gamePath, else the save's own
	// directory). (data in eax, this on the stack)
	CGameSaveItem(const XGAME_FIND_DATA &data, const char *gamePath);   // 0x0011D4B0
	// Downloaded content. Quirk: this builds a second, temporary item from
	// the content's data and throws it away, so the item itself stays
	// empty (no path, title or size). (data in eax, this in ebx)
	CGameSaveItem(const XCONTENT_FIND_DATA &data, const char *gamePath);   // 0x0011D5DF
	virtual ~CGameSaveItem();                             // 0x0011BF60 (deleting 0x00020C11)

	// The KB and block texts (from the size the folder takes).
	void ComputeSize();                                   // 0x0011D34F (this in esi)
	// Its saveimage.xbx (or the one in its parent folder), 48x48, once.
	// (this on the stack)
	BOOL LoadImage();                                     // 0x0011C3B0

	char *m_pszPath;                       // +0x04 the save's folder (0x104 bytes, owned)
	wchar_t *m_pszTitle;                   // +0x08 the save's name, in quotes (0x80 wchars, owned)
	wchar_t m_szDate[20];                  // +0x0C "date time" of the last write
	wchar_t m_szFolder[20];                // +0x34 the folder's name
	wchar_t m_szSize[25];                  // +0x5C "n KB"
	wchar_t m_szBlocks[25];                // +0x8E "n Blocks"
	int m_unknownC0;                       // +0xC0
	IDirect3DTexture8 *m_pImage;           // +0xC4 saveimage.xbx (through g_pImageManager)
	BOOL m_bImageTried;                    // +0xC8
	float m_fSizeKB;                       // +0xCC
	DWORD m_dwBlocks;                      // +0xD0
	BOOL m_bContent;                       // +0xD4 downloaded content (can't be copied here)
	int m_unknownD8;                       // +0xD8
};

// A game with saved data. 0x328 bytes.
struct SGameEntry
{
	// (path in the first argument, this in esi)
	SGameEntry(const char *path, const wchar_t *name, BOOL onMemoryUnit, const char *titleId);   // 0x0011BA03
	~SGameEntry();                                         // 0x00119C82 (deleting 0x00020C59)

	// Lists the saves (and the content in TDATA) and the totals.
	void LoadSaves();                                     // 0x0011D64D (this on the stack)
	// The TitleImage.xbx (96x96) and the saves' images, once. (this on the
	// stack)
	BOOL LoadImages();                                    // 0x0011C792
	void FreeSaves();                                     // 0x00119C3F (this in eax)

	std::vector<CGameSaveItem *> saves;   // +0x000
	BOOL imagesLoaded;                    // +0x010
	float sizeKB;                         // +0x014
	DWORD blocks;                         // +0x018
	int unknown1C;                        // +0x01C
	char titleId[12];                     // +0x020 the folder's name (8 hex digits)
	char path[0x104];                     // +0x02C "E:\UDATA\<id>\" or "<mu>:\<id>\"
	wchar_t name[0x80];                   // +0x130 TitleMeta.xbx's TitleName
	long titleNumber;                     // +0x230 atol of the folder's name (not used)
	UINT saveCount;                       // +0x234 never set (the delete question shows 0)
	UINT contentCount;                    // +0x238 never set
	IDirect3DTexture8 *image;             // +0x23C TitleImage.xbx
	wchar_t sizeText[25];                 // +0x240 "n KB"
	wchar_t blocksText[25];               // +0x272 "n Blocks"
	wchar_t countText[44];                // +0x2A4 "%u Saves, %u Contents" (memory units: "n Saves")
	wchar_t folderName[20];               // +0x2FC
	BOOL onMemoryUnit;                    // +0x324
};

// 0x210 bytes, no vtable.
class CGameManager
{
public:
	CGameManager();                                       // 0x0011C649 (this in ebx)
	~CGameManager();                                      // 0x0011BC38 (this in edi; deleting 0x0002C6BA)
	// font: the list's. defaultIcon: a game or save without its own image
	// (the dashboard's orb). (defaultIcon in eax, envMap in ecx, this in
	// edx)
	HRESULT Init(CXBFont *font, IDirect3DTexture8 *defaultIcon, IDirect3DTexture8 *envMap);   // 0x0011E048
	// Opens the screen: lays it out for the screen and sets the sounds'
	// volume. (this in ebx)
	void Show();                                          // 0x0011BAFA
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x0011E249 (this on the stack)
	HRESULT Render();                                     // 0x0011C8F4 (this in eax)

	// Scans the drives for games (on a thread, else here). (this in eax)
	void Reload();                                        // 0x0011DFDD
	// The scan itself. (this in ebx)
	void ScanGames();                                     // 0x0011DB91
	static unsigned __stdcall ScanThread(void *param);    // 0x0011DFC4
	// Creates the message box on first use and shows it. (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x0011C52C

	// Drawing (GameManagerRender.cpp). (this in ebx or eax)
	void DrawHeader(float x, float y);                    // 0x0011AB23
	void DrawTab(float x, float y, const wchar_t *text, DWORD textColor, DWORD fillColor, DWORD borderColor,
	             float alpha, DWORD lineWidth);           // 0x0011B123
	void DrawSavePanel(float x, float y, int game, DWORD fillColor, DWORD borderColor, float alpha);   // 0x0011A4A1
	void DrawIcon(float x, float y, float width, float height, IDirect3DTexture8 *texture, float alpha,
	              BOOL selected);                         // 0x0011B33C
	void DrawInfoPanel(float x, float y);                 // 0x00119C91 (x in xmm6)
	// Copy, Delete. (Copy only on a save.)
	void OpenMenu();
	void OpenCopyMenu(SGameEntry *game);
	void CopySave(SGameEntry *game, CGameSaveItem *save, int destination);
	void AskDelete(SGameEntry *game);
	void DeleteSelected();

	std::vector<SGameEntry *> m_games;     // +0x000
	std::vector<BYTE> m_downloads;         // +0x010 0xE8-byte entries of a removed save download; never filled
	float m_fX;                            // +0x020 80 at 640x480
	float m_fY;                            // +0x024 20
	float m_fWidth;                        // +0x028 480
	float m_fHeight;                       // +0x02C 440
	BOOL m_bLoaded;                        // +0x030 the scan is done
	float m_fScaleX;                       // +0x034 screen/640, at least 1
	float m_fScaleY;                       // +0x038 screen/480, at least 1
	CXBFont *m_pFont;                      // +0x03C
	CXBFont *m_pSymbolFont;                // +0x040 XboxDings (the button glyphs)
	CXBFont *m_pTitleFont;                 // +0x044 the dialog font
	CSound *m_pSound48;                    // +0x048 (deleted, never set)
	CSound *m_pSoundSelect;                // +0x04C
	CSound *m_pSoundBack;                  // +0x050
	CSound *m_pSoundGame;                  // +0x054 "Game Save Select" (moving between games)
	CSound *m_pSoundSlot;                  // +0x058 "Memory Slot" (moving between saves)
	CSound *m_pSoundDone;                  // +0x05C "Action Complete"
	IDirect3DTexture8 *m_pDefaultIcon;     // +0x060
	IDirect3DTexture8 *m_pEnvMap;          // +0x064 (not drawn)
	IDirect3DTexture8 *m_pTexture68;       // +0x068 (released, never set)
	BOOL m_bMessageShown;                  // +0x06C
	CMessageBox *m_pMessageBox;            // +0x070
	BOOL m_bOpen;                          // +0x074
	CXBMesh m_mesh78;                      // +0x078 (not used)
	CXBMesh m_mesh90;                      // +0x090 (not used)
	BOOL m_bInSaves;                       // +0x0A8 the focus is on the save grid
	int m_nGame;                           // +0x0AC
	int m_nSave;                           // +0x0B0
	int m_nSaveColumn;                     // +0x0B4 0-3
	int m_nSaveRow;                        // +0x0B8
	int m_nSaveRows;                       // +0x0BC
	COptionMenu m_menu;                    // +0x0C0 "Select": Copy, Delete
	BOOL m_bMenuShown;                     // +0x154
	COptionMenu *m_pCopyMenu;              // +0x158 "Copy To": hard disk, memory units 1A-4B
	BOOL m_bCopyMenuShown;                 // +0x15C
	COptionMenu *m_pDownloadMenu;          // +0x160 the removed download: never set
	BOOL m_bDownloadMenuShown;             // +0x164
	CInternetSession *m_pSession;          // +0x168 never set
	BOOL m_bDownloadMode;                  // +0x16C never set
	BOOL m_bDownloading;                   // +0x170
	BOOL m_unknown174;                     // +0x174
	int m_unknown178;                      // +0x178
	std::string m_str17C;                  // +0x17C (unused)
	std::string m_str198;                  // +0x198 (unused)
	BOOL m_bDeletePending;                 // +0x1B4 the message box is the delete question
	BOOL m_bWaitForCopy;                   // +0x1B8 input ignored while a copy runs; then "Action Complete"
	int m_unknown1BC;                      // +0x1BC
	float m_world[16];                     // +0x1C0 a world transform (set, not needed by the 2D drawing)
	CAppSettings *m_pSettings;             // +0x200
	BYTE m_unknown204[0xC];                // +0x204
};

// (The scan sorts the games with std::sort, 0x00020D23, by GameLess.)
