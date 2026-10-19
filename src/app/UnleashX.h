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

class CAppFrame;
class CPassCode;
class CScreenSaver;
class CAudioGUI;
class CSound;
class CXboxInfo;
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

	void SetMode(int mode);                           // 0x000E568F
	// Starts or stops the background sound (only in the menu, without
	// soundtrack music or a preview with sound).
	void PlayBackgroundSound(BOOL play);              // 0x000E045B
	void InitOrb();                                   // 0x000E0662 TODO: decompile
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
		virtual void OnPreview(const char *file);     // 0x000E4173 TODO: decompile

		CUnleashX *m_pApp;                            // +0x04
	};

	class CSkinEvent : public CSkinEventSink
	{
	public:
		virtual void OnSkinLoaded();                  // slot 2, 0x000E5824

		CUnleashX *m_pApp;                            // +0x04
	};

	CXBPackedResource m_resource;             // +0x220 UnleashX's own .xpr (icons)
	CXBFontEx m_fonts[6];                     // +0x238 0xE4 each
	CXBMesh m_mesh790;                        // +0x790 CXBMeshEx
	BYTE m_unknown7A8[0x8];                   // +0x7A8
	CXBMesh m_mesh7B0;                        // +0x7B0 CXBMeshEx
	BYTE m_unknown7C8[0x8];                   // +0x7C8
	IDirect3DTexture8 *m_pTexture7D0;         // +0x7D0
	IDirect3DTexture8 *m_pTexture7D4;         // +0x7D4
	IDirect3DTexture8 *m_pTexture7D8;         // +0x7D8
	IDirect3DTexture8 *m_pTexture7DC;         // +0x7DC
	IDirect3DTexture8 *m_pFileIcon;           // +0x7E0 File.png
	IDirect3DTexture8 *m_pFolderIcon;         // +0x7E4 Folder.png
	IDirect3DTexture8 *m_pDefaultLoadingIcon; // +0x7E8 when the skin has no loading image
	IDirect3DTexture8 *m_pTexture7EC;         // +0x7EC
	IDirect3DTexture8 *m_pTexture7F0;         // +0x7F0
	int m_unknown7F4;                         // +0x7F4
	CSound *m_pBackgroundSound;               // +0x7F8 looped in the menu
	CClockEditor *m_pClockEditor;     // +0x7FC
	CPassCode *m_pPassCode;                   // +0x800 asks for the passcode at startup
	int m_unknown804;                         // +0x804
	void *m_pDefaultGamepad;                  // +0x808 &m_defaultGamepad
	void *m_pDefaultIR;                       // +0x80C &m_defaultIR
	CXBStopWatch m_timer;                     // +0x810
	int m_nMode;                              // +0x830 AppMode
	int m_nPrevMode;                          // +0x834
	CXBStopWatch m_timers[2];                 // +0x838
	float m_unknown878;                       // +0x878 0.2
	BYTE m_unknown87C[0x144];                 // +0x87C view/projection matrices (+0x8C0, +0x900, +0x940)
	int m_unknown9C0;                         // +0x9C0
	float m_fSplashAlpha;                     // +0x9C4
	BOOL m_bSplashIsVideo;                    // +0x9C8 TODO: verify
	int m_unknown9CC;                         // +0x9CC
	IDirect3DTexture8 *m_pSplash;             // +0x9D0
	int m_unknown9D4;                         // +0x9D4
	IDirect3DTexture8 *m_pTexture9D8;         // +0x9D8
	BYTE m_unknown9DC[0x8];                   // +0x9DC
	IDirect3DTexture8 *m_pTexture9E4;         // +0x9E4
	BYTE m_unknown9E8[0x8];                   // +0x9E8
	CMenu m_menu;                             // +0x9F0
	CGameManager *m_pGameManager;             // +0x1938
	CFileManager *m_pFileManager;             // +0x193C
	CMessageBox *m_pMessageBox;               // +0x1940
	CSkinBrowser *m_pSkinBrowser;             // +0x1944
	CXboxInfo *m_pSystemInfo;                 // +0x1948 the System Info screen (mode 7)
	CNotePad *m_pTextEditor;                  // +0x194C
	BOOL m_bScreenSaverActive;                // +0x1950
	CScreenSaver *m_pScreenSaver;             // +0x1954
	CSettingsMenu *m_pSettingsMenu;           // +0x1958
	BOOL m_bMessageShown;                     // +0x195C
	BOOL m_bPasswordShown;                    // +0x1960 the keyboard asks for the password at startup
	int m_unknown1964;                        // +0x1964
	int m_unknown1968;                        // +0x1968
	BOOL m_bAutoLaunchDisc;                   // +0x196C start a disc when one is inserted
	BOOL m_bStartMusic;                       // +0x1970 create the music manager (soundtracks)
	BOOL m_bKeepAwake;                        // +0x1974 no screen saver this frame. TODO: verify
	CXBMesh m_mesh1978;                       // +0x1978
	float m_globePath[0x200];                 // +0x1990 <Main Globe>: camera heights the globe sways through
	int m_nGlobePos;                          // +0x2190 index in m_globePath
	int m_nGlobeStep;                         // +0x2194 +1 or -1
	BOOL m_bNoHardDisk;                       // +0x2198 C:\ and E:\ both missing at startup
	BYTE m_unknown219C[0x2248 - 0x219C];      // +0x219C
	HANDLE m_hThread2248;                     // +0x2248
	CMenuEvent m_menuEvent;                   // +0x224C
	CSkinEvent m_skinEvent;                   // +0x2254
	BOOL m_bEditOnYes;                        // +0x225C the message box answered Yes opens the text editor
	BOOL m_bDiscLaunchPending;                // +0x2260 TODO: verify
	BOOL m_bShowReleaseNfo;                   // +0x2264 TRUE: show the disc's Release.nfo once
};

extern CUnleashX *g_pApp;                    // 0x00276204
extern CAudioGUI *g_pAudioGUI;               // 0x00276228 the music player screen (mode 9)
extern HANDLE g_hNetworkThread;              // 0x0026A630
// Starts the network thread (NetworkThread.cpp) unless it's running.
void StartNetworkThread();                   // 0x000EC903
extern BOOL g_bUpdateAvailable;              // 0x0027620C a newer UnleashX was found
extern std::string g_strUpdateMessage;       // 0x0026C200
extern BOOL g_bAutoLaunchPending;            // 0x00276210

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
// Maps D: to the XBE's folder and launches it with launchData. TODO: decompile
DWORD LaunchXbeWithData(const char *path, LAUNCH_DATA *launchData);               // 0x000A648C
