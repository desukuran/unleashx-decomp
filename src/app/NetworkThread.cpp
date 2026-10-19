// The network thread (StartNetworkThread): brings the network up when the
// cable is plugged in, then every 2.5 seconds keeps the FTP server running,
// downloads a newsfeed a minute, checks for a newer UnleashX once, and syncs
// the clock over SNTP once.
#include <cstdlib>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../net/FtpServer.h"
#include "../net/Http.h"
#include "../net/Network.h"
#include "../net/Newsfeed.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/SkinText.h"
#include "../system/SystemInfo.h"
#include "../xml/XmlDocument.h"
#include "FtpEvents.h"
#include "UnleashX.h"

BOOL g_bUpdateAvailable;               // 0x0027620C
std::string g_strUpdateMessage;        // 0x0026C200 "UnleashX V... is available for download."
BOOL g_bAutoLaunchPending;             // 0x00276210 set by Initialize; skips newsfeed and update

static unsigned __stdcall NetworkThread(void *);

// 0x000EC903
void StartNetworkThread()
{
	if (g_hNetworkThread != INVALID_HANDLE_VALUE)
	{
		if (WaitForSingleObject(g_hNetworkThread, 100) != WAIT_OBJECT_0)
			return;   // still running
		CloseHandle(g_hNetworkThread);
		g_hNetworkThread = NULL;
	}
	g_hNetworkThread = (HANDLE)_beginthreadex(NULL, 0x10000, NetworkThread, NULL, 0, NULL);
}

// The settings strings show the configuration actually in use (DHCP
// results included): the V2 block if the MS dashboard wrote one, else V1.
static void ReadBackNetworkConfig(CAppSettings *settings)
{
	SNetworkSettings &net = settings->m_network;
	if (g_netConfig.v2Tag == XNET_CONFIG_V2_TAG)
	{
		net.m_strIP = FormatIPAddress(g_netConfig.v2Ip);
		net.m_strSubnet = FormatIPAddress(g_netConfig.v2Subnet);
		net.m_strGateway = FormatIPAddress(g_netConfig.v2Gateway);
		net.m_strDNS1 = FormatIPAddress(g_netConfig.v2Dns1);
		net.m_strDNS2 = FormatIPAddress(g_netConfig.v2Dns2);
	}
	else
	{
		net.m_strIP = FormatIPAddress(g_netConfig.v1Ip);
		net.m_strSubnet = FormatIPAddress(g_netConfig.v1Subnet);
		net.m_strGateway = FormatIPAddress(g_netConfig.v1Gateway);
		net.m_strDNS1 = FormatIPAddress(g_netConfig.v1Dns1);
		net.m_strDNS2 = FormatIPAddress(g_netConfig.v1Dns2);
	}
}

// Part of the loop: the FTP server, configured from <FTP> each time it's
// (re)started.
static void KeepFtpServerRunning(CAppSettings *settings)
{
	SFTPSettings &ftp = settings->m_ftp;
	if (!g_bFtpEnabled || !ftp.m_bEnable || g_ftpServer.IsRunning())
		return;

	if (ftp.m_nPort > 0)
		g_ftpServer.SetPort(ftp.m_nPort);
	if (ftp.m_nMaxUsers > 0)
		g_ftpServer.SetMaxUsers(ftp.m_nMaxUsers);
	if (ftp.m_strGreeting.size() != 0)
		g_ftpServer.SetWelcome(ftp.m_strGreeting.c_str());
	if (ftp.m_strUser.size() != 0)
		g_ftpServer.SetUser(StrTrimRight(StrTrimLeft(ftp.m_strUser)).c_str());
	if (ftp.m_strPassword.size() != 0)
		g_ftpServer.SetPassword(StrTrimRight(StrTrimLeft(ftp.m_strPassword)).c_str());
	g_ftpServer.m_bAllowAnonymous = ftp.m_bAllowAnon == TRUE;
	g_ftpServer.m_bBoost = ftp.m_bBoost;
	g_ftpServer.SetPasvPortMin(ftp.m_nMinPasvPort);
	g_ftpServer.SetPasvPortMax(ftp.m_nMaxPasvPort);
	g_ftpServer.SetSyst(ftp.m_strSyst.c_str());
	g_ftpServer.SetEventSink(&g_ftpEventReceiver);
	g_ftpServer.Start();
}

// Part of the loop: downloads the next newsfeed and saves it to
// <data>\Newsfeed\<file>. One feed per call; after the last one the next
// round waits until the uptime passes a growing delay (5 minutes more each
// round, 30 minutes more after an hour).
static void DownloadNextNewsfeed(int *feedIndex, DWORD *roundDelay)
{
	if (*roundDelay * 1000 >= GetTickCount())
		return;

	std::string url = g_newsfeed.GetUrl(*feedIndex);
	(*feedIndex)++;
	if (url.size() == 0)
	{
		if (*roundDelay < 3600)
			*roundDelay += 300;
		else
			*roundDelay += 1800;
		*feedIndex = 0;
		return;
	}

	std::string host;
	std::string path;
	path = "";
	std::string content;
	host = url;
	int pos = StrFind(host, ":", 0);
	if (pos != -1)
	{
		host = StrMid(host, pos + 1, host.size() - (pos + 1));
		StrReplaceAll(host, "//", "");
	}
	pos = StrFind(host, "/", 0);
	if (pos != -1)
	{
		path = StrMid(host, pos, host.size() - pos);
		host = StrLeft(host, pos);
	}
	if (host.size() == 0)
		return;

	content = HttpGetText(host.c_str(), path.c_str(), 80);
	if (content.size() == 0)
		return;
	g_newsfeed.AddFeed(url.c_str(), content.c_str());

	pos = StrReverseFind(path, "/", -1);
	if (pos != -1)
		path = StrMid(path, pos + 1, path.size() - (pos + 1));
	url = GetPersistentDataPath(L"Newsfeed", 4);
	if (StrRight(url, 1).compare("\\") != 0)
		url.append("\\");
	url.append(path);
	WriteTextFile(url.c_str(), content.c_str(), FALSE);
}

// Part of the loop: <LiveUpdate> (default http://ux.home.insightbb.com/,
// spelled out character by character so it isn't a plain string in the
// XBE) + "Version.xml". Its <Release> (and with <LiveUpdate EnableWIP>, then
// <WIP>) element has Major, Minor, Revision, Build, Type and MD5Hash
// attributes; anything newer than 0.39 build 584 with an MD5Hash sets
// g_bUpdateAvailable and g_strUpdateMessage. Returns FALSE if the dashboard
// is being left.
static BOOL CheckForUpdate(CAppSettings *settings)
{
	std::string url = settings->m_strLiveUpdate;
	if (url.size() == 0)
	{
		StrFormat(url, "http://%c%c.%c%c%c%c.%c%c%c%c%c%c%c%c%c.com/", 'u', 'x', 'h', 'o', 'm', 'e', 'i', 'n', 's',
		          'i', 'g', 'h', 't', 'b', 'b');
	}
	else
	{
		if (StrCmpNoCase(StrLeft(url, 7), "http://") != 0)
		{
			std::string withScheme;
			StrFormat(withScheme, "%s%s", "http://", url.c_str());
			url = withScheme;
		}
		if (StrRight(url, 1).compare("/") != 0)
			url.append("/");
	}
	url.append("Version.xml");

	CInternetSession session;
	session.Get(url.c_str(), TRUE);
	Sleep(250);
	while (!g_bLaunching && !session.IsDone())
		Sleep(250);
	if (g_bLaunching)
		return FALSE;

	if (session.m_nStatus == 200)
	{
		std::string text;
		if (!session.m_bSavedToFile)
			text = _strdup(session.GetData());   // Quirk: the copy is never freed
		else
			text = ReadTextFile(session.m_szFile);

		if (text.size() != 0)
		{
			CXmlDocument doc;
			if (doc.LoadFromString(text.c_str()))
			{
				BOOL release = TRUE;
				BOOL tryWip = settings->m_bLiveUpdateWIP && !settings->m_bHomeNotOnD;   // TODO: verify the second flag
				std::string channel;
				for (;;)
				{
					if (release)
						channel = "Release";
					else if (tryWip)
						channel = "WIP";
					else
						channel.clear();
					if (channel.size() == 0)
						break;

					doc.m_pCurrent = &doc.m_root;
					CXmlElement *version = doc.FindElement(doc.m_pCurrent, channel.c_str());
					if (version == NULL)
						break;
					unsigned major = atol(version->GetAttribute("Major").c_str());
					unsigned minor = atol(version->GetAttribute("Minor").c_str());
					unsigned revision = atol(version->GetAttribute("Revision").c_str());
					unsigned build = atol(version->GetAttribute("Build").c_str());
					// Quirk: "newer" is any version with a hash that isn't
					// 0.<40 with a revision below 585.
					BOOL newer = version->GetAttribute("MD5Hash").size() != 0 &&
					             !(major == 0 && minor < 40 && revision < 585);
					if (!newer)
					{
						if (!release || !tryWip)
							break;
						release = FALSE;
						continue;
					}
					StrFormat(g_strUpdateMessage, "UnleashX V%u.%02u.%04u%s Build %u is available for download.",
					          major, minor, build, version->GetAttribute("Type").c_str(), revision);
					g_bUpdateAvailable = TRUE;
					break;
				}
			}
		}
	}
	return TRUE;
}

// 0x000EAAB5
static unsigned __stdcall NetworkThread(void *)
{
	CAppSettings *settings = g_pSettings;
	if (!settings->m_network.m_bEnable)
	{
		Sleep(1000);
		_endthreadex(0);
		return 0;
	}

	BOOL timeSynced = !settings->m_prefs.m_bSNTPSync;
	DWORD nextNewsfeed = GetTickCount();
	DWORD roundDelay = 0;          // seconds of uptime before the next newsfeed round
	int feedIndex = 0;
	BOOL started = FALSE;
	BOOL firstNewsfeed = TRUE;
	BOOL checkUpdate = !settings->m_bRunFromDVD && settings->m_bLiveUpdateAutoCheck;
	g_bLaunching = FALSE;

	do
	{
		DWORD wait;
		if (!IsEthernetLinkUp())
		{
			started = FALSE;
			wait = 2000;
		}
		else if (!started)
		{
			if (!IsEthernetLinkUp())
			{
				wait = 2500;
			}
			else
			{
				// the cable is in: start the network
				started = TRUE;
				SNetworkSettings &net = settings->m_network;
				ApplyNetworkConfig(net.m_bDHCP, net.m_strIP.c_str(), net.m_strSubnet.c_str(),
				                   net.m_strGateway.c_str(), net.m_strDNS1.c_str(), net.m_strDNS2.c_str());
				StartNetwork(0);
				Sleep(5);
				while (!IsNetworkReady())
				{
					if (WaitForSingleObject(g_hExitEvent, 500) == WAIT_OBJECT_0)
					{
						StopNetwork();
						_endthreadex(0);
						return 0;
					}
					if (g_bLaunching)
					{
						_endthreadex(0);
						return 0;
					}
				}
				ReadBackNetworkConfig(settings);
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
				continue;
			}
		}
		else if (!IsNetworkReady())
		{
			started = FALSE;
			wait = 2000;
		}
		else
		{
			if (WaitForSingleObject(g_hExitEvent, 2500) == WAIT_OBJECT_0)
				break;
			if (g_ftpServer.m_bPaused)
			{
				Sleep(1000);   // a boost transfer runs
				continue;
			}

			KeepFtpServerRunning(settings);
			if (g_bAutoLaunchPending)
				continue;

			DWORD now = GetTickCount();
			if (checkUpdate && !g_bUpdateAvailable && now > 30000)
			{
				if (!CheckForUpdate(settings))
					continue;   // leaving; the loop condition ends the thread
				checkUpdate = FALSE;
			}
			else if (g_bNewsStarted && settings->m_bNewsfeedEnable && nextNewsfeed < now)
			{
				if (firstNewsfeed)
				{
					// the other <URL>s; LoadConfigSettings added the first
					g_config.m_pCurrent = &g_config.m_root;
					CXmlElement *section = g_config.FindElement(&g_config.m_root, "Settings");
					CXmlElement *newsfeed = section != NULL ? g_config.FindElement(section, "Newsfeed") : NULL;
					if (newsfeed != NULL)
					{
						std::string value;
						if (newsfeed->FindFirstChild("URL") != NULL)
						{
							CXmlElement *url;
							while ((url = newsfeed->FindNextChild()) != NULL)
							{
								value = url->m_strValue;
								if (value.size() != 0)
									g_newsfeed.AddFeed(value.c_str(), "");
							}
						}
					}
					firstNewsfeed = FALSE;
				}
				nextNewsfeed = GetTickCount() + 60000;
				DownloadNextNewsfeed(&feedIndex, &roundDelay);
			}

			if (!timeSynced && settings->m_prefs.m_bSNTPSync && (int)settings->m_prefs.m_strSNTPServer.size() > 6)
			{
				SyncTimeWithSntp(settings->m_prefs.m_strSNTPServer.c_str(), 0, settings->m_prefs.m_nSNTPPort);
				timeSynced = TRUE;
			}
			continue;
		}

		if (WaitForSingleObject(g_hExitEvent, wait) == WAIT_OBJECT_0)
			break;
	} while (!g_bLaunching);

	_endthreadex(0);
	return 0;
}
