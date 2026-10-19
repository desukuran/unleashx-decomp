// CUnleashX: the application. It owns the menu, the fonts and the tool
// screens, and switches between them (SetMode).
#pragma once

#include <string>

#include "../common/Platform.h"
#include "../render/Fonts.h"
#include "../skin/Skin.h"
#include "../ui/Menu.h"
#include "Screens.h"
#include "XBApp.h"

class CXmlElement;

// 0x2268 bytes
class CUnleashX : public CXBApplicationEx
{
public:
	CUnleashX();                                      // 0x000E229C
	virtual ~CUnleashX();                             // 0x0002B81C

	virtual HRESULT Initialize();                     // 0x000EC94A TODO: decompile
	virtual HRESULT FrameMove();                      // 0x000EB93B TODO: decompile
	virtual HRESULT Render();                         // 0x000F0097 TODO: decompile
	virtual HRESULT Cleanup();                        // 0x000E1B1E TODO: decompile

	void SetMode(int mode);                           // 0x000E568F TODO: decompile
	// Message box with OK; text is ANSI.
	void ShowMessage(const char *text, const wchar_t *title, int flags = 0);   // 0x000E2153
	BOOL CreateTextEditor();                          // 0x000E2853
	void PrepareHardDisk();                           // 0x000E48B4 TODO: decompile
	// Starts the disc in the drive (games, or the DVD/CD player).
	HRESULT LaunchDVD();                              // 0x000E99FD TODO: decompile

	// Menu callbacks; m_pApp points back here.
	class CMenuEvent : public CMenuEventSink
	{
	public:
		virtual BOOL OnItemSelected(CXmlElement *item);   // 0x000EE514
		virtual BOOL Unknown2() { return TRUE; }      // 0x000DFF11
		virtual void Unknown4();                      // 0x000E4173 TODO: decompile

		CUnleashX *m_pApp;                            // +0x04
	};

	class CSkinEvent : public CSkinEventSink
	{
	public:
		CUnleashX *m_pApp;                            // +0x04
	};

	CXBPackedResource m_resource;             // +0x220 UnleashX's own .xpr (icons)
	CXBFontEx m_fonts[6];                     // +0x238 0xE4 each
	CXBMesh m_meshes[2];                      // +0x790 CXBMeshEx
	BYTE m_unknown7D0[0x10];                  // +0x7D0
	IDirect3DTexture8 *m_pFileIcon;           // +0x7E0 File.png
	IDirect3DTexture8 *m_pFolderIcon;         // +0x7E4 Folder.png
	BYTE m_unknown7E8[0x14];                  // +0x7E8
	CDateTimeSetting *m_pDateTimeSetting;     // +0x7FC
	BYTE m_unknown800[0x8];                   // +0x800
	void *m_pDefaultGamepad;                  // +0x808 &m_defaultGamepad
	void *m_pDefaultIR;                       // +0x80C &m_defaultIR
	CXBStopWatch m_timer;                     // +0x810
	int m_nMode;                              // +0x830 AppMode
	int m_nPrevMode;                          // +0x834
	CXBStopWatch m_timers[2];                 // +0x838
	float m_unknown878;                       // +0x878
	BYTE m_unknown87C[0x174];                 // +0x87C
	CMenu m_menu;                             // +0x9F0
	CGameManager *m_pGameManager;             // +0x1938
	CFileManager *m_pFileManager;             // +0x193C
	CMessageBox *m_pMessageBox;               // +0x1940
	CSkinBrowser *m_pSkinBrowser;             // +0x1944
	void *m_pFtpSites;                        // +0x1948 TODO: class
	CNotePad *m_pTextEditor;                  // +0x194C
	BYTE m_unknown1950[0x8];                  // +0x1950
	CSettingsMenu *m_pSettingsMenu;           // +0x1958
	BOOL m_bMessageShown;                     // +0x195C
	BYTE m_unknown1960[0x18];                 // +0x1960
	CXBMesh m_mesh1978;                       // +0x1978
	BYTE m_unknown1998[0x2264 - 0x1998 - 0x1C];
	CMenuEvent m_menuEvent;                   // +0x224C
	CSkinEvent m_skinEvent;                   // +0x2254
	BOOL m_bDiscLaunchPending;                // +0x2260 TODO: verify
	int m_unknown2264;                        // +0x2264 1
};

extern CUnleashX *g_pApp;                    // 0x00276204

// Set when the clock was invalid at boot (or by the SetClock action): the
// date/time screen is shown.
extern BOOL g_bColdBoot;                     // 0x00276208
extern BYTE g_bLaunching;                    // 0x00276236 leaving the dashboard: stop background work
extern BYTE g_bShuttingDown;                 // 0x00276235
extern DWORD g_dwShutdownDeadline;           // 0x00276238
extern HANDLE g_hExitEvent;                  // 0x0027624C
extern std::string g_strLoadingPreview;      // 0x00277F70 <Item Preview> for the GamePreview loading screen

void AddRecentItem(const char *path, const char *name, int region);   // 0x000E1988
// Shows the loading screen and starts path (an XBE); returns only on
// failure. region: the game's region (-1 = read it from the XBE).
BOOL LaunchXbe(const char *path, const char *name, int region, const char *args);   // 0x000E4691
