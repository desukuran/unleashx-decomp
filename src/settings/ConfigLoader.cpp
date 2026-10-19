#include "ConfigLoader.h"

#include <cstdlib>
#include <cstring>

#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../net/Newsfeed.h"
#include "../system/Drives.h"
#include "../system/SystemControl.h"
#include "../ui/Language.h"
#include "../xml/XmlDocument.h"
#include "../xml/XmlUtil.h"
#include "AppSettings.h"

CXmlDocument g_config;         // 0x00278160
CXmlDocument g_defaultMenu;    // 0x00278040
BOOL g_bConfigLoaded;          // 0x0026A610
BOOL g_bColdBoot = TRUE;       // 0x00276208 TODO: verify initial value
CUnleashX *g_pApp;             // 0x00276204

static const char kEmptyConfig[] = "<UnleashX><Settings></Settings></UnleashX>";

// <Element>value</Element> trimmed.
static std::string TrimmedValue(CXmlElement *element)
{
	std::string value = element->GetValue();
	return StrTrimRight(StrTrimLeft(value));
}

// AutoLaunch-style flag: missing or empty means TRUE.
static BOOL DefaultTrue(const std::string &value)
{
	return value.size() == 0 || IsTrueString(value.c_str());
}

// Copies the value into dst if it parses as an IP address.
static void AssignIfIP(std::string &dst, const std::string &value)
{
	if (inet_addr(value.c_str()) != INADDR_NONE)  // inet_addr: XNet, 0x001E14FE
		dst = value;
}

// 0x000EA0A6
unsigned __stdcall ConfigLoaderThread(void *param)
{
	(void)param;
	CAppSettings *settings = g_pSettings;

	Sleep(5);
	char bootDrive = StrLeft(settings->m_strHomePath, 1)[0];
	MountDrive(bootDrive);

	// cfgcheck.dat is left behind if a previous attempt to create the config
	// didn't finish; in that case start from an empty config.
	std::string checkFile;
	ResolvePath("cfgcheck.dat", checkFile);
	if (FileExists(checkFile.c_str()) && !settings->m_bRunFromDVD)
	{
		g_bConfigLoaded = FALSE;
		SetDriveFMapped(TRUE);
		g_config.LoadFromString(kEmptyConfig);
		BuildDefaultMenu();
		DeleteFileA(checkFile.c_str());
		_endthreadex(0);
		return 0;
	}

	// "D:\config.xml" resolves to the home directory unless running from DVD.
	std::string configFile;
	configFile = "";
	ResolvePath("D:\\config.xml", configFile);
	BOOL exists = FileExists(configFile.c_str());
	if (!exists)
	{
		if (!settings->m_bRunFromDVD)
			WriteTextFile(checkFile.c_str(), " ", FALSE);

		// Where a new config will be written.
		configFile = settings->m_strHomePath;
		configFile = StrLeft(configFile, StrReverseFind(configFile, "\\"));
		if (configFile.size() == 0)
		{
			configFile = "D:\\config.xml";
		}
		else
		{
			StrAddBackslash(configFile);
			configFile += "Config.xml";
		}
	}

	BOOL loaded = FALSE;
	if (exists)
	{
		loaded = g_config.Load(configFile.c_str());
		if (!loaded)
		{
			Sleep(100);
			loaded = g_config.Load(configFile.c_str());
		}
		g_bConfigLoaded = loaded;
	}

	MountDrive('C');
	MountDrive('D');
	MountDrive('E');

	// Booting from F: or G: forces those drives on.
	if (bootDrive == 'F')
	{
		settings->m_prefs.m_bEnableDriveF = TRUE;
		SetDriveFMapped(TRUE);
		MountDrive('F');
	}
	if (bootDrive == 'G')
	{
		settings->m_prefs.m_bEnableDriveF = TRUE;
		settings->m_prefs.m_bEnableDriveG = TRUE;
		SetDriveFMapped(TRUE);
		SetDriveGMapped(TRUE);
		MountDrive('F');
		MountDrive('G');
	}

	if (loaded)
	{
		LoadConfigSettings();
		SetDriveFMapped(settings->m_prefs.m_bEnableDriveF || bootDrive == 'F');
		SetDriveGMapped(settings->m_prefs.m_bEnableDriveG || bootDrive == 'G');
	}

	if (settings->m_prefs.m_bEnableDriveF)
	{
		MountDrive('F');
		if (settings->m_prefs.m_bEnableDriveG)
			MountDrive('G');
		else
			UnmountDrive('G');
	}
	else
	{
		UnmountDrive('F');
	}

	if (!loaded)
	{
		// No usable config: build a default one, keep a copy of whatever was
		// there as BadConfig.xml, and save the new one.
		g_config.LoadFromString(kEmptyConfig);
		BuildDefaultMenu();
		std::string badConfig;
		ResolvePath("D:\\BadConfig.xml", badConfig);
		CopyFileA(configFile.c_str(), badConfig.c_str(), FALSE);
		g_config.SaveAs(configFile.c_str());
	}

	int fanSpeed = settings->m_prefs.m_nFanSpeed;
	if (fanSpeed > 10 && fanSpeed <= 50)
	{
		SetFanSpeed(fanSpeed);
		fanSpeed = settings->m_prefs.m_nFanSpeed;
		if (GetFanSpeed() != fanSpeed)
			SetFanSpeed(fanSpeed);
	}

	if (!settings->m_bRunFromDVD)
		DeleteFileA(checkFile.c_str());

	_endthreadex(0);
	return 0;
}

// The <Password> and <Passcode> elements share this. TRUE if a value was set.
static BOOL LoadPassword(CAppSettings *settings, CXmlElement *element)
{
	settings->m_strPassword = element->GetValue();
	if (settings->m_strPassword.size() == 0)
		return FALSE;
	settings->m_bUnlocked = FALSE;
	settings->m_nPasswordMaxTries = atol(element->GetAttribute("MaxTries").c_str());
	return TRUE;
}

static void LoadNetwork(CAppSettings *settings, CXmlElement *network)
{
	SNetworkSettings &net = settings->m_network;

	for (std::vector<CXmlAttribute *>::iterator it = network->m_attributes.begin();
	     it != network->m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr == NULL)
			continue;
		if (StrCmpNoCase(attr->m_strName, "Enable") == 0)
		{
			if (attr->m_strValue.size() != 0 && IsFalseString(attr->m_strValue.c_str()))
				net.m_bEnable = FALSE;
		}
		else if (StrCmpNoCase(attr->m_strName, "type") == 0 && attr->m_strValue.size() != 0 &&
		         StrCmpNoCase(attr->m_strValue, "dhcp") == 0)
		{
			net.m_bDHCP = TRUE;
		}
	}

	for (CXmlElement *child = g_config.GetFirstChild(network); child != NULL;
	     child = g_config.GetNextChild(network))
	{
		const std::string &name = child->m_strName;
		if (StrCmpNoCase(name, "IP") == 0 && child->m_strValue.size() != 0)
			AssignIfIP(net.m_strIP, child->GetValue());
		else if (StrCmpNoCase(name, "Subnet") == 0 && child->m_strValue.size() != 0)
			AssignIfIP(net.m_strSubnet, child->GetValue());
		else if (StrCmpNoCase(name, "Gateway") == 0 && child->m_strValue.size() != 0)
			AssignIfIP(net.m_strGateway, child->GetValue());
		else if (StrCmpNoCase(name, "dns1") == 0)
			AssignIfIP(net.m_strDNS1, child->GetValue());
		else if (StrCmpNoCase(name, "dns2") == 0)
			AssignIfIP(net.m_strDNS2, child->GetValue());
		else if (StrCmpNoCase(name, "AutoDetect") == 0)
			net.m_bAutoDetect = IsTrueString(child->GetValue().c_str());
	}
}

static void LoadNewsfeed(CAppSettings *settings, CXmlElement *newsfeed)
{
	std::string value = newsfeed->GetAttribute("Enable");
	if (value.size() == 0 || !IsTrueString(value.c_str()))
		return;

	settings->m_bNewsfeedEnable = TRUE;
	value = newsfeed->GetAttribute("Interval");
	settings->m_nNewsfeedInterval = atol(value.c_str());
	value = newsfeed->GetAttribute("Continuous");
	settings->m_bNewsfeedContinuous = !IsFalseString(value.c_str());
	value = newsfeed->GetAttribute("Separator");
	if (value.size() != 0)
		StrUtf8ToWide(value.c_str(), settings->m_szNewsfeedSeparator, 4);

	// Only the first <URL> is used.
	CXmlElement *url = newsfeed->FindFirstChild("URL");
	if (url != NULL)
	{
		value = url->GetValue();
		if (value.size() != 0)
			g_newsfeed.AddFeed(value.c_str(), "");
	}
}

static void LoadFTP(CAppSettings *settings, CXmlElement *ftpElement)
{
	SFTPSettings &ftp = settings->m_ftp;

	for (std::vector<CXmlAttribute *>::iterator it = ftpElement->m_attributes.begin();
	     it != ftpElement->m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr != NULL && StrCmpNoCase(attr->m_strName, "Enable") == 0)
		{
			if (IsFalseString(attr->m_strValue.c_str()))
				ftp.m_bEnable = FALSE;
			break;
		}
	}

	for (CXmlElement *child = g_config.GetFirstChild(ftpElement); child != NULL;
	     child = g_config.GetNextChild(ftpElement))
	{
		const std::string &name = child->m_strName;
		std::string value = child->GetValue();
		if (StrCmpNoCase(name, "Port") == 0)
		{
			ftp.m_nPort = atol(value.c_str());
			if (ftp.m_nPort < 1 || ftp.m_nPort > 0x10000)
				ftp.m_nPort = 21;
		}
		else if (StrCmpNoCase(name, "maxusers") == 0)
		{
			ftp.m_nMaxUsers = atol(value.c_str());
		}
		else if (StrCmpNoCase(name, "AllowAnon") == 0)
		{
			if (IsTrueString(value.c_str()))
				ftp.m_bAllowAnon = TRUE;
		}
		else if (StrCmpNoCase(name, "greeting") == 0)
		{
			ftp.m_strGreeting = value;
		}
		else if (StrCmpNoCase(name, "user") == 0 && value.size() != 0)
		{
			ftp.m_strUser = value;
		}
		else if (StrCmpNoCase(name, "password") == 0 && value.size() != 0)
		{
			ftp.m_strPassword = value;
		}
		else if (StrCmpNoCase(name, "Boost") == 0)
		{
			ftp.m_bBoost = !IsFalseString(value.c_str());
		}
		else if (StrCmpNoCase(name, "MinPASVPort") == 0)
		{
			ftp.m_nMinPasvPort = atol(value.c_str());
			if ((unsigned)ftp.m_nMinPasvPort < 0x400 || (unsigned)ftp.m_nMinPasvPort > 0x10000)
				ftp.m_nMinPasvPort = 0x400;
		}
		else if (StrCmpNoCase(name, "MaxPASVPort") == 0)
		{
			ftp.m_nMaxPasvPort = atol(value.c_str());
			if ((unsigned)ftp.m_nMaxPasvPort < 0x400 || (unsigned)ftp.m_nMaxPasvPort > 0x10000)
				ftp.m_nMaxPasvPort = 0x10000;
		}
		else if (StrCmpNoCase(name, "SYST") == 0)
		{
			ftp.m_strSyst = value;
		}
	}
}

static void LoadHTTP(CAppSettings *settings, CXmlElement *http)
{
	for (std::vector<CXmlAttribute *>::iterator it = http->m_attributes.begin(); it != http->m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr == NULL)
			continue;
		if (StrCmpNoCase(attr->m_strName, "UseProxy") == 0)
			settings->m_network.m_bUseProxy = IsTrueString(attr->m_strValue.c_str());
		else if (StrCmpNoCase(attr->m_strName, "Proxy") == 0)
			settings->m_network.m_strProxy = attr->m_strValue;
		else if (StrCmpNoCase(attr->m_strName, "Port") == 0)
			settings->m_network.m_nProxyPort = atol(attr->m_strValue.c_str());
	}
}

static void LoadVideoSettings(CAppSettings *settings, CXmlElement *element)
{
	SVideoSettings &video = settings->m_video;
	DWORD avPack = XGetAVPack();

	for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
	     it != element->m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr->m_strValue.size() == 0)
			continue;
		const std::string &name = attr->m_strName;
		const char *value = attr->m_strValue.c_str();
		if (StrCmpNoCase(name, "Widescreen") == 0)
			video.m_bWidescreen = IsTrueString(value);
		else if (StrCmpNoCase(name, "UsePAL60") == 0)
			video.m_bUsePAL60 = IsTrueString(value);
		else if (StrCmpNoCase(name, "FastRefresh") == 0)
			video.m_bFastRefresh = IsTrueString(value);
		else if (StrCmpNoCase(name, "Soften") == 0)
			video.m_bSoften = !IsFalseString(value);
		else if (StrCmpNoCase(name, "Use16bit") == 0)
			video.m_bUse16bit = IsTrueString(value);
		else if (StrCmpNoCase(name, "EnableTextureCompression") == 0)
			video.m_bTextureCompression = IsTrueString(value);
		else if (StrCmpNoCase(name, "FilterLevel") == 0)
			video.m_nFilterLevel = atol(value) % 6;
		else if (avPack & 1)  // HD modes only with an AV pack that has bit 0 set. TODO: verify meaning
		{
			if (StrCmpNoCase(name, "Use480P") == 0)
				video.m_bUse480p = IsTrueString(value);
			else if (StrCmpNoCase(name, "Use720P") == 0)
				video.m_bUse720p = IsTrueString(value);
			else if (StrCmpNoCase(name, "Use1080I") == 0)
				video.m_bUse1080i = IsTrueString(value);
		}
	}
}

static void LoadPreferences(CAppSettings *settings, CXmlElement *preference)
{
	SPreferences &prefs = settings->m_prefs;
	std::string value;

	for (CXmlElement *child = preference->GetFirstChild(); child != NULL; child = preference->GetNextChild())
	{
		const std::string &name = child->m_strName;

		if (StrCmpNoCase(name, "EnableDriveF") == 0)
		{
			value = TrimmedValue(child);
			if (IsFalseString(value.c_str()))
				prefs.m_bEnableDriveF = FALSE;
		}
		else if (StrCmpNoCase(name, "EnableDriveG") == 0)
		{
			value = TrimmedValue(child);
			prefs.m_bEnableDriveG |= IsTrueString(value.c_str());
		}
		else if (StrCmpNoCase(name, "Games") == 0)
		{
			value = child->GetAttribute("AutoLaunch");
			prefs.m_bGamesAutoLaunch = DefaultTrue(value);
		}
		else if (StrCmpNoCase(name, "DVD") == 0)
		{
			value = child->GetAttribute("AutoLaunch");
			prefs.m_bDVDAutoLaunch = DefaultTrue(value);
			value = child->GetValue();
			if (value.size() != 0)
				prefs.m_strDVDPlayer = value;
		}
		else if (StrCmpNoCase(name, "AudioCD") == 0)
		{
			value = child->GetAttribute("AutoLaunch");
			prefs.m_bAudioCDAutoLaunch = DefaultTrue(value);
			if (child->GetValue().size() == 0)
				prefs.m_strAudioCDPlayer.clear();
			else
				prefs.m_strAudioCDPlayer = child->GetValue();
		}
		else if (StrCmpNoCase(name, "Data") == 0)
		{
			value = child->GetAttribute("AutoLaunch");
			prefs.m_bDataAutoLaunch = IsTrueString(value.c_str());
			// Pointing the data launcher at the dashboard itself is ignored.
			value = settings->m_strHomePath;
			value += settings->m_strXbeName;
			if (child->GetValue().size() != 0 && StrCmpNoCase(value, child->GetValue().c_str()) != 0)
				prefs.m_strDataLauncher = child->GetValue();
		}
		else if (StrCmpNoCase(name, "SNTP") == 0)
		{
			value = child->GetAttribute("Synchronize");
			prefs.m_bSNTPSync = IsTrueString(value.c_str());
			value = child->GetValue();
			if (value.size() != 0)
			{
				AssignIfIP(prefs.m_strSNTPServer, value);
				CXmlAttribute *port = child->FindAttribute("Port");
				if (port != NULL)
				{
					prefs.m_nSNTPPort = atol(port->m_strValue.c_str());
					if (prefs.m_nSNTPPort == 0)
						prefs.m_nSNTPPort = 123;
				}
			}
		}
		else if (StrCmpNoCase(name, "FanSpeed") == 0)
		{
			double speed = strtod(child->GetValue().c_str(), NULL) * 10.0;
			if (speed >= 10.0 && speed <= 50.0)
				prefs.m_nFanSpeed = (int)speed;
		}
		else if (StrCmpNoCase(name, "ScreenSaver") == 0)
		{
			value = child->GetAttribute("Wait");
			if (value.size() != 0)
				prefs.m_nScreenSaverWait = atol(value.c_str());
			value = child->GetAttribute("EnableBGM");
			if (IsFalseString(value.c_str()))
				prefs.m_bScreenSaverBGM = FALSE;
			value = child->GetAttribute("Type");
			prefs.m_nScreenSaverType = atol(value.c_str());

			CXmlElement *sub = child->FindFirstChild("Text");
			if (sub != NULL && sub->GetValue().size() != 0)
				prefs.m_strScreenSaverText = sub->GetValue();
			sub = child->FindFirstChild("Video");
			if (sub != NULL && sub->GetValue().size() != 0)
				prefs.m_strScreenSaverVideo = sub->GetValue();
			sub = child->FindFirstChild("Path");
			if (sub != NULL && sub->GetValue().size() != 0)
				prefs.m_strScreenSaverPath = sub->GetValue();
		}
		else if (StrCmpNoCase(name, "AutoTurnOff") == 0)
		{
			value = TrimmedValue(child);
			if (value.size() != 0)
				prefs.m_nAutoTurnOff = atol(value.c_str());
		}
		else if (StrCmpNoCase(name, "UseFahrenheit") == 0)
		{
			value = TrimmedValue(child);
			prefs.m_bUseFahrenheit = value.size() != 0 && IsTrueString(value.c_str());
		}
		else if (StrCmpNoCase(name, "TempThreshold") == 0)
		{
			value = TrimmedValue(child);
			if (value.size() != 0)
				prefs.m_nTempThreshold = atol(value.c_str()) <= 100 ? atol(value.c_str()) : 100;
		}
		else if (StrCmpNoCase(name, "VideoSettings") == 0)
		{
			LoadVideoSettings(settings, child);
		}
		else if (StrCmpNoCase(name, "AutoVideoMode") == 0)
		{
			value = TrimmedValue(child);
			prefs.m_bAutoVideoMode = value.size() != 0 && IsTrueString(value.c_str());
			value = child->GetAttribute("UsePAL60");
			prefs.m_bAutoVideoPAL60 = value.size() != 0 && IsTrueString(value.c_str());
			value = child->GetAttribute("MultiRegion");
			if (value.size() != 0)
			{
				if (StrCmpNoCase(value.c_str(), "NTSC") == 0 || StrCmpNoCase(value.c_str(), "NTSC M") == 0)
					prefs.m_nMultiRegion = REGION_NTSC;
				else if (StrCmpNoCase(value.c_str(), "NTSC J") == 0)
					prefs.m_nMultiRegion = REGION_NTSC_J;
				else if (StrCmpNoCase(value.c_str(), "PAL") == 0)
					prefs.m_nMultiRegion = REGION_PAL;
			}
		}
		else if (StrCmpNoCase(name, "LEDColor") == 0)
		{
			value = TrimmedValue(child);
			if (StrCmpNoCase(value.c_str(), "Red") == 0)
				prefs.m_nLEDDefault = LED_RED;
			else if (StrCmpNoCase(value.c_str(), "Orange") == 0)
				prefs.m_nLEDDefault = LED_ORANGE;
			else if (StrCmpNoCase(value.c_str(), "Off") == 0)
				prefs.m_nLEDDefault = LED_OFF;
			else if (StrCmpNoCase(value.c_str(), "Green") == 0)
				prefs.m_nLEDDefault = LED_GREEN;
		}
		else if (StrCmpNoCase(name, "AutoSetTime") == 0)
		{
			prefs.m_bAutoSetTime = DefaultTrue(TrimmedValue(child));
		}
		else if (StrCmpNoCase(name, "Use24HFormat") == 0)
		{
			prefs.m_bUse24HFormat = IsTrueString(TrimmedValue(child).c_str());
		}
		else if (StrCmpNoCase(name, "LanguageFile") == 0)
		{
			prefs.m_strLanguageFile = TrimmedValue(child);
		}
		else if (StrCmpNoCase(name, "Skin") == 0)
		{
			value = child->GetAttribute("Path");
			if (value.size() != 0)
			{
				ResolvePath(value.c_str(), value);
				if (StrRight(value, 1).compare("\\") != 0)
					value += "\\";
				StrReplaceAll(value, "/", "\\");
				StrReplaceAll(value, "\\\\", "\\");
				prefs.m_strSkinPath = value;
			}
			value = TrimmedValue(child);
			if (value.size() != 0 && StrCmpNoCase(value.c_str(), "[Default]") != 0)
			{
				StrReplaceAll(value, "/", "\\");
				StrReplaceAll(value, "\\\\", "\\");
				prefs.m_strSkinName = value;
			}
		}
		else if (StrCmpNoCase(name, "AutoRename") == 0)
		{
			prefs.m_bAutoRename = IsTrueString(TrimmedValue(child).c_str());
		}
		else if (StrCmpNoCase(name, "AutoPatch") == 0)
		{
			prefs.m_bAutoPatch = DefaultTrue(TrimmedValue(child));
		}
		else if (StrCmpNoCase(name, "AbortOnError") == 0)
		{
			prefs.m_bAbortOnError = DefaultTrue(TrimmedValue(child));
		}
		else if (StrCmpNoCase(name, "DefaultFolder") == 0)
		{
			value = TrimmedValue(child);
			if (value.size() != 0 && DirectoryExists(value.c_str()))
				prefs.m_strDefaultFolder = value;
		}
		else if (StrCmpNoCase(name, "ContextMenu") == 0)
		{
			for (std::vector<CXmlAttribute *>::iterator it = child->m_attributes.begin();
			     it != child->m_attributes.end(); ++it)
			{
				CXmlAttribute *attr = *it;
				if (attr == NULL)
					continue;
				value = attr->m_strValue;
				const std::string &attrName = attr->m_strName;
				if (StrCmpNoCase(attrName, "AllowUninstall") == 0)
					prefs.m_bAllowUninstall = IsTrueString(value.c_str());
				else if (StrCmpNoCase(attrName, "AllowItemEdit") == 0)
					prefs.m_bAllowItemEdit = IsTrueString(value.c_str());
				else if (StrCmpNoCase(attrName, "AllowRecentList") == 0)
					prefs.m_bAllowRecentList = !IsFalseString(value.c_str());
				else if (StrCmpNoCase(attrName, "LinkToGame") == 0)
					prefs.m_bLinkToGame = !IsFalseString(value.c_str());
				else if (StrCmpNoCase(attrName, "LinkToSave") == 0)
					prefs.m_bLinkToSave = IsTrueString(value.c_str());
			}
		}
		else if (StrCmpNoCase(name, "LiveUpdate") == 0)
		{
			settings->m_strLiveUpdate = TrimmedValue(child);
			settings->m_bLiveUpdateAutoCheck = IsTrueString(child->GetAttribute("AutoCheck").c_str());
			settings->m_bLiveUpdateWIP = IsTrueString(child->GetAttribute("EnableWIP").c_str());
		}
		else if (StrCmpNoCase(name, "ScrollSpeed") == 0)
		{
			unsigned int speed = (unsigned int)atol(TrimmedValue(child).c_str());
			if (speed > 9 && speed < 61)
				settings->m_nScrollSpeed = speed;
		}
		else if (StrCmpNoCase(name, "DVDMediaCheck") == 0)
		{
			value = TrimmedValue(child);
			prefs.m_bDVDMediaCheckExtended = StrCmpNoCase(value, "Extended") == 0;
		}
	}

	if (!IsDriveFMapped())
		SetDriveFMapped(prefs.m_bEnableDriveF);
	if (IsDriveFMapped())
		IsDriveGMapped();  // result unused in the original
	SetDriveGMapped(prefs.m_bEnableDriveG);
}

static void LoadAudio(CAppSettings *settings, CXmlElement *audioElement)
{
	SAudioSettings &audio = settings->m_audio;
	static const char *const soundNames[SOUND_EFFECT_COUNT] = {
		"Scroll", "Select", "Action Complete", "Ambient", "Keyboard Stroke",
		"Back", "Menu Up", "Menu Down", "Game Save Select", "Memory Slot",
	};
	std::string value;

	for (CXmlElement *child = audioElement->GetFirstChild(); child != NULL; child = audioElement->GetNextChild())
	{
		const std::string &name = child->m_strName;

		if (StrCmpNoCase(name, "EnableSoundTrack") == 0)
		{
			value = TrimmedValue(child);
			if (value.size() != 0)
			{
				if (StrCmpNoCase(value.c_str(), "Both") == 0 || IsTrueString(value.c_str()))
					audio.m_nSoundTrackMode = SOUNDTRACK_BOTH;
				else if (StrCmpNoCase(value.c_str(), "Xbox") == 0)
					audio.m_nSoundTrackMode = SOUNDTRACK_XBOX;
				else if (StrCmpNoCase(value.c_str(), "Custom") == 0)
					audio.m_nSoundTrackMode = SOUNDTRACK_CUSTOM;
			}

			for (std::vector<CXmlAttribute *>::iterator it = child->m_attributes.begin();
			     it != child->m_attributes.end(); ++it)
			{
				CXmlAttribute *attr = *it;
				if (attr == NULL)
					continue;
				const char *attrValue = attr->m_strValue.c_str();
				if (StrCmpNoCase(attr->m_strName, "Random") == 0)
					audio.m_bRandom = IsTrueString(attrValue);
				else if (StrCmpNoCase(attr->m_strName, "MusicPath") == 0)
				{
					if (attr->m_strValue.size() != 0)
						ResolvePath(attrValue, audio.m_strMusicPath);
				}
				else if (StrCmpNoCase(attr->m_strName, "Global") == 0)
					audio.m_bGlobal = IsTrueString(attrValue);
				else if (StrCmpNoCase(attr->m_strName, "Enable_5_1") == 0)
					audio.SetEnable51(IsTrueString(attrValue));
			}
		}
		else if (StrCmpNoCase(name, "SoundVolume") == 0)
		{
			unsigned int volume = (unsigned int)atol(child->GetValue().c_str());
			if (volume <= 100)
				audio.m_nSoundVolume = volume;
			g_soundEffects.SetQuiet((unsigned int)audio.m_nSoundVolume < 5);
		}
		else if (StrCmpNoCase(name, "MusicVolume") == 0)
		{
			unsigned int volume = (unsigned int)atol(child->GetValue().c_str());
			if (volume <= 100)
				audio.m_nMusicVolume = volume;
		}
		else if (StrCmpNoCase(name, "EnableSoundEffect") == 0)
		{
			value = TrimmedValue(child);
			if (!IsFalseString(value.c_str()))
			{
				audio.m_bSoundEffects = TRUE;
				g_soundEffects.m_bDisabled = FALSE;
			}
			else
			{
				audio.m_bSoundEffects = FALSE;
				g_soundEffects.m_bDisabled = TRUE;
			}
		}
		else
		{
			// <Menu_Up>file.wav</Menu_Up> etc.; '_' stands for a space.
			value = TrimmedValue(child);
			if (value.size() != 0)
			{
				std::string effect(name);
				StrReplaceAll(effect, "_", " ");
				for (int i = 0; i < SOUND_EFFECT_COUNT; i++)
				{
					if (StrCmpNoCase(effect, soundNames[i]) == 0)
					{
						audio.m_strSound[i] = value;
						break;
					}
				}
			}
		}
	}
}

static void LoadLEDs(CAppSettings *settings, CXmlElement *led)
{
	SPreferences &prefs = settings->m_prefs;

	for (CXmlElement *child = led->GetFirstChild(); child != NULL; child = led->GetNextChild())
	{
		if (child->m_strValue.size() == 0)
			continue;
		for (int color = 0; color < LED_COLOR_COUNT; color++)
		{
			std::string colorName = StrFromWide(g_ledColorNames[color]);
			if (StrCmpNoCase(child->m_strValue, colorName.c_str()) != 0)
				continue;
			if (color != LED_DEFAULT)
			{
				const std::string &name = child->m_strName;
				if (StrCmpNoCase(name, "Default") == 0)
					prefs.m_nLEDDefault = color;
				else if (StrCmpNoCase(name, "ScreenSaver") == 0)
					prefs.m_nLEDScreenSaver = color;
				else if (StrCmpNoCase(name, "FileCopy") == 0)
					prefs.m_nLEDFileCopy = color;
				else if (StrCmpNoCase(name, "FTPSend") == 0)
					prefs.m_nLEDFTPSend = color;
				else if (StrCmpNoCase(name, "FTPReceive") == 0)
					prefs.m_nLEDFTPReceive = color;
				else if (StrCmpNoCase(name, "FTPBoost") == 0)
					prefs.m_nLEDFTPBoost = color;
				else if (StrCmpNoCase(name, "Busy") == 0)
					prefs.m_nLEDBusy = color;
			}
			break;
		}
	}
}

// 0x000E5A4F
void LoadConfigSettings()
{
	CAppSettings *settings = g_pSettings;
	CXmlElement *element;

	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *section = g_config.FindElement(&g_config.m_root, "Settings");
	if (section != NULL)
	{
		element = g_config.FindElement(section, "MSDashBoard");
		if (element != NULL && element->GetValue().size() != 0)
			settings->m_strMSDashboard = element->GetValue();

		// <Password MaxTries="n">text</Password>, or failing that a
		// controller-button <Passcode>.
		element = g_config.FindElement(section, "Password");
		if (element != NULL)
			LoadPassword(settings, element);
		if (settings->m_bUnlocked && (element = g_config.FindElement(section, "Passcode")) != NULL)
		{
			if (LoadPassword(settings, element))
				settings->m_bUsePasscode = TRUE;
		}

		element = g_config.FindElement(section, "Network");
		if (element != NULL)
			LoadNetwork(settings, element);

		if (settings->m_network.m_bEnable && (element = g_config.FindElement(section, "Newsfeed")) != NULL)
			LoadNewsfeed(settings, element);

		element = g_config.FindElement(section, "FTP");
		if (element != NULL)
			LoadFTP(settings, element);
		if (settings->m_ftp.m_strSyst.size() == 0)
			StrFormat(settings->m_ftp.m_strSyst, "%s %u.%02u.%04u%s Build %u", "UNIX emulated by UnleashX",
			          0, 39, 528, "A", 584);

		element = g_config.FindElement(section, "HTTP");
		if (element != NULL)
			LoadHTTP(settings, element);

		element = g_config.FindElement(section, "Preference");
		if (element != NULL)
			LoadPreferences(settings, element);

		element = g_config.FindElement(section, "Audio");
		if (element != NULL)
			LoadAudio(settings, element);
	}

	// Note: looked up even without <Settings> (then under the root).
	element = g_config.FindElement(section, "LED");
	if (element != NULL)
		LoadLEDs(settings, element);

	// Holding BACK while booting skips a custom language file.
	if (settings->m_prefs.m_strLanguageFile.size() != 0)
	{
		UpdateGamepads((CXBApplicationEx *)g_pApp);
		if (g_gamepadInput.GetAction() != INPUT_BACK)
			LoadLanguageFile(settings->m_prefs.m_strLanguageFile.c_str());
	}

	// AutoSetTime: on a cold boot with a clock that looks unset, take the time
	// from recent.dat's modification date.
	if (settings->m_prefs.m_bAutoSetTime)
	{
		if (g_bColdBoot)
		{
			std::string recent(settings->m_strDataPath);
			StrAddBackslash(recent);
			recent += "recent.dat";
			WIN32_FILE_ATTRIBUTE_DATA info;
			SYSTEMTIME st;
			if (GetFileAttributesExA(recent.c_str(), GetFileExInfoStandard, &info))
			{
				FileTimeToSystemTime(&info.ftLastWriteTime, &st);
				if (st.wYear > 2006 && st.wYear < 3000)
					SetXboxSystemTime(st, TRUE);
			}
		}
		g_bColdBoot = FALSE;
	}
}

// 0x000E37C7
void BuildDefaultMenu()
{
	CAppSettings *settings = g_pSettings;
	CXmlElement *root = &g_defaultMenu.m_root;
	CXmlElement *list;

	g_defaultMenu.Clear();
	g_defaultMenu.m_pCurrent = root;
	root->m_strName = "Menu";
	g_defaultMenu.m_pCurrent = root;

	AddChildElement(root, "Item", "Launch DVD", "Action=\"LaunchDVD\"");

	list = AddChildElement(root, "List", "", "Text=\"Games\"");
	if (list != NULL)
	{
		if (settings->m_bRunFromDVD)
			AddChildElement(list, "Path", "D:\\Games", NULL);
		AddChildElement(list, "Path", "E:\\Games", NULL);
		AddChildElement(list, "Path", "F:\\Games", NULL);
		AddChildElement(list, "Path", "G:\\Games", NULL);
		if (DirectoryExists("E:\\HDDLoader"))
			AddChildElement(list, "Path", "E:\\HDDLoader", NULL);
		if (DirectoryExists("F:\\HDDLoader"))
			AddChildElement(list, "Path", "F:\\HDDLoader", NULL);
	}

	list = AddChildElement(root, "List", "", "Text=\"Applications\"");
	if (list != NULL)
	{
		if (settings->m_bRunFromDVD)
		{
			AddChildElement(list, "Path", "E:\\Apps", NULL);
			AddChildElement(list, "Path", "E:\\Applications", NULL);
		}
		AddChildElement(list, "Path", "E:\\Apps", NULL);
		AddChildElement(list, "Path", "F:\\Apps", NULL);
		if (DirectoryExists("G:\\Apps"))
			AddChildElement(list, "Path", "G:\\Apps", NULL);
		if (DirectoryExists("E:\\Applications"))
			AddChildElement(list, "Path", "E:\\Applications", NULL);
		if (DirectoryExists("F:\\Applications"))
			AddChildElement(list, "Path", "F:\\Applications", NULL);
	}

	if ((DirectoryExists("E:\\Emulators") || DirectoryExists("F:\\Emulators")) &&
	    (list = AddChildElement(root, "List", "", "Text=\"Emulators\"")) != NULL)
	{
		AddChildElement(list, "Path", "E:\\Emulators", NULL);
		AddChildElement(list, "Path", "F:\\Emulators", NULL);
	}

	list = AddChildElement(root, "List", "", "Text=\"System\" Sort=\"Off\"");
	if (list != NULL)
	{
		AddChildElement(list, "Item", "Settings", "Action=\"Settings\"");
		AddChildElement(list, "Item", "Game Saves Manager", "Action=\"SavesManager\"");
		AddChildElement(list, "Item", "Clock Settings", "Action=\"SetClock\"");
		AddChildElement(list, "Item", "Skins", "Action=\"Skins\"");
		AddChildElement(list, "Item", "Copy DVD", "Action=\"CopyDVD\"");
		list = AddChildElement(list, "List", "", "Text=\"Miscellaneous\" Sort=\"Off\"");
		if (list != NULL)
		{
			AddChildElement(list, "Item", "Stop FTP Server", "Action=\"FTPStop\"");
			AddChildElement(list, "Item", "Start FTP Server", "Action=\"FTPStart\"");
			AddChildElement(list, "Item", "Reset FTP Server", "Action=\"FTPReset\"");
			AddChildElement(list, "Item", "Restart Network", "Action=\"NETReset\"");
			AddChildElement(list, "Item", "Open DVD Tray", "Action=\"TrayOpen\"");
			AddChildElement(list, "Item", "Close DVD Tray", "Action=\"TrayClose\"");
		}
	}

	const char *msDash = NULL;
	if (FileExists("C:\\xboxdash.xbe"))
		msDash = "Action=\"C:\\xboxdash.xbe\"";
	else if (FileExists("C:\\msxboxdash.xbe"))
		msDash = "Action=\"C:\\msxboxdash.xbe\"";
	if (msDash != NULL)
		AddChildElement(root, "Item", "MS Dashboard", msDash);

	AddChildElement(root, "Item", "File Explorer", "Action=\"FileManager\"");
	AddChildElement(root, "Item", "Reboot", "Action=\"Restart\"");
	AddChildElement(root, "Item", "Shutdown", "Action=\"Shutdown\"");

	g_config.m_pCurrent = &g_config.m_root;
	if (g_config.m_root.FindFirstChild("Menu") == NULL)
	{
		g_defaultMenu.m_pCurrent = root;
		g_config.m_pCurrent = &g_config.m_root;
		AppendElement(&g_config.m_root, CloneElement(root), &g_config);
	}
}
