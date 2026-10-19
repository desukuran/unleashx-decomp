// CAppSettings: the dashboard's settings, a singleton created on first use.
// Defaults come from SetDefaults(); Config.xml values are applied by
// LoadConfigSettings() (ConfigLoader.cpp).
//
// The groups below match sub-objects the constructor initializes separately
// (each had its own inlined constructor). Member names are reconstructed;
// most come straight from the Config.xml key they're loaded from.
#pragma once

#include <string>

#include "../common/Platform.h"

struct IDirect3DTexture8;

// D3DVIEWPORT8. 0x18 bytes
struct SViewport
{
	DWORD X;
	DWORD Y;
	DWORD Width;
	DWORD Height;
	float MinZ;
	float MaxZ;
};

// Screen position/size adjustment for one video mode. 0x18 bytes
struct SScreenAdjust
{
	int x;           // +0x00 left margin
	int y;           // +0x04 top margin
	int width;       // +0x08 (modeWidth - 2*x)
	int height;      // +0x0C (modeHeight - 2*y)
	int unknown10;   // +0x10
	float scale;     // +0x14
};

enum ScreenMode
{
	SCREEN_640x480,
	SCREEN_720x480,
	SCREEN_640x576,
	SCREEN_720x576,
	SCREEN_1280x720,
	SCREEN_1920x1080,
	SCREEN_MODE_COUNT
};

enum SoundEffect
{
	SOUND_SCROLL,
	SOUND_SELECT,
	SOUND_ACTION_COMPLETE,
	SOUND_AMBIENT,
	SOUND_KEYBOARD_STROKE,
	SOUND_BACK,
	SOUND_MENU_UP,
	SOUND_MENU_DOWN,
	SOUND_GAME_SAVE_SELECT,
	SOUND_MEMORY_SLOT,
	SOUND_EFFECT_COUNT
};

enum SoundTrackMode
{
	SOUNDTRACK_NONE = 0,
	SOUNDTRACK_XBOX = 1,     // <EnableSoundTrack>Xbox
	SOUNDTRACK_CUSTOM = 2,   // <EnableSoundTrack>Custom
	SOUNDTRACK_BOTH = 3,     // <EnableSoundTrack>Both / Yes
};

// LED states, in the order of the names in g_ledColorNames.
enum LedColor
{
	LED_DEFAULT,
	LED_RED,
	LED_ORANGE,
	LED_OFF,
	LED_GREEN,
	LED_FLASHING_RED,
	LED_FLASHING_ORANGE,
	LED_FLASHING_GREEN,
	LED_GREEN_ORANGE,
	LED_ORANGE_RED,
	LED_RED_GREEN,
	LED_COLOR_COUNT
};

enum MultiRegion
{
	REGION_NONE = 0,
	REGION_NTSC = 1,    // "NTSC" / "NTSC M"
	REGION_NTSC_J = 2,
	REGION_PAL = 3,
};

// <Audio>. At CAppSettings+0x1324; continues past the named fields.
struct SAudioSettings
{
	int m_nSoundVolume;                          // +0x00 0..100
	int m_nMusicVolume;                          // +0x04 0..100
	BOOL m_bEnable51;                            // +0x08
	std::string m_strMusicPath;                  // +0x0C
	BOOL m_bRandom;                              // +0x28
	BOOL m_bGlobal;                              // +0x2C
	int m_nSoundTrackMode;                       // +0x30 SoundTrackMode
	BOOL m_bSoundEffects;                        // +0x34
	std::string m_strSound[SOUND_EFFECT_COUNT];  // +0x38

	void SetEnable51(BOOL enable);               // 0x0002B3E4 TODO: decompile
};

// Video options. At CAppSettings+0x14BC; zeroed as a block by the constructor.
struct SVideoSettings
{
	BOOL m_bPAL;                                     // +0x00 (+0x14BC) video standard is PAL
	BOOL m_bWidescreen;                              // +0x04
	BOOL m_bUsePAL60;                                // +0x08
	BOOL m_bUse480p;                                 // +0x0C
	BOOL m_bUse720p;                                 // +0x10
	BOOL m_bUse1080i;                                // +0x14
	SScreenAdjust m_screenAdjust[SCREEN_MODE_COUNT]; // +0x18
	BOOL m_bFastRefresh;                             // +0xA8 (+0x1564)
	BOOL m_bUnknownAC;                               // +0xAC
	BOOL m_bSoften;                                  // +0xB0
	int m_nFilterLevel;                              // +0xB4 0..5
	BOOL m_bUse16bit;                                // +0xB8
	BOOL m_bTextureCompression;                      // +0xBC
};

// <Network> and <HTTP>. At CAppSettings+0x15A8.
struct SNetworkSettings
{
	BOOL m_bEnable;             // +0x00
	BOOL m_bDHCP;               // +0x04
	std::string m_strIP;        // +0x08
	std::string m_strSubnet;    // +0x24
	std::string m_strGateway;   // +0x40
	std::string m_strDNS1;      // +0x5C
	std::string m_strDNS2;      // +0x78
	BOOL m_bAutoDetect;         // +0x94
	BOOL m_bUseProxy;           // +0x98 <HTTP UseProxy>
	std::string m_strProxy;     // +0x9C <HTTP Proxy>
	int m_nProxyPort;           // +0xB8 <HTTP Port>
};

// <FTP>. At CAppSettings+0x1664.
struct SFTPSettings
{
	BOOL m_bEnable;             // +0x00
	int m_nPort;                // +0x04
	int m_nMaxUsers;            // +0x08
	BOOL m_bAllowAnon;          // +0x0C
	std::string m_strGreeting;  // +0x10
	std::string m_strUser;      // +0x2C
	std::string m_strPassword;  // +0x48
	int m_nMinPasvPort;         // +0x64
	int m_nMaxPasvPort;         // +0x68
	std::string m_strSyst;      // +0x6C reply to SYST
	BOOL m_bBoost;              // +0x88
	int m_unknown8C;            // +0x8C
	int m_unknown90;            // +0x90 0x10000
	int m_unknown94;            // +0x94 1
};

// <Preference>. At CAppSettings+0x16FC.
struct SPreferences
{
	int m_nDateFormat;                  // +0x00 1. TODO: verify
	char m_chDateSeparator;             // +0x04 '/'
	int m_nTimeFormat;                  // +0x08 2. TODO: verify
	char m_chTimeSeparator;             // +0x0C ':'
	BOOL m_bGamesAutoLaunch;            // +0x10
	BOOL m_bDVDAutoLaunch;              // +0x14
	std::string m_strDVDPlayer;         // +0x18
	BOOL m_bAudioCDAutoLaunch;          // +0x34
	std::string m_strAudioCDPlayer;     // +0x38
	BOOL m_bDataAutoLaunch;             // +0x54
	std::string m_strDataLauncher;      // +0x58
	BOOL m_bSNTPSync;                   // +0x74
	std::string m_strSNTPServer;        // +0x78
	int m_nSNTPPort;                    // +0x94
	int m_unknown98;                    // +0x98 1
	std::string m_strUnknown9C;         // +0x9C
	std::string m_strSkinPath;          // +0xB8 <Skin Path=...>
	std::string m_strSkinName;          // +0xD4 <Skin>
	BOOL m_bEnableDriveF;               // +0xF0
	BOOL m_bEnableDriveG;               // +0xF4
	BOOL m_bDVDMediaCheckExtended;      // +0xF8
	BOOL m_bUseFahrenheit;              // +0xFC
	BOOL m_bAllowUninstall;             // +0x100 <ContextMenu ...>
	BOOL m_bAllowItemEdit;              // +0x104
	BOOL m_bAllowRecentList;            // +0x108
	BOOL m_bLinkToGame;                 // +0x10C
	BOOL m_bLinkToSave;                 // +0x110
	int m_nScreenSaverWait;             // +0x114 seconds
	int m_nScreenSaverType;             // +0x118
	std::string m_strScreenSaverText;   // +0x11C
	std::string m_strScreenSaverVideo;  // +0x138
	std::string m_strScreenSaverPath;   // +0x154
	BOOL m_bScreenSaverBGM;             // +0x170
	std::string m_strLanguageFile;      // +0x174
	int m_nFanSpeed;                    // +0x190 FanSpeed * 10 (10..50)
	int m_nAutoTurnOff;                 // +0x194
	BOOL m_bAutoSetTime;                // +0x198
	BOOL m_bUse24HFormat;               // +0x19C
	int m_nLEDDefault;                  // +0x1A0 LedColor; also <LEDColor>
	int m_nLEDScreenSaver;              // +0x1A4
	int m_nLEDFileCopy;                 // +0x1A8
	int m_nLEDUnknownAC;                // +0x1AC
	int m_nLEDUnknownB0;                // +0x1B0
	int m_nLEDFTPSend;                  // +0x1B4
	int m_nLEDFTPReceive;               // +0x1B8
	int m_nLEDFTPBoost;                 // +0x1BC
	int m_nLEDBusy;                     // +0x1C0
	int m_nTempThreshold;               // +0x1C4 <= 100
	BOOL m_bAutoVideoMode;              // +0x1C8
	int m_nMultiRegion;                 // +0x1CC MultiRegion
	BOOL m_bAutoVideoPAL60;             // +0x1D0
	BOOL m_bAutoRename;                 // +0x1D4
	BOOL m_bAutoPatch;                  // +0x1D8
	BOOL m_bAbortOnError;               // +0x1DC
	std::string m_strDefaultFolder;     // +0x1E0
};

// vtable 0x0025A0E8, size 0x191C
class CAppSettings
{
public:
	static CAppSettings *GetInstance();      // 0x000AD908

	CAppSettings();                          // 0x000ACE0B
	virtual ~CAppSettings();                 // 0x000311B1

	void SetDefaults();                      // 0x000AC39B
	void InitPaths(std::string xbePath);     // 0x000ACA86

	static CAppSettings *s_pInstance;        // 0x0027626C

	BYTE m_launchData[0xC00];                // +0x0004 LAUNCH_DATA
	DWORD m_dwLaunchInfoResult;              // +0x0C04 XGetLaunchInfo() return
	DWORD m_dwLaunchDataType;                // +0x0C08
	std::string m_strLaunchPath;             // +0x0C0C device path of this XBE
	std::string m_strHomePath;               // +0x0C28 DOS dir of this XBE, lowercase, ends in '\'
	std::string m_strXbeName;                // +0x0C44 file name of this XBE
	std::string m_strDataPath;               // +0x0C60 TODO: verify (dir of recent.dat)
	std::string m_strMSDashboard;            // +0x0C7C <MSDashBoard>
	BOOL m_bRunFromDVD;                      // +0x0C98
	BOOL m_bHomeNotOnD;                      // +0x0C9C our directory isn't what D: maps to. TODO: verify
	BOOL m_bNewsfeedEnable;                  // +0x0CA0
	int m_nNewsfeedInterval;                 // +0x0CA4
	BOOL m_bNewsfeedContinuous;              // +0x0CA8
	wchar_t m_szNewsfeedSeparator[5];        // +0x0CAC
	BYTE m_unknown0CB6[0x514];               // +0x0CB6
	int m_unknown11CC;                       // +0x11CC
	IDirect3DTexture8 *m_pLoadingIcon;       // +0x11D0 loading screen image of the launched item
	std::string m_strUnknown11D4;            // +0x11D4
	int m_unknown11F0;                       // +0x11F0 1
	wchar_t m_szDiscTitle[0x80];             // +0x11F4 from D:\DiscTitle.txt
	int m_nDisplayWidth;                     // +0x12F4 640. TODO: verify
	int m_nDisplayHeight;                    // +0x12F8 480
	SViewport m_viewport;                    // +0x12FC the screen area (D3DDevice_GetViewport)
	int m_nSkinWidth;                        // +0x1314 render size the skin asks for (<ScreenSize>)
	int m_nSkinHeight;                       // +0x1318
	float m_fUnknown131C;                    // +0x131C 1.0
	int m_nScrollSpeed;                      // +0x1320 10..60
	SAudioSettings m_audio;                  // +0x1324
	BYTE m_unknownAudio[0x14BC - 0x1474];    // rest of the audio block
	SVideoSettings m_video;                  // +0x14BC
	std::string m_strPassword;               // +0x157C
	int m_unknown1598;                       // +0x1598
	int m_nPasswordMaxTries;                 // +0x159C
	BOOL m_bNoPassword;                      // +0x15A0 TRUE until a Password/Passcode is set
	BOOL m_bUsePasscode;                     // +0x15A4 controller-button passcode instead of text
	SNetworkSettings m_network;              // +0x15A8
	SFTPSettings m_ftp;                      // +0x1664
	SPreferences m_prefs;                    // +0x16FC
	BOOL m_bLiveUpdateAutoCheck;             // +0x18F8
	std::string m_strLiveUpdate;             // +0x18FC
	BOOL m_bLiveUpdateWIP;                   // +0x1918
};

// The settings pointer most code uses; set by CUnleashX's constructor from
// CAppSettings::GetInstance(). 0x00276230
extern CAppSettings *g_pSettings;

// Set while the first CAppSettings is being constructed. 0x0026BBDC
extern BOOL g_bAppSettingsCreated;

extern const wchar_t g_ledColorNames[LED_COLOR_COUNT][16];  // 0x0026A4B0
