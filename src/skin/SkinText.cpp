#include "SkinText.h"

#include <cstring>
#include <cwchar>
#include <map>
#include <string>

#include "../audio/MusicManager.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../net/Network.h"
#include "../net/Newsfeed.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/SystemControl.h"
#include "../system/SystemInfo.h"
#include "../thirdparty/XKUtils.h"
#include "../xml/XmlDocument.h"
#include "Skin.h"

// The value cache. Looking a source up with operator[] adds an empty entry,
// which is how "not read yet" is tested throughout.
static std::map<int, std::wstring> s_textCache;                         // 0x00277830
static CAppSettings *s_pSettings = CAppSettings::GetInstance();         // 0x0027783C

DWORD g_dwSkinTextTick;                // 0x00278CEC tick of the last GetSkinTextValue call
int g_nThreadCount;                    // 0x00276248 $Thread$: running threads (ThreadNotifyRoutine)
int g_nFtpClientCount;                 // 0x002785D8 $NumClient$ TODO: verify

// Refresh deadlines (GetTickCount values).
static DWORD s_dwNextDriveRefresh;     // 0x00278CE8
static DWORD s_dwNextNetworkRefresh;   // 0x00278CE4
static DWORD s_dwNextClockRefresh;     // 0x00278CE0
static DWORD s_dwNextTempRefresh;      // 0x00278CDC
static DWORD s_dwNextHddTempRefresh;   // 0x00278CD8
static DWORD s_dwNextFanRefresh;       // 0x00278CD4
static DWORD s_dwNextNewsRefresh;      // 0x00278CD0
static DWORD s_dwNextMemoryRefresh;    // 0x00278CCC
static DWORD s_dwNextSongRefresh;      // 0x00278CC8

// Newsfeed state. Each refresh shows the next headline of the current feed;
// at the end of a feed the next feed is loaded.
static int s_nNewsFeed;                // 0x00278CC4 index of the feed to load next
static bool s_bNewsFeedDone;           // 0x00278CC0 current feed has no more items
static CXmlElement *s_pNewsChannel;    // 0x00278CBC <channel> of the loaded feed
static CXmlDocument *s_pNewsDoc;       // 0x002761E8
static BOOL s_bNewsShown;              // 0x002761EC a headline has been shown
BOOL g_bNewsStarted;                   // 0x00276244
static int s_nNewsLoadedFeed = -1;     // 0x0026C6CC index of the feed in s_pNewsDoc
static wchar_t *s_pszNewsText;         // 0x002761F0 all headlines (0x401 chars)

// Hard disk info isn't read until the system has been up this long (ms).
static const DWORD kHardwareInfoDelay = 8000;

static bool IsCached(int source)
{
	return s_textCache[source].size() != 0;
}

static void SetText(int source, const wchar_t *text)
{
	s_textCache[source] = text;
}

static void ClearText(int source)
{
	s_textCache[source].clear();
}

// ANSI string to the 100-char text buffer.
static void ToWide(const std::string &s, wchar_t *out)
{
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size() + 1, out, 100);
}

// --- Drives -----------------------------------------------------------------

struct SDriveTextIds
{
	char drive;
	int free;      // bytes
	int freeMB;
	int total;     // bytes
	int totalMB;
	int pfree;     // percent
};

static const SDriveTextIds kDriveC = {'C', TEXT_C_FREE, TEXT_C_FREEMB, TEXT_C_TOTAL, TEXT_C_TOTALMB, TEXT_C_PFREE};
static const SDriveTextIds kDriveE = {'E', TEXT_E_FREE, TEXT_E_FREEMB, TEXT_E_TOTAL, TEXT_E_TOTALMB, TEXT_E_PFREE};
static const SDriveTextIds kDriveF = {'F', TEXT_F_FREE, TEXT_F_FREEMB, TEXT_F_TOTAL, TEXT_F_TOTALMB, TEXT_F_PFREE};
static const SDriveTextIds kDriveG = {'G', TEXT_G_FREE, TEXT_G_FREEMB, TEXT_G_TOTAL, TEXT_G_TOTALMB, TEXT_G_PFREE};

static void SetDriveTextsEmpty(const SDriveTextIds &ids)
{
	SetText(ids.freeMB, L"0");
	SetText(ids.free, L"0");
	SetText(ids.totalMB, L"0");
	SetText(ids.total, L"0");
	SetText(ids.pfree, L"0%");
}

// "1,234.56 MB"
static void FormatMegabytes(ULONGLONG bytes, wchar_t *out)
{
	std::string str;
	StrFormat(str, "%s", FormatDouble(2, (double)bytes * (1.0 / 1048576)).c_str());
	ToWide(str, out);
	wcscat(out, L" MB");
}

// Free space is updated on every refresh; total size and the free percentage
// only the first time.
static void SetDriveTexts(const SDriveInfo *info, const SDriveTextIds &ids)
{
	wchar_t text[101];
	FormatNumberW(text, info->freeBytes);
	SetText(ids.free, text);
	FormatMegabytes(info->freeBytes, text);
	SetText(ids.freeMB, text);

	if (!IsCached(ids.total))
	{
		FormatNumberW(text, info->totalBytes);
		SetText(ids.total, text);
		FormatMegabytes(info->totalBytes, text);
		SetText(ids.totalMB, text);
	}

	if (!IsCached(ids.pfree))
	{
		ULONGLONG percent = 0;
		if (info->totalBytes != 0)
			percent = info->freeBytes * 100 / info->totalBytes;
		FormatNumberW(text, percent);
		wcscat(text, L"%");
		SetText(ids.pfree, text);
	}
}

// Fills the texts of one drive, or zeros if it isn't mounted.
static void UpdateDrive(const SDriveTextIds &ids)
{
	SDriveInfo *info = GetDriveInfo(ids.drive);
	if (info == NULL || !info->mounted)
	{
		if (!IsCached(ids.freeMB))
			SetDriveTextsEmpty(ids);
	}
	else
	{
		SetDriveTexts(info, ids);
	}
}

static void UpdateDriveTexts()
{
	UpdateDrive(kDriveC);
	UpdateDrive(kDriveE);

	// Quirk: G: is only looked at when F: is enabled.
	if (!IsDriveFMapped())
	{
		if (!IsCached(kDriveF.freeMB))
		{
			SetDriveTextsEmpty(kDriveF);
			SetDriveTextsEmpty(kDriveG);
		}
		return;
	}
	UpdateDrive(kDriveF);

	SDriveInfo *info = NULL;
	if (IsDriveGMapped())
		info = GetDriveInfo('G');
	if (info == NULL || !info->mounted || !IsDriveGMapped())
	{
		if (!IsCached(kDriveG.freeMB))
			SetDriveTextsEmpty(kDriveG);
	}
	else
	{
		SetDriveTexts(info, kDriveG);
	}
}

// --- Network ----------------------------------------------------------------

static void UpdateNetworkTexts()
{
	wchar_t text[101];
	DWORD link = XNetGetEthernetLinkStatus();

	if (!IsNetworkReady())
	{
		if (IsEthernetLinkUp())
		{
			ToWide(GetIPAddressString(), text);
			SetText(TEXT_IP, text);
		}
		else if (IsCached(TEXT_IP))
		{
			ClearText(TEXT_IP);
		}
	}
	else
	{
		ToWide(GetIPAddressString(), text);
		SetText(TEXT_IP, text);

		const XNetConfigParams &cfg = g_netConfig;
		BOOL isV2 = cfg.v2Tag == XNET_CONFIG_V2_TAG;
		ToWide(FormatIPAddress(isV2 ? cfg.v2Subnet : cfg.v1Subnet), text);
		SetText(TEXT_SUBNET, text);
		ToWide(FormatIPAddress(isV2 ? cfg.v2Gateway : cfg.v1Gateway), text);
		SetText(TEXT_GATEWAY, text);
		ToWide(FormatIPAddress(isV2 ? cfg.v2Dns1 : cfg.v1Dns1), text);
		SetText(TEXT_DNS1, text);
		ToWide(FormatIPAddress(isV2 ? cfg.v2Dns2 : cfg.v1Dns2), text);
		SetText(TEXT_DNS2, text);
	}

	if (!(link & XNET_ETHERNET_LINK_ACTIVE))
	{
		SetText(TEXT_LINKSTATUS, L"No Link");
		return;
	}
	wcscpy(text, (link & XNET_ETHERNET_LINK_100MBPS) ? L"100 Mbps" : L"10 Mbps");
	if (link & XNET_ETHERNET_LINK_FULL_DUPLEX)
		wcscat(text, L"/Full Duplex");
	else if (link & XNET_ETHERNET_LINK_HALF_DUPLEX)
		wcscat(text, L"/Half Duplex");
	SetText(TEXT_LINKSTATUS, text);
}

// --- Temperatures -----------------------------------------------------------

// "45.00 C" / "113.50 F". Quirk: Fahrenheit is C * 1.8 + 32.5, not + 32.
static void SetTemperatureTexts(float celsius, int idC, int idF)
{
	wchar_t text[101];
	std::string str;
	StrFormat(str, "%s C", FormatDouble(2, celsius).c_str());
	ToWide(str, text);
	SetText(idC, text);
	StrFormat(str, "%s F", FormatDouble(2, celsius * 1.8f + 32.5f).c_str());
	ToWide(str, text);
	SetText(idF, text);
}

// SMART reports Celsius; values over 90 are taken to be Fahrenheit.
static void UpdateHddTemperature()
{
	float temp = (float)GetHddTemperature();
	if (temp > 0.0f)
	{
		if (temp > 90.0f)
			temp = (temp - 32.5f) * (5.0f / 9);
		SetTemperatureTexts(temp, TEXT_HDTEMPC, TEXT_HDTEMPF);
	}
	else if (IsCached(TEXT_HDTEMPF))
	{
		ClearText(TEXT_HDTEMPF);
		ClearText(TEXT_HDTEMPC);
	}
}

// --- Hardware strings -------------------------------------------------------

// "<serial>  FW: <firmware>" of the hard disk; the model goes to TEXT_HDDMODEL.
static void UpdateHddTexts()
{
	wchar_t text[101];
	char model[0x300];
	char firmware[0x100];
	memset(model, 0, sizeof(model));
	memset(firmware, 0, sizeof(firmware));
	GetIdeModel(TRUE, model, sizeof(model));
	GetIdeFirmware(TRUE, firmware, sizeof(firmware));

	std::string str;
	str = model;
	StrTrimRight(str);
	ToWide(str, text);
	SetText(TEXT_HDDMODEL, text);

	// The serial comes from the kernel, not from IDENTIFY.
	str = HalDiskSerialNumber.Buffer;
	StrTrimRight(str);
	StrFormat(str, "%s  FW: %s", str.c_str(), firmware);
	ToWide(str, text);
	SetText(TEXT_HDDSERIAL, text);
}

static void UpdateDvdTexts()
{
	wchar_t text[101];
	char model[0x180];
	char serial[0x180];
	char firmware[0x100];
	memset(model, 0, sizeof(model));
	memset(serial, 0, sizeof(serial));
	memset(firmware, 0, sizeof(firmware));
	GetIdeModel(FALSE, model, sizeof(model));
	GetIdeSerial(FALSE, serial, sizeof(serial));
	GetIdeFirmware(FALSE, firmware, sizeof(firmware));

	std::string str;
	str = model;
	StrTrimRight(str);
	if (str.size() == 0)
		str = " ";
	ToWide(str, text);
	SetText(TEXT_DVDMODEL, text);

	if (strlen(serial) == 0)
		strcpy(serial, "(Not Supported)");
	StrFormat(str, "%s  FW: %s", serial, firmware);
	ToWide(str, text);
	SetText(TEXT_DVDSERIAL, text);
}

static void UpdateHddStatusText()
{
	wchar_t text[101];
	WORD status = GetHddSecurityStatus();
	const wchar_t *state;
	if (!(status & HDD_SECURITY_SUPPORTED))
		state = L"Locking Not Supported";
	else if (!(status & HDD_SECURITY_ENABLED))
		state = L"Not Locked";
	else
		state = L"Locked";
	swprintf(text, 101, L"%ls", state);
	// Bug: the original passes the narrow string "REQUIRES RESET" for a wide
	// %s, so the text shows as garbage.
	if (status & HDD_SECURITY_COUNT_EXPIRED)
		swprintf(text, 101, L"%ls", (const wchar_t *)"REQUIRES RESET");
	SetText(TEXT_HDDSTATUS, text);
}

static void UpdateHddKeyText()
{
	wchar_t text[101];
	char buf[100];
	DWORD length;
	memset(buf, 0, sizeof(buf));
	std::string str;

	BYTE key[16];
	memcpy(key, XboxHDKey, sizeof(key));
	if (key[0] == 0)
	{
		// Not unlocked by the kernel: decrypt it from the EEPROM.
		g_eeprom.GetHDKeyString(buf, &length);
		str = buf;
	}
	else
	{
		StrFormat(str, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", key[0], key[1],
		          key[2], key[3], key[4], key[5], key[6], key[7], key[8], key[9], key[10], key[11], key[12],
		          key[13], key[14], key[15]);
	}
	swprintf(text, 101, L"%hs", str.c_str());
	SetText(TEXT_HDDKEY, text);
}

static void UpdateBiosTexts()
{
	wchar_t text[101];
	CMD5 *md5 = new CMD5(NULL, -1);
	if (md5 != NULL)
	{
		// The BIOS flash is mapped at the top 16 MB; the first MB is hashed.
		if (md5->HashMemory(BIOS_FLASH_BASE, 0x100000))
		{
			std::string str;
			StrFormat(str, "0x%s", md5->m_szHex);
			ToWide(str, text);
			SetText(TEXT_BIOSMD5, text);
			str = "Unknown";
			ToWide(str, text);
			SetText(TEXT_BIOSNAME, text);
		}
		delete md5;
	}
	if (!IsCached(TEXT_BIOSMD5))
		SetText(TEXT_BIOSMD5, L" ");
}

// --- Music ------------------------------------------------------------------

// Returns FALSE when nothing is playing (the caller then returns "").
static BOOL UpdateSongTexts()
{
	wchar_t text[101];
	wchar_t song[100];
	memset(song, 0, sizeof(song));
	g_pMusicManager->GetSongInfo(text, song, NULL);
	SetText(TEXT_SOUNDTRACK, text);
	SetText(TEXT_SONGNAME, song);
	swprintf(text, 101, L"%ls - %ls", s_textCache[TEXT_SOUNDTRACK].c_str(), s_textCache[TEXT_SONGNAME].c_str());
	SetText(TEXT_SONGTRACK, text);

	SSongFormat *format = g_pMusicManager->GetSongFormat();
	if (format == NULL || format->duration == 0)
	{
		ClearText(TEXT_SONGCHANNEL);
		ClearText(TEXT_SONGBITRATE);
		ClearText(TEXT_SONGFREQUENCY);
		ClearText(TEXT_SONGLENGTH);
		return FALSE;
	}

	const wchar_t *channels;
	if (g_pMusicManager->m_nSoundtrack == SOUNDTRACK_CD)
		channels = L"CD Stereo";
	else if (format->channels == 1)
		channels = L"Mono";
	else
		channels = L"Stereo";
	wcscpy(text, channels);
	SetText(TEXT_SONGCHANNEL, text);
	swprintf(text, 101, L"%d Kbps", format->bitRate / 1000);
	SetText(TEXT_SONGBITRATE, text);
	swprintf(text, 101, L"%.1f KHz", (double)((float)format->sampleRate * 0.001f));
	SetText(TEXT_SONGFREQUENCY, text);
	swprintf(text, 101, L"%02d:%02d", format->duration / 60, format->duration % 60);
	SetText(TEXT_SONGLENGTH, text);
	return TRUE;
}

// --- Newsfeed ---------------------------------------------------------------

// Shows the channel title as $NewsSource$. Returns FALSE if the document has
// no <channel>.
static BOOL FindNewsChannel(CXmlElement **title)
{
	s_pNewsDoc->m_pCurrent = &s_pNewsDoc->m_root;
	s_pNewsChannel = s_pNewsDoc->FindElement(&s_pNewsDoc->m_root, "Channel");
	if (s_pNewsChannel == NULL)
		return FALSE;

	wchar_t text[101];
	std::string str;
	*title = s_pNewsChannel->FindFirstChild("Title");
	if (*title == NULL)
		str = "";
	else
		str = (*title)->GetValue();
	ToWide(str, text);
	SetText(TEXT_NEWSSOURCE, text);
	return TRUE;
}

// No download yet: try the copy saved under the persistent data folder.
static BOOL LoadNewsFromDisk(std::string url, CXmlElement **item)
{
	BOOL loaded = FALSE;
	std::string path;
	int slash = StrReverseFind(url, "/");
	if (slash != -1)
		url = StrMid(url, slash + 1, (int)url.size() - (slash + 1));
	path = GetPersistentDataPath(L"Newsfeed", 4);
	if (StrRight(path, 1).compare("\\") != 0)
		path += "\\";
	path += url;

	if (!FileExists(path.c_str()))
		return FALSE;

	CXmlDocument *doc = new CXmlDocument;
	if (doc != NULL)
	{
		if (doc->Load(path.c_str()) && StrCmpNoCase(doc->m_root.m_strName, "rss") == 0)
		{
			std::string xml = doc->m_strXml;
			if (s_pNewsDoc->LoadFromString(xml.c_str()) && FindNewsChannel(item))
				loaded = TRUE;
			delete doc;
		}
		else
		{
			// Quirk: doc is leaked here.
			DeleteFileA(path.c_str());
		}
	}
	s_bNewsFeedDone = false;
	return loaded;
}

// Joins all headlines of the channel, separated by <Newsfeed Separator>,
// into s_pszNewsText (at most 0x400 chars).
static void BuildNewsText()
{
	memset(s_pszNewsText, 0, 0x800);
	CXmlElement *item = s_pNewsChannel->FindFirstChild("Item");
	if (item == NULL)
		return;
	CXmlElement *title = item->FindFirstChild("Title");
	if (title == NULL)
		return;

	std::string str = title->GetValue();
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), s_pszNewsText, 100);
	while ((item = s_pNewsChannel->GetNextChild()) != NULL)
	{
		title = item->FindFirstChild("Title");
		if (title == NULL)
			continue;
		str = title->GetValue();
		const wchar_t *separator = s_pSettings->m_szNewsfeedSeparator;
		if (str.size() + wcslen(separator) + wcslen(s_pszNewsText) > 0x400)
			break;
		wcscat(s_pszNewsText, separator);
		wchar_t headline[100];
		StrUtf8ToWide(str.c_str(), headline, 100);
		wcscat(s_pszNewsText, headline);
	}
}

static void UpdateNewsTexts(DWORD tick)
{
	if (s_pszNewsText == NULL)
		s_pszNewsText = new wchar_t[0x401];
	s_dwNextNewsRefresh = s_pSettings->m_nNewsfeedInterval * 1000 + tick;
	if (s_pNewsDoc == NULL)
	{
		s_pNewsDoc = new CXmlDocument;
		if (s_pNewsDoc == NULL)
			return;
	}

	CXmlElement *item = NULL;   // the item to show; NULL = the one after the last shown
	BOOL loaded = FALSE;
	if (s_nNewsFeed == 0 || s_bNewsFeedDone)
	{
		std::string content;
		std::string url = g_newsfeed.GetUrl(s_nNewsFeed);
		if (url.size() == 0)
		{
			// Past the last feed: start over.
			s_nNewsFeed = 0;
			url = g_newsfeed.GetUrl(s_nNewsFeed);
			if (url.size() != 0)
				content = g_newsfeed.GetContent(url.c_str());
		}
		else
		{
			content = g_newsfeed.GetContent(url.c_str());
		}

		if (content.size() == 0)
		{
			if (!g_bNewsStarted || s_bNewsFeedDone)
				loaded = LoadNewsFromDisk(url, &item);
		}
		else if (s_nNewsLoadedFeed == s_nNewsFeed)
		{
			// Same feed as before (the only one): restart at its first item.
			if (FindNewsChannel(&item))
			{
				item = s_pNewsChannel->FindFirstChild("Item");
				s_bNewsFeedDone = item == NULL;
			}
		}
		else
		{
			s_nNewsLoadedFeed = s_nNewsFeed;
			if (!s_pNewsDoc->LoadFromString(content.c_str()))
				s_bNewsFeedDone = true;
			else if (FindNewsChannel(&item))
				loaded = TRUE;
		}

		s_nNewsFeed++;
		if (loaded)
		{
			if (s_pSettings->m_bNewsfeedContinuous && s_pNewsChannel != NULL)
				BuildNewsText();
			s_pNewsChannel->GetFirstChild();
			item = s_pNewsChannel->FindFirstChild("Item");
			s_bNewsFeedDone = item == NULL;
		}
	}

	if (s_pNewsChannel != NULL)
	{
		CXmlElement *title = NULL;
		if (item == NULL)
		{
			item = s_pNewsChannel->FindNextChild();
			if (item != NULL)
			{
				s_bNewsFeedDone = false;
				title = item->FindFirstChild("Title");
			}
			else
			{
				s_bNewsFeedDone = true;
			}
		}
		else
		{
			title = item->FindFirstChild("Title");
		}

		if (title == NULL)
		{
			// Try the next feed in a second.
			s_dwNextNewsRefresh = tick + 1000;
			s_bNewsFeedDone = true;
		}
		else
		{
			wchar_t text[101];
			ToWide(title->GetValue(), text);
			SetText(TEXT_NEWSFEED, text);
			s_bNewsShown = TRUE;
		}
	}
	g_bNewsStarted = TRUE;
}

// ----------------------------------------------------------------------------

// 0x001146ED
const wchar_t *GetSkinTextValue(int source)
{
	if (source >= TEXT_SOURCE_COUNT)
		return L"";

	DWORD tick = GetTickCount();
	g_dwSkinTextTick = tick;
	wchar_t text[101];
	memset(text, 0, sizeof(text));
	std::string str;
	char buf[100];
	memset(buf, 0, sizeof(buf));
	DWORD length;

	switch (source)
	{
	case TEXT_DVDTITLE:
		return GetDiscTitle();
	case 2:
		return GetDiscTitleIfPresent();

	case TEXT_C_TOTAL: case TEXT_E_TOTAL: case TEXT_F_TOTAL:
	case TEXT_C_FREE: case TEXT_E_FREE: case TEXT_F_FREE:
	case TEXT_C_TOTALMB: case TEXT_E_TOTALMB: case TEXT_F_TOTALMB:
	case TEXT_C_FREEMB: case TEXT_E_FREEMB: case TEXT_F_FREEMB:
	case TEXT_G_TOTALMB: case TEXT_G_FREEMB: case TEXT_G_TOTAL: case TEXT_G_FREE:
	case TEXT_C_PFREE: case TEXT_E_PFREE: case TEXT_F_PFREE: case TEXT_G_PFREE:
		if (tick <= s_dwNextDriveRefresh)
			break;
		s_dwNextDriveRefresh = tick + 2000;
		UpdateDriveTexts();
		break;

	case TEXT_HOMEPATH:
		if (!IsCached(TEXT_HOMEPATH))
		{
			ToWide(s_pSettings->m_strHomePath, text);
			SetText(TEXT_HOMEPATH, text);
		}
		break;
	case TEXT_LAUNCHPATH:
		if (!IsCached(TEXT_LAUNCHPATH))
		{
			ToWide(s_pSettings->m_strLaunchPath, text);
			SetText(TEXT_LAUNCHPATH, text);
		}
		break;

	case TEXT_IP: case TEXT_LINKSTATUS:
	case TEXT_SUBNET: case TEXT_GATEWAY: case TEXT_DNS1: case TEXT_DNS2:
		if (s_dwNextNetworkRefresh < tick)
		{
			s_dwNextNetworkRefresh = tick + 2500;
			UpdateNetworkTexts();
		}
		break;

	case TEXT_TIME: case TEXT_DATE: case TEXT_DATETIME:
		if (s_dwNextClockRefresh < tick)
		{
			wchar_t time[15];
			memset(time, 0, sizeof(time));
			s_dwNextClockRefresh = tick + 1000;
			LARGE_INTEGER now;
			KeQuerySystemTime(&now);
			FormatFileTime((const FILETIME *)&now, text, time, TRUE, s_pSettings->m_prefs.m_bUse24HFormat);
			SetText(TEXT_DATE, text);
			SetText(TEXT_TIME, time);
			wcscat(text, L" ");
			wcscat(text, time);
			SetText(TEXT_DATETIME, text);
		}
		break;

	case TEXT_APPVERSION:
		if (!IsCached(TEXT_APPVERSION))
		{
			swprintf(text, 101, L"%u.%02u.%04u%hs", 0, 39, 528, "A");
			SetText(TEXT_APPVERSION, text);
		}
		break;
	case TEXT_APPVERSIONBUILD:
		if (!IsCached(TEXT_APPVERSIONBUILD))
		{
			swprintf(text, 101, L"%u.%02u.%04u%hs Build %u", 0, 39, 528, "A", 584);
			SetText(TEXT_APPVERSIONBUILD, text);
		}
		break;

	case TEXT_KERNELVERSION:
	case TEXT_BIOSVERSION:   // sic: the kernel version too
		if (!IsCached(TEXT_KERNELVERSION))
		{
			StrUtf8ToWide(GetKernelVersionString().c_str(), text, 100);
			SetText(TEXT_KERNELVERSION, text);
			SetText(TEXT_BIOSVERSION, text);
		}
		break;

	case TEXT_NICKNAME:
		if (!IsCached(TEXT_NICKNAME))
		{
			HANDLE find = XFindFirstNicknameW(FALSE, text, 100);
			if (find != INVALID_HANDLE_VALUE)
				CloseHandle(find);
			if (wcslen(text) == 0)
				wcscpy(text, L" ");
			SetText(TEXT_NICKNAME, text);
		}
		break;

	// The unit-less sources follow the Fahrenheit setting.
	case TEXT_MBTEMP:
		return GetSkinTextValue(s_pSettings->m_prefs.m_bUseFahrenheit ? TEXT_MBTEMPF : TEXT_MBTEMPC);
	case TEXT_CPUTEMP:
		return GetSkinTextValue(s_pSettings->m_prefs.m_bUseFahrenheit ? TEXT_CPUTEMPF : TEXT_CPUTEMPC);
	case TEXT_HDTEMP:
		return GetSkinTextValue(s_pSettings->m_prefs.m_bUseFahrenheit ? TEXT_HDTEMPF : TEXT_HDTEMPC);

	case TEXT_SKINNAME:
		return g_skin.m_szName;
	case TEXT_SKINVERSION:
		return g_skin.m_szVersion;
	case TEXT_SKINAUTHOR:
		return g_skin.m_szAuthor;

	case TEXT_MAC:
		if (!IsCached(TEXT_MAC) && tick > kHardwareInfoDelay)
		{
			g_eeprom.GetMACAddressString(buf, &length);
			str = buf;
			ToWide(str, text);
			SetText(TEXT_MAC, text);
		}
		break;

	case TEXT_VERSION:
		if (!IsCached(TEXT_VERSION) && tick > kHardwareInfoDelay)
		{
			GetXboxVersion(buf);
			if (strlen(buf) == 0)
				buf[0] = ' ';
			MultiByteToWideChar(CP_ACP, 0, buf, (int)strlen(buf) + 1, text, 100);
			SetText(TEXT_VERSION, text);
		}
		break;

	case TEXT_HDDKEY:
		if (!IsCached(TEXT_HDDKEY) && tick > kHardwareInfoDelay)
			UpdateHddKeyText();
		break;

	case TEXT_HDDMODEL:
	case TEXT_HDDSERIAL:
		if (!IsCached(TEXT_HDDSERIAL) && tick > 4000)
			UpdateHddTexts();
		break;

	case TEXT_HDDSTATUS:
		if (!IsCached(TEXT_HDDSTATUS) && tick > kHardwareInfoDelay)
			UpdateHddStatusText();
		break;

	case TEXT_DVDMODEL:
	case TEXT_DVDSERIAL:
		if (!IsCached(TEXT_DVDMODEL) && tick > kHardwareInfoDelay)
			UpdateDvdTexts();
		break;

	case TEXT_HDTEMPC:
	case TEXT_HDTEMPF:
		if (s_dwNextHddTempRefresh < tick)
		{
			s_dwNextHddTempRefresh = tick + 15000;
			UpdateHddTemperature();
		}
		break;

	case TEXT_SERIALNUM:
		if (!IsCached(TEXT_SERIALNUM) && tick > kHardwareInfoDelay)
		{
			g_eeprom.GetSerialNumberString(buf, &length);
			str = buf;
			ToWide(str, text);
			SetText(TEXT_SERIALNUM, text);
		}
		break;

	case TEXT_VIDEOMODE:
		if (!IsCached(TEXT_VIDEOMODE))
		{
			const wchar_t *mode;
			DWORD standard = XGetVideoStandard();
			if (standard == XC_VIDEO_STANDARD_NTSC_J)
				mode = L"NTSC J";
			else if (standard == XC_VIDEO_STANDARD_PAL_I)
				mode = (XGetVideoFlags() & XC_VIDEO_FLAGS_PAL_60Hz) ? L"PAL-60" : L"PAL";
			else
				mode = L"NTSC M";
			swprintf(text, 101, L"%ls", mode);
			SetText(TEXT_VIDEOMODE, text);
		}
		break;

	case TEXT_MBTEMPF: case TEXT_CPUTEMPF: case TEXT_MBTEMPC: case TEXT_CPUTEMPC:
		if (tick <= s_dwNextTempRefresh && IsCached(TEXT_CPUTEMPC))
			break;
		s_dwNextTempRefresh = tick + 5000;
		SetTemperatureTexts(ReadTemperature(TRUE, TRUE), TEXT_CPUTEMPC, TEXT_CPUTEMPF);
		SetTemperatureTexts(ReadTemperature(FALSE, TRUE), TEXT_MBTEMPC, TEXT_MBTEMPF);
		break;

	case TEXT_BIOSMD5:
	case TEXT_BIOSNAME:
		if (!IsCached(TEXT_BIOSMD5) && tick > kHardwareInfoDelay)
			UpdateBiosTexts();
		break;

	case TEXT_TRAY:
		{
			// The UnleashX installer CD gets its own name.
			const wchar_t *state;
			if (GetDiscType() == DISC_GAME && _wcsicmp(GetDiscTitle(), L"UnleashX Xbox Launcher") == 0)
				state = L"UnleashX CD";
			else
				state = GetTrayStateName();
			swprintf(text, 101, L"%ls", state);
			SetText(TEXT_TRAY, text);
		}
		break;

	case TEXT_DVDTYPE:
		wcscpy(text, IsDiscPresent() ? GetTrayStateName() : L"Empty");
		SetText(TEXT_DVDTYPE, text);
		break;

	case TEXT_SONGTIME:
		if (g_pMusicManager == NULL)
			return L"";
		{
			float position = g_pMusicManager->GetSongPosition();
			if (position == 0.0f)
				text[0] = 0;
			else
				swprintf(text, 101, L"%02d:%02d", (int)(position * (1.0f / 60)), (DWORD)(int)position % 60);
			SetText(TEXT_SONGTIME, text);
		}
		// Refresh the other song texts too.
		// fall through
	case TEXT_SONGNAME: case TEXT_SOUNDTRACK:
	case TEXT_SONGBITRATE: case TEXT_SONGLENGTH: case TEXT_SONGFREQUENCY: case TEXT_SONGCHANNEL: case TEXT_SONGTRACK:
		if (s_dwNextSongRefresh < tick)
		{
			s_dwNextSongRefresh = tick + 1000;
			// Quirk: "" for every song source while nothing plays, even the
			// ones just set.
			if (g_pMusicManager == NULL || !UpdateSongTexts())
				return L"";
		}
		break;

	case TEXT_NUMCLIENT:
		swprintf(text, 101, L"%d", g_nFtpClientCount);
		SetText(TEXT_NUMCLIENT, text);
		break;

	case TEXT_CHIPID:
		if (!IsCached(TEXT_CHIPID) && tick > kHardwareInfoDelay)
		{
			ToWide(GetFlashChipName(FALSE), text);
			SetText(TEXT_CHIPID, text);
		}
		break;

	case TEXT_NEWSFEED:
	case TEXT_NEWSSOURCE:
		if (!s_pSettings->m_bNewsfeedEnable || s_pSettings->m_nNewsfeedInterval == 0)
			break;
		if (s_dwNextNewsRefresh < tick || (s_bNewsShown && !IsCached(TEXT_NEWSFEED)))
			UpdateNewsTexts(tick);
		break;

	case TEXT_LAUNCHMODE:
		if (!IsCached(TEXT_LAUNCHMODE))
		{
			wcscpy(text, s_pSettings->m_bHomeNotOnD ? L"Dashboard" : L"Application");
			SetText(TEXT_LAUNCHMODE, text);
		}
		break;

	case TEXT_FANSPEED:
		if (tick <= s_dwNextFanRefresh && IsCached(TEXT_FANSPEED))
			break;
		s_dwNextFanRefresh = tick + 15000;
		StrFormat(str, "%0.02fx", (double)((float)GetFanSpeed() * 0.1f));
		ToWide(str, text);
		SetText(TEXT_FANSPEED, text);
		break;

	case TEXT_THREAD:
		swprintf(text, 101, L"%d", g_nThreadCount);
		SetText(TEXT_THREAD, text);
		break;

	case TEXT_FREEMEMORY:
	case TEXT_TOTALMEMORY:
		if (s_dwNextMemoryRefresh < tick)
		{
			s_dwNextMemoryRefresh = tick + 2500;
			MEMORYSTATUS status;
			GlobalMemoryStatus(&status);
			swprintf(text, 101, L"%u KB", status.dwAvailPhys >> 10);
			SetText(TEXT_FREEMEMORY, text);
			if (!IsCached(TEXT_TOTALMEMORY))
			{
				swprintf(text, 101, L"%u KB", status.dwTotalPhys >> 10);
				SetText(TEXT_TOTALMEMORY, text);
			}
		}
		break;
	}

	if (s_textCache.find(source) == s_textCache.end())
		return L"";
	return s_textCache[source].c_str();
}

// 0x001146A3
void InvalidateSkinText(int source)
{
	if (source >= 0 && source < TEXT_SOURCE_COUNT && s_textCache.find(source) != s_textCache.end())
		s_textCache[source].clear();
}

// 0x00114655
const wchar_t *GetNewsText()
{
	if (s_pszNewsText == NULL)
		return L"";
	return s_pszNewsText;
}

// The label of a source's line on the system info screen; NULL for the
// sources shown as their bare value.
static const wchar_t *GetInfoLabel(int source)
{
	switch (source)
	{
	case TEXT_HOMEPATH:
	case TEXT_LAUNCHPATH: return L"Path: %ls";
	case TEXT_TIME: return L"Time: %ls";
	case TEXT_APPVERSION: return L"UnleashX Version: %ls";
	case TEXT_BIOSVERSION:
	case TEXT_BIOSNAME: return L"BIOS: %ls";
	case TEXT_NICKNAME: return L"Nickname: %ls";
	case TEXT_MBTEMPF:
	case TEXT_MBTEMPC: return L"M/B: %ls";
	case TEXT_CPUTEMPF:
	case TEXT_CPUTEMPC: return L"CPU: %ls";
	case TEXT_DATE:
	case TEXT_DATETIME: return L"Date: %ls";
	case TEXT_MAC: return L"MAC: %ls";
	case TEXT_VERSION: return L"Xbox %ls";
	case TEXT_HDDKEY: return L"HDD Key: %ls";
	case TEXT_HDDMODEL: return L"HDD Model: %ls";
	case TEXT_HDDSERIAL: return L"HDD Serial No.: %ls";
	case TEXT_HDDSTATUS: return L"HDD Status: %ls";
	case TEXT_DVDMODEL: return L"DVD Model: %ls";
	case TEXT_DVDSERIAL: return L"DVD Serial: %ls";
	case TEXT_HDTEMP:
	case TEXT_HDTEMPC:
	case TEXT_HDTEMPF: return L"HDD Temp: %ls";
	case TEXT_SERIALNUM: return L"Serial No.: %ls";
	// Quirk: the MB sizes get the same labels as the plain ones.
	case TEXT_C_TOTAL:
	case TEXT_C_TOTALMB: return L"C: %ls";
	case TEXT_E_TOTAL:
	case TEXT_E_TOTALMB: return L"E: %ls";
	case TEXT_F_TOTAL:
	case TEXT_F_TOTALMB: return L"F: %ls";
	case TEXT_G_TOTAL:
	case TEXT_G_TOTALMB: return L"G: %ls";
	// (Free space and free percentage share a label too.)
	case TEXT_C_FREE:
	case TEXT_C_FREEMB:
	case TEXT_C_PFREE: return L"C: %ls free";
	case TEXT_E_FREE:
	case TEXT_E_FREEMB:
	case TEXT_E_PFREE: return L"E: %ls free";
	case TEXT_F_FREE:
	case TEXT_F_FREEMB:
	case TEXT_F_PFREE: return L"F: %ls free";
	case TEXT_G_FREE:
	case TEXT_G_FREEMB:
	case TEXT_G_PFREE: return L"G: %ls free";
	case TEXT_BIOSMD5: return L"BIOS MD5: %ls";
	case TEXT_TRAY: return L"Tray: %ls";
	case TEXT_DVDTYPE: return L"DVD: %ls";
	case TEXT_NUMCLIENT: return L"Users: %ls";
	case TEXT_GATEWAY: return L"Gateway: %ls";
	// Quirk: both DNS servers are labelled "Subnet".
	case TEXT_SUBNET:
	case TEXT_DNS1:
	case TEXT_DNS2: return L"Subnet: %ls";
	case TEXT_FANSPEED: return L"Fan: %ls";
	case TEXT_THREAD: return L"Threads: %ls";
	}
	return NULL;
}

// 0x00117FB9 (source in edx)
// Quirk: the original formats into its own stack buffer and returns that;
// the caller copies it right away. A static buffer stands in here.
const wchar_t *GetInfoText(int source)
{
	static wchar_t line[150];
	if (source > 0x59)
		return L"";
	memset(line, 0, sizeof(line));
	switch (source)
	{
	case 0x13:
	case 0x19:
	case 0x1D:
	case 0x46:
	case 0x47:
		// Unused IDs: an empty line.
		return line;
	case TEXT_IP:
	{
		const wchar_t *format = IsStaticAddress() ? L"IP: %ls Static" : IsDhcpAddress() ? L"IP: %ls DHCP" : L"IP: %ls";
		swprintf(line, 150, format, GetSkinTextValue(TEXT_IP));
		return line;
	}
	// The temperatures in the configured unit.
	case TEXT_MBTEMP:
		source = s_pSettings->m_prefs.m_bUseFahrenheit ? TEXT_MBTEMPF : TEXT_MBTEMPC;
		break;
	case TEXT_CPUTEMP:
		source = s_pSettings->m_prefs.m_bUseFahrenheit ? TEXT_CPUTEMPF : TEXT_CPUTEMPC;
		break;
	}
	const wchar_t *label = GetInfoLabel(source);
	if (label == NULL)
		return GetSkinTextValue(source);
	swprintf(line, 150, label, GetSkinTextValue(source));
	return line;
}
