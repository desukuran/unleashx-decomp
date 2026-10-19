#include "AppSettings.h"

#include <cstring>
#include <cwchar>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"

CAppSettings *CAppSettings::s_pInstance;  // 0x0027626C
CAppSettings *g_pSettings;                // 0x00276230
BOOL g_bAppSettingsCreated;               // 0x0026BBDC

// 0x0026A4B0
const wchar_t g_ledColorNames[LED_COLOR_COUNT][16] = {
	L"Default", L"Red", L"Orange", L"Off", L"Green", L"Flashing Red", L"Flashing Orange",
	L"Flashing Green", L"Green-Orange", L"Orange-Red", L"Red-Green",
};

// 0x000AD908
CAppSettings *CAppSettings::GetInstance()
{
	if (s_pInstance == NULL)
		s_pInstance = new CAppSettings();
	return s_pInstance;
}

// Sets one screen mode's margins and derives the usable size from them.
static void InitScreenAdjust(SScreenAdjust &adjust, int modeWidth, int modeHeight, float scale)
{
	adjust.width = (modeWidth / 2 - adjust.x) * 2;
	adjust.height = (modeHeight / 2 - adjust.y) * 2;
	adjust.scale = scale;
}

// 0x000ACE0B
CAppSettings::CAppSettings()
{
	if (s_pInstance != NULL)
		return;

	g_bAppSettingsCreated = TRUE;
	m_dwLaunchInfoResult = XGetLaunchInfo(&m_dwLaunchDataType, (PLAUNCH_DATA)m_launchData);
	InitPaths(GetXbePath());

	// Find out whether D: (the launch directory every title gets) is our home.
	m_bHomeNotOnD = !m_bRunFromDVD;
	if (!m_bRunFromDVD && GetFileAttributesA("D:\\") != INVALID_FILE_ATTRIBUTES)
	{
		std::string device;
		std::string prefix;
		GetDriveDevicePath('D', device);
		prefix = StrLeft(m_strLaunchPath, (int)device.size());
		if (device.size() != 0 && StrCmpNoCase(prefix, device.c_str()) == 0)
			m_bHomeNotOnD = FALSE;
	}

	memset(m_szDiscTitle, 0, sizeof(m_szDiscTitle));
	if (m_bRunFromDVD)
	{
		std::string title;
		title = "";
		const char *titleFile = "D:\\DiskTitle.txt";
		if (FileExists(titleFile) || FileExists(titleFile = "D:\\DiscTitle.txt"))
			title = ReadTextFile(titleFile);
		if (title.size() != 0)
			MultiByteToWideChar(CP_ACP, 0, title.c_str(), (int)title.size(), m_szDiscTitle, 0x80);
	}

	m_bNoPassword = TRUE;
	m_unknown1598 = 0;
	m_nPasswordMaxTries = 0;
	m_bUsePasscode = FALSE;
	m_network.m_bEnable = TRUE;
	m_network.m_bDHCP = FALSE;
	m_network.m_strIP.clear();
	m_network.m_bAutoDetect = FALSE;
	m_network.m_strSubnet.clear();
	m_bLiveUpdateWIP = FALSE;

	SetDefaults();
	m_ftp.m_strUser = "xbox";
	m_ftp.m_strPassword = "xbox";

	m_unknown11CC = 0;
	m_unknown11D0 = 0;
	memset(m_unknown0CB6, 0, sizeof(m_unknown0CB6));
	m_nDisplayWidth = 640;
	m_nDisplayHeight = 480;

	int standard = XGetVideoStandard();
	DWORD flags = XGetVideoFlags();
	memset(&m_video, 0, sizeof(m_video));
	m_video.m_bWidescreen = flags & XC_VIDEO_FLAGS_WIDESCREEN;
	m_video.m_bPAL = standard == XC_VIDEO_STANDARD_PAL_I;
	m_video.m_bUsePAL60 = m_video.m_bPAL && (flags & XC_VIDEO_FLAGS_PAL_60Hz);
	m_video.m_bUse480p = flags & XC_VIDEO_FLAGS_HDTV_480p;
	m_video.m_bUse720p = flags & XC_VIDEO_FLAGS_HDTV_720p;
	m_video.m_bUse1080i = flags & XC_VIDEO_FLAGS_HDTV_1080i;
	m_video.m_bFastRefresh = TRUE;
	m_video.m_bUnknownAC = FALSE;

	// Default overscan margins per mode. When running from DVD the original
	// means to use 0 (later 5) margins, but every branch writes mode 0's fields,
	// so the other modes keep 0 from the memset above and mode 0 ends up 5,5.
	SScreenAdjust *adj = m_video.m_screenAdjust;
	BOOL dvd = m_bRunFromDVD;
	if (!dvd) { adj[SCREEN_640x480].x = 15; adj[SCREEN_640x480].y = 10; }
	else      { adj[SCREEN_640x480].x = 0;  adj[SCREEN_640x480].y = 0; }
	InitScreenAdjust(adj[SCREEN_640x480], 640, 480, 1.0f);

	if (!dvd) { adj[SCREEN_720x480].x = 15; adj[SCREEN_720x480].y = 10; }
	else      { adj[SCREEN_640x480].x = 0;  adj[SCREEN_640x480].y = 0; }
	InitScreenAdjust(adj[SCREEN_720x480], 720, 480, 1.0f);

	if (!dvd) { adj[SCREEN_640x576].x = 15; adj[SCREEN_640x576].y = 10; }
	else      { adj[SCREEN_640x480].x = 5;  adj[SCREEN_640x480].y = 5; }
	InitScreenAdjust(adj[SCREEN_640x576], 640, 576, 1.0f);

	if (!dvd) { adj[SCREEN_720x576].x = 15; adj[SCREEN_720x576].y = 10; }
	else      { adj[SCREEN_640x480].x = 5;  adj[SCREEN_640x480].y = 5; }
	InitScreenAdjust(adj[SCREEN_720x576], 720, 576, 1.0f);

	if (!dvd) { adj[SCREEN_1280x720].x = 20; adj[SCREEN_1280x720].y = 10; }
	else      { adj[SCREEN_640x480].x = 5;   adj[SCREEN_640x480].y = 5; }
	InitScreenAdjust(adj[SCREEN_1280x720], 1280, 720, 1.5f);

	if (!dvd) { adj[SCREEN_1920x1080].x = 25; adj[SCREEN_1920x1080].y = 10; }
	else      { adj[SCREEN_640x480].x = 5;    adj[SCREEN_640x480].y = 5; }
	InitScreenAdjust(adj[SCREEN_1920x1080], 1920, 1080, 2.0f);

	m_nSkinWidth = 640;
	m_nSkinHeight = 480;
}

// 0x000311B1
CAppSettings::~CAppSettings()
{
}

// 0x000AC39B
void CAppSettings::SetDefaults()
{
	int i;

	// <HTTP>
	m_network.m_strProxy.clear();
	m_ftp.m_bEnable = m_network.m_bEnable;
	m_network.m_nProxyPort = 1080;
	m_network.m_bUseProxy = FALSE;

	// <FTP>
	m_ftp.m_nPort = 21;
	m_ftp.m_nMaxUsers = 2;
	m_ftp.m_bAllowAnon = FALSE;
	m_ftp.m_nMinPasvPort = 1024;
	m_ftp.m_nMaxPasvPort = 0x10000;
	m_ftp.m_strSyst.clear();
	m_ftp.m_unknown90 = 0x10000;
	m_ftp.m_bBoost = TRUE;
	m_ftp.m_unknown8C = 0;
	m_ftp.m_unknown94 = 1;

	StrFormat(m_strMSDashboard, "C:%s", "\\XboxDash.xbe");

	// <Preference>
	m_prefs.m_nDateFormat = 1;
	m_prefs.m_chDateSeparator = '/';
	m_prefs.m_bGamesAutoLaunch = TRUE;
	m_prefs.m_bDVDAutoLaunch = TRUE;
	m_prefs.m_bAudioCDAutoLaunch = TRUE;
	m_prefs.m_bDataAutoLaunch = FALSE;
	m_prefs.m_bSNTPSync = FALSE;
	m_prefs.m_unknown98 = 1;
	m_prefs.m_nSNTPPort = 123;
	m_prefs.m_strAudioCDPlayer.clear();
	m_prefs.m_strDVDPlayer = m_strMSDashboard;
	m_prefs.m_strDataLauncher.clear();
	m_prefs.m_nTimeFormat = 2;
	m_prefs.m_chTimeSeparator = ':';
	m_prefs.m_bEnableDriveF = TRUE;
	m_prefs.m_bEnableDriveG = FALSE;
	m_prefs.m_bDVDMediaCheckExtended = FALSE;
	m_prefs.m_bUseFahrenheit = FALSE;
	m_prefs.m_bAllowUninstall = FALSE;
	m_prefs.m_bAllowItemEdit = FALSE;
	m_prefs.m_bAllowRecentList = TRUE;
	m_prefs.m_bLinkToGame = TRUE;
	m_prefs.m_bLinkToSave = FALSE;
	m_prefs.m_nScreenSaverWait = 60;
	m_prefs.m_nScreenSaverType = 0;
	m_prefs.m_strScreenSaverText.clear();
	m_prefs.m_strScreenSaverVideo.clear();
	m_prefs.m_strScreenSaverPath.clear();
	m_prefs.m_bScreenSaverBGM = TRUE;
	m_prefs.m_strLanguageFile.clear();
	m_prefs.m_nFanSpeed = 10;
	m_prefs.m_nAutoTurnOff = 0;
	m_prefs.m_bAutoSetTime = TRUE;
	m_prefs.m_bUse24HFormat = FALSE;
	m_prefs.m_strSkinPath = "Skins";
	m_prefs.m_strSkinName = "";
	m_prefs.m_strSNTPServer = "time-a.nist.gov";
	m_prefs.m_nSNTPPort = 37;  // overrides the 123 above (TIME instead of NTP)
	m_prefs.m_nLEDDefault = LED_DEFAULT;
	m_prefs.m_nLEDScreenSaver = LED_DEFAULT;
	m_prefs.m_nLEDFileCopy = LED_DEFAULT;
	m_prefs.m_nLEDUnknownAC = LED_DEFAULT;
	m_prefs.m_nLEDUnknownB0 = LED_DEFAULT;
	m_prefs.m_nLEDFTPSend = LED_DEFAULT;
	m_prefs.m_nLEDFTPReceive = LED_DEFAULT;
	m_prefs.m_nLEDFTPBoost = LED_DEFAULT;
	m_prefs.m_nLEDBusy = LED_DEFAULT;
	m_prefs.m_nTempThreshold = 0;
	m_prefs.m_bAutoVideoMode = FALSE;
	m_prefs.m_bAutoVideoPAL60 = FALSE;
	m_prefs.m_nMultiRegion = REGION_NONE;
	m_prefs.m_bAutoRename = FALSE;
	m_prefs.m_bAutoPatch = TRUE;
	m_prefs.m_bAbortOnError = TRUE;
	m_prefs.m_strDefaultFolder.clear();

	m_unknown11F0 = 1;

	// <Newsfeed>
	m_bNewsfeedEnable = FALSE;
	m_nNewsfeedInterval = 30;
	m_bNewsfeedContinuous = TRUE;
	wcsncpy(m_szNewsfeedSeparator, L" | ", 3);

	m_fUnknown131C = 1.0f;
	m_nScrollSpeed = 20;

	// <VideoSettings>
	m_video.m_bSoften = TRUE;
	m_video.m_nFilterLevel = 2;
	m_video.m_bUse16bit = FALSE;
	m_video.m_bTextureCompression = FALSE;

	// <LiveUpdate>
	m_bLiveUpdateAutoCheck = FALSE;
	m_strLiveUpdate.clear();

	// <Audio>
	static const char *const defaultSounds[SOUND_EFFECT_COUNT] = {
		"C:\\Audio\\MainAudio\\Global Scroll Beep.wav",
		"C:\\Audio\\MainAudio\\Global A Button Select.wav",
		"C:\\Audio\\MainAudio\\Global Completion Beep.wav",
		"C:\\Audio\\AmbientAudio\\AMB_05_ENGINEROOM_LR.wav",
		"C:\\Audio\\MainAudio\\Global Keyboard Stroke 1.wav",
		"C:\\Audio\\MainAudio\\Global B Button Back.wav",
		"C:\\Audio\\MainAudio\\Global Main MenuBack3ver2.wav",
		"C:\\Audio\\MainAudio\\Global Main MenuFwd3ver2.wav",
		"C:\\Audio\\MemoryAudio\\Memory Games Select.wav",
		"C:\\Audio\\MemoryAudio\\Memory Memory Slot Select.wav",
	};
	m_audio.m_nSoundVolume = 90;
	m_audio.m_nMusicVolume = 90;
	m_audio.m_bEnable51 = FALSE;
	m_audio.m_strMusicPath = "F:\\Music";
	m_audio.m_bRandom = FALSE;
	m_audio.m_bGlobal = FALSE;
	m_audio.m_nSoundTrackMode = SOUNDTRACK_NONE;
	m_audio.m_bSoundEffects = TRUE;
	for (i = 0; i < SOUND_EFFECT_COUNT; i++)
		m_audio.m_strSound[i] = defaultSounds[i];
}

// 0x000ACA86
// Works out where this XBE lives, both as a device path (m_strLaunchPath)
// and as a DOS path (m_strHomePath + m_strXbeName). xbePath is the device
// path the kernel loaded us from (XeImageFileName).
void CAppSettings::InitPaths(std::string xbePath)
{
	m_bRunFromDVD = FALSE;

	if (xbePath.size() == 0)
	{
		// No image name: assume we replaced the MS dashboard.
		const char *device;
		if (IsDriveAvailable('C'))
			device = "\\Device\\Harddisk0\\Partition2";
		else if (IsDriveAvailable('D'))
			device = "\\Device\\Cdrom0";
		else
			goto convert;
		m_strLaunchPath = device;
		m_strLaunchPath += "\\XboxDash.xbe";
	}
	else
	{
		// Normalize the device prefix's capitalization.
		std::string rest;
		const char *device;
		if (StrCmpNoCase(StrLeft(xbePath, 13).c_str(), "\\Device\\CdRom") == 0)
		{
			rest = StrMid(xbePath, 13, (int)xbePath.size() - 13);
			device = "\\Device\\CdRom";
		}
		else
		{
			rest = StrMid(xbePath, 27, (int)xbePath.size() - 27);
			device = "\\Device\\Harddisk0\\Partition";
		}
		xbePath = device + rest;
		m_strLaunchPath = xbePath;
	}

convert:
	std::string path(m_strLaunchPath);
	if (m_strLaunchPath.size() == 0)
		return;

	// Map the device path to a drive letter.
	StrReplaceAll(m_strLaunchPath, "Cdrom", "CdRom");
	path = m_strLaunchPath;
	if (StrFind(m_strLaunchPath, ":") == 0)
	{
		path = m_strLaunchPath;
	}
	else if (StrFind(m_strLaunchPath, "\\Device\\Harddisk0\\Partition2") == 0)
	{
		StrReplaceAll(path, "\\Device\\Harddisk0\\Partition2", "C:");
	}
	else if (StrFind(m_strLaunchPath, "\\Device\\Cdrom0") == 0 ||
	         StrFind(m_strLaunchPath, "\\Device\\CdRom0") == 0)
	{
		StrReplaceAll(path, "\\Device\\Cdrom0", "D:");
		StrReplaceAll(path, "\\Device\\CdRom0", "D:");
		StrReplaceAll(path, "\\Device\\Cdrom0", "D:");
		m_bRunFromDVD = TRUE;
	}
	else
	{
		static const char *const partitions[][2] = {
			{"\\Device\\Harddisk0\\Partition1", "E:"},
			{"\\Device\\Harddisk0\\Partition6", "F:"},
			{"\\Device\\Harddisk0\\Partition7", "G:"},
			{"\\Device\\Harddisk0\\Partition3", "X:"},
			{"\\Device\\Harddisk0\\Partition4", "Y:"},
			{"\\Device\\Harddisk0\\Partition5", "Z:"},
		};
		for (size_t i = 0; i < sizeof(partitions) / sizeof(partitions[0]); i++)
		{
			if (StrFind(m_strLaunchPath, partitions[i][0]) == 0)
			{
				StrReplaceAll(path, partitions[i][0], partitions[i][1]);
				break;
			}
		}
	}

	StrReplaceAll(path, "/", "\\");
	int slash = StrReverseFind(path, "\\");
	if (slash == -1)
	{
		m_strHomePath = path;
	}
	else
	{
		slash++;
		// The original lowercases a _strdup() copy and never frees it.
		m_strHomePath = StrToLower(StrLeft(path, slash));
		m_strXbeName = StrMid(path, slash, (int)path.size() - slash);
	}
	StrAddBackslash(m_strHomePath);
}
