// CUnleashX: the application. It owns the menu, the fonts and the tool
// screens, and switches between them (SetMode).
#pragma once

#include <string>

#include "../common/Platform.h"
#include "../render/D3D.h"
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
	virtual HRESULT FrameMove();                      // 0x000EB93B (FrameMove.cpp)
	virtual HRESULT Render();                         // 0x000F0097 (AppRender.cpp)
	virtual HRESULT Cleanup();                        // 0x000E1B1E TODO: decompile

	void SetMode(int mode);                           // 0x000E568F
	// Starts or stops the background sound (only in the menu, without
	// soundtrack music or a preview with sound).
	void PlayBackgroundSound(BOOL play);              // 0x000E045B
	// Fills the orb's vertex buffer: a sphere with an environment map
	// for the current view. (this in eax)
	void InitOrb();                                   // 0x000E0662
	// The orb's vertex buffer. (this in esi)
	HRESULT CreateOrbBuffer();                        // 0x000E0411
	// Fills m_globePath (a slow sway). (this in ecx)
	void BuildGlobePath();                            // 0x000DFF17 TODO: decompile
	// The startup splash (or the previous program's screen) at alpha, full
	// screen. Inlined in the original.
	void DrawSplash(float alpha, const D3DDISPLAYMODE &mode);
	// Message box with OK; text is ANSI.
	void ShowMessage(const char *text, const wchar_t *title, int flags = 0);   // 0x000E2153
	BOOL CreateTextEditor();                          // 0x000E2853
	void PrepareHardDisk();                           // 0x000E48B4 TODO: decompile
	// Starts the disc in the drive: a game, the DVD or audio CD player, the
	// data launcher or the file manager. manual: the LaunchDVD action
	// (otherwise a disc was inserted, and each kind needs its AutoLaunch
	// setting). (this in ebx, manual in edx)
	HRESULT LaunchDVD(BOOL manual);                   // 0x000E99FD
	// The screen saver and the "any input" bookkeeping: starts the saver
	// after the wait, stops it on input. (this in ebx)
	void UpdateScreenSaver(int *action);              // 0x000EA56C
	// The background: the globe, the skin's image or video, the orb and
	// the game icon. See Background.cpp.
	HRESULT RenderBackground();                       // 0x000E0E0F (this on the stack)
	// The file operation progress/error over the menu (inlined in Render).
	void RenderFileOperation();
	// Started by the updater with launch data naming the old dashboard XBE:
	// renames that to .bak and moves this one (from D:) in its place,
	// then asks to reboot. (this on the stack)
	BOOL InstallUpdate();                             // 0x000E2604

	// Menu callbacks; m_pApp points back here.
	class CMenuEvent : public CMenuEventSink
	{
	public:
		virtual BOOL OnItemSelected(CXmlElement *item);   // 0x000EE514
		virtual BOOL DrawItem(int, float *, float *, const wchar_t *, DWORD, float, BOOL, CMenuItem *, int, int)
		{
			return TRUE;
		}                                             // 0x000DFF11
		virtual void OnPreview(const char *file);     // 0x000E4173 TODO: decompile

		CUnleashX *m_pApp;                            // +0x04
	};

	class CSkinEvent : public CSkinEventSink
	{
	public:
		virtual void OnSkinLoaded();                  // slot 2, 0x000E5824
		virtual void DrawImage(int source, int left, int top, int width, int height, float opacity);   // slot 3, 0x000E1838

		CUnleashX *m_pApp;                            // +0x04
	};

	CXBPackedResource m_resource;             // +0x220 UnleashX's own .xpr (icons)
	CXBFontEx m_fonts[6];                     // +0x238 0xE4 each
	CXBMesh m_crossHairMesh;                  // +0x790 CXBMeshEx, CrossHair.xbg
	BYTE m_unknown7A8[0x8];                   // +0x7A8
	CXBMesh m_mesh7B0;                        // +0x7B0 CXBMeshEx
	BYTE m_unknown7C8[0x8];                   // +0x7C8
	IDirect3DTexture8 *m_pCellWall;           // +0x7D0 "CellWall" in resource.xpr: the globe's sky
	IDirect3DTexture8 *m_pOrb;                // +0x7D4 "Orb" in resource.xpr
	IDirect3DTexture8 *m_pTexture7D8;         // +0x7D8
	IDirect3DTexture8 *m_pEarthEnvMap;        // +0x7DC "EarthEnvMap" in resource.xpr
	IDirect3DTexture8 *m_pFileIcon;           // +0x7E0 File.png
	IDirect3DTexture8 *m_pFolderIcon;         // +0x7E4 Folder.png
	IDirect3DTexture8 *m_pDefaultLoadingIcon; // +0x7E8 when the skin has no loading image
	IDirect3DTexture8 *m_pDashCellWall;       // +0x7EC the MS dashboard's cellwall.xbx
	IDirect3DTexture8 *m_pDashOrb;            // +0x7F0 the MS dashboard's xbox4.xbx (used over Orb)
	IDirect3DTexture8 *m_pDashLogo;           // +0x7F4 the MS dashboard's xboxlogo(w).xbx
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
	DWORD m_unknown87C;                       // +0x87C
	float m_globeMatrix[16];                  // +0x880 the globe's world transform (D3DMATRIX)
	float m_worldMatrix[16];                  // +0x8C0 identity
	float m_viewMatrix[16];                   // +0x900
	float m_projMatrix[16];                   // +0x940
	BYTE m_unknown980[0x40];                  // +0x980
	int m_unknown9C0;                         // +0x9C0
	float m_fSplashAlpha;                     // +0x9C4
	BOOL m_bSplashIsScreen;                   // +0x9C8 the splash is the previous program's screen
	int m_unknown9CC;                         // +0x9CC
	IDirect3DTexture8 *m_pSplash;             // +0x9D0
	int m_unknown9D4;                         // +0x9D4
	IDirect3DTexture8 *m_pTexture9D8;         // +0x9D8
	BYTE m_unknown9DC[0x8];                   // +0x9DC
	IDirect3DVertexBuffer8 *m_pOrbVB;         // +0x9E4 the orb: 15 rings of 31 vertex pairs
	UINT m_nOrbVertices;                      // +0x9E8 930
	int m_unknown9EC;                         // +0x9EC
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
	BOOL m_bTakeScreenshot;                   // +0x1968 save the next frame as a .bmp
	BOOL m_bDiscPresent;                      // +0x196C a disc is in (MonitorThread); FrameMove autostarts it
	BOOL m_bStartMusic;                       // +0x1970 create the music manager (soundtracks)
	BOOL m_bKeepAwake;                        // +0x1974 no screen saver this frame. TODO: verify
	CXBMesh m_outerSphereMesh;                // +0x1978 OuterSphere.xbg (textured with CellWall)
	float m_globePath[0x200];                 // +0x1990 <Main Globe>: camera heights the globe sways through
	int m_nGlobePos;                          // +0x2190 index in m_globePath
	int m_nGlobeStep;                         // +0x2194 +1 or -1
	BOOL m_bNoHardDisk;                       // +0x2198 C:\ and E:\ both missing at startup
	BYTE m_unknown219C[0x2248 - 0x219C];      // +0x219C
	HANDLE m_hThread2248;                     // +0x2248
	CMenuEvent m_menuEvent;                   // +0x224C
	CSkinEvent m_skinEvent;                   // +0x2254
	BOOL m_bEditOnYes;                        // +0x225C the message box answered Yes opens the text editor
	BOOL m_bUpdatePending;                    // +0x2260 started by the Live Update installer: validate, then install
	BOOL m_bShowReleaseNfo;                   // +0x2264 TRUE: show the disc's Release.nfo once
};

extern CUnleashX *g_pApp;                    // 0x00276204
extern CXBPackedResource *g_pResource;       // 0x00276200 the app's m_resource (the media pack)
extern CAudioGUI *g_pAudioGUI;               // 0x00276228 the music player screen (mode 9)
extern HANDLE g_hNetworkThread;              // 0x0026A630
// Starts the network thread (NetworkThread.cpp) unless it's running.
void StartNetworkThread();                   // 0x000EC903
unsigned __stdcall NetworkThread(void *);    // 0x000EAAB5
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

// The pointer's cursor, fading out (alpha). The original takes x in ecx and
// y in eax. TODO: decompile
void DrawPointer(int x, int y, float alpha);         // 0x000E0950
// A free screenshotN.bmp on Z:, Y: or X: (one with 1MB free), or "".
// TODO: decompile
std::string GetScreenshotPath();                     // 0x000A3C39
extern DWORD g_dwPointerMoveTime;            // 0x0027216C

// Startup work Initialize runs on threads. TODO: decompile
// Fonts, the resource .xpr and the media; non-zero when something required
// is missing (the recovery screen follows).
unsigned __stdcall LoadResources(void *app);        // 0x000E8938
// Starts the item cache thread (0x000E9CCE) and loads the "Ambient" sound.
unsigned __stdcall StartupThread(void *app);        // 0x000EA4AA
// SMC/EEPROM/temperature monitoring for the whole run.
unsigned __stdcall MonitorThread(void *app);        // 0x000E4188
// A button held at boot runs its <Settings><Shortcut> entry; TRUE when it
// launched something. (action in eax)
BOOL RunBootShortcut(int action);                   // 0x000E87BD
// Draws the screen the previous program left (captured at startup).
void DrawSavedScreen(IDirect3DTexture8 *texture, DWORD width, DWORD height, float alpha);   // 0x0009AEB8
// Copies what the previous program left on screen into a new 640x480 texture.
// The picture the previous program (or the boot animation) left on the
// screen, as a 640x480 texture. (device isn't used.) Texture.cpp
HRESULT CaptureSavedScreen(IDirect3DTexture8 **texture, IDirect3DDevice8 *device);   // 0x0009F23D
// Text centered on x, y. TODO: decompile
void DrawTextCentered(CXBFont *font, float x, float y, const wchar_t *text, D3DCOLOR color, DWORD flags);   // 0x0002AED1
// Called by the kernel for each thread created/ended. TODO: decompile
void __stdcall ThreadNotifyRoutine(BOOL create);    // 0x000DFF8C

// Creates g_pMusicManager (on a thread; directly if that fails). See Startup.cpp.
unsigned __stdcall MusicManagerThread(void *app);  // 0x000E85B3
// Set by Render after each frame (FrameMove waits for the first one).
extern BOOL g_bFrameDrawn;                   // 0x00276218
// The orb spins this way after a menu move (+1 up, -1 down), and when.
// (Named for the globe; it's the orb that turns. See Background.cpp.)
extern float g_fGlobeSpin;                   // 0x0027621C
extern DWORD g_dwGlobeSpinTime;              // 0x00276220

void AddRecentItem(const char *path, const char *name, int region);   // 0x000E1988
// Shows the loading screen and starts path (an XBE); returns only on
// failure. region: the game's region (-1 = read it from the XBE).
BOOL LaunchXbe(const char *path, const char *name, int region, const char *args);   // 0x000E4691
// The skin's loading screen before path starts: its preview video, or the
// loading picture fading in, with "Loading..." and the title (name, else
// the XBE's) when ShowInfo is set. A or START cuts it short. See
// LaunchScreen.cpp.
void ShowLaunchScreen(const char *path, const char *name);   // 0x000E3B65
