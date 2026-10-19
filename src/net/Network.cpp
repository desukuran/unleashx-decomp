#include "Network.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../common/StrUtil.h"

BYTE g_bNetworkStarted = 0;
BOOL g_bNetworkStopping = FALSE;
HANDLE g_hNetworkEvent = NULL;
DWORD g_dwIPAddress = 0;
XNetConfigParams g_netConfig;
XNADDR g_xnAddr;
DWORD g_dwXnAddrStatus = XNET_GET_XNADDR_NONE;
std::vector<SNetClient> g_netClients;

static BOOL s_bLinkMonitorStarted = FALSE;   // 0x00278D78
static DWORD s_dwNextClientScan = 0;         // 0x00278D7C

static const WORD kClientScanPort = 4905;

static void InitNetworkStack();

// 0x000AEB5B
HRESULT StopNetwork()
{
	if (!g_bNetworkStarted)
		return 1;   // S_FALSE

	if (g_hNetworkEvent != NULL)
		SetEvent(g_hNetworkEvent);
	g_bNetworkStopping = TRUE;
	g_bNetworkStarted = FALSE;
	Sleep(500);
	int wsa = WSACleanup();
	int xnet = XNetCleanup();
	g_dwIPAddress = 0;
	g_dwXnAddrStatus = XNET_GET_XNADDR_NONE;
	if (g_hNetworkEvent != NULL)
	{
		CloseHandle(g_hNetworkEvent);
		g_hNetworkEvent = NULL;
	}
	return (wsa == 0 && xnet == 0) ? S_OK : E_FAIL;
}

// 0x000AEBF9
// No address yet (pending or none), or a status value outside the normal
// range.
BOOL HasNoNetworkAddress()
{
	if (g_dwXnAddrStatus > 3 && g_dwXnAddrStatus <= XNET_GET_XNADDR_ONLINE)
		return FALSE;
	return TRUE;
}

// 0x000AEC23
BOOL IsDhcpAddress()
{
	return g_dwXnAddrStatus & XNET_GET_XNADDR_DHCP;
}

// 0x000AEC2C
BOOL IsStaticAddress()
{
	return g_dwXnAddrStatus & XNET_GET_XNADDR_STATIC;
}

// 0x000AEC35
DWORD GetIPAddress()
{
	return g_dwIPAddress;
}

// 0x000AEC3B
BOOL IsNetworkReady()
{
	return g_bNetworkStarted && !HasNoNetworkAddress();
}

// 0x000AEC52
BOOL IsEthernetLinkUp()
{
	return (XNetGetEthernetLinkStatus() & XNET_ETHERNET_LINK_ACTIVE) != 0;
}

// 0x000AEC63
// Stops the stack while the cable is pulled and restarts it when it's back,
// polling every second (1.5 s while unplugged).
// Quirk: the thread ends as soon as the link is up and the network is ready,
// or one poll after a restart attempt, and is never started again (see
// StartLinkMonitor), so in practice it only covers the time until the first
// address.
static unsigned __stdcall LinkMonitorThread(void *)
{
	BOOL restarted = FALSE;
	DWORD wait = 1000;
	while (!g_bNetworkStopping)
	{
		if (WaitForSingleObject(g_hNetworkEvent, wait) == WAIT_OBJECT_0)
			break;
		if (!IsEthernetLinkUp())
		{
			if (IsNetworkReady())
			{
				StopNetwork();
				g_bNetworkStopping = FALSE;
			}
			wait = 1500;
			restarted = FALSE;
			continue;
		}
		if (restarted || IsNetworkReady())
			break;
		restarted = TRUE;
		Sleep(100);
		StartNetwork(0);
		wait = 1000;
	}
	return 0;
}

// 0x000AECD0
static void StartLinkMonitor()
{
	if (s_bLinkMonitorStarted)
		return;
	s_bLinkMonitorStarted = TRUE;
	unsigned threadId;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, LinkMonitorThread, NULL, 0, &threadId);
	if (thread != NULL && thread != INVALID_HANDLE_VALUE)
	{
		SetThreadPriority(thread, THREAD_PRIORITY_LOWEST);
		Sleep(5);
		CloseHandle(thread);
	}
}

// Parses one address for ApplyNetworkConfig and stores it in the V1 or V2
// field. Returns TRUE if it changed.
static BOOL SetConfigAddress(const std::string &text, DWORD &v1, DWORD &v2, BOOL isV2)
{
	DWORD &field = isV2 ? v2 : v1;
	DWORD addr = inet_addr(text.c_str());
	if (addr == INADDR_NONE || field == addr)
		return FALSE;
	field = addr;
	return TRUE;
}

// Like SetConfigAddress, but an empty or invalid string falls back: empty
// with no address on record uses emptyDefault, invalid uses the current
// value, or invalidDefault if there's none.
static BOOL SetConfigAddressWithDefault(const std::string &text, DWORD &v1, DWORD &v2, BOOL isV2,
                                        const char *emptyDefault, const char *invalidDefault)
{
	DWORD &field = isV2 ? v2 : v1;
	DWORD addr;
	if (text.size() == 0)
	{
		// Both blocks are checked, though only one is written.
		if (v2 != INADDR_NONE || v1 != INADDR_NONE)
			return FALSE;
		field = inet_addr(emptyDefault);
		return TRUE;
	}

	addr = inet_addr(text.c_str());
	if (addr == INADDR_NONE)
	{
		addr = field;
		if (addr == INADDR_NONE)
			addr = inet_addr(invalidDefault);
	}
	if (field == addr)
		return FALSE;
	field = addr;
	return TRUE;
}

// 0x000AED23
BOOL ApplyNetworkConfig(BOOL useDhcp, const char *ip, const char *subnet, const char *gateway,
                        const char *dns1, const char *dns2)
{
	XNetConfigParams params;
	memset(&params, 0, sizeof(params));
	BOOL saved = FALSE;
	XNetLoadConfigParams(&params);
	BOOL isV2 = params.v2Tag == XNET_CONFIG_V2_TAG;

	if (!useDhcp)
	{
		BOOL changed = params.flags != XNET_CONFIG_STATIC;
		if (changed)
			params.flags = XNET_CONFIG_STATIC;

		std::string text;
		text = ip;
		changed |= SetConfigAddressWithDefault(text, params.v1Ip, params.v2Ip, isV2,
		                                       "192.168.0.100", "192.168.0.5");
		text = subnet;
		changed |= SetConfigAddressWithDefault(text, params.v1Subnet, params.v2Subnet, isV2,
		                                       "255.255.255.0", "255.255.255.0");
		text = gateway;
		if (text.size() != 0)
			changed |= SetConfigAddress(text, params.v1Gateway, params.v2Gateway, isV2);
		text = dns1;
		if (text.size() != 0)
			changed |= SetConfigAddress(text, params.v1Dns1, params.v2Dns1, isV2);
		text = dns2;
		if (text.size() != 0)
			changed |= SetConfigAddress(text, params.v1Dns2, params.v2Dns2, isV2);

		if (!changed)
			goto reload;
	}
	else
	{
		if (params.flags == 0)
			goto reload;
		params.flags = 0;
	}

	XNetSaveConfigParams(&params);
	saved = TRUE;

reload:
	XNetLoadConfigParams(&g_netConfig);
	return saved;
}

// 0x000AF004
std::string GetIPAddressString()
{
	char text[16];
	IN_ADDR addr;
	addr.s_addr = g_dwIPAddress;
	XNetInAddrToString(addr, text, sizeof(text));
	std::string result;
	result = text;
	return result;
}

// Cuts reply up to the next delimiter: returns the part before it and
// leaves the rest in reply, or returns -1 if there's no delimiter.
static int TakeField(std::string &reply, const char *delimiter, std::string &field)
{
	int pos = StrFind(reply, delimiter);
	if (pos == -1)
		return -1;
	field = StrLeft(reply, pos);
	reply = StrMid(reply, pos + 1, (int)reply.size() - (pos + 1));
	return pos;
}

// 0x000AF055
int ScanNetworkClients()
{
	if (GetTickCount() <= s_dwNextClientScan)
		return (int)g_netClients.size();
	s_dwNextClientScan = GetTickCount() + 5000;
	g_netClients.clear();

	sockaddr_in to;
	memset(&to, 0, sizeof(to));
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		return (int)g_netClients.size();

	int broadcast = 1;
	int timeout = 2000;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, 4);
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, 4);
	unsigned long nonBlocking = 1;
	ioctlsocket(s, FIONBIO, &nonBlocking);

	to.sin_addr.s_addr = INADDR_BROADCAST;
	to.sin_family = AF_INET;
	to.sin_port = htons(kClientScanPort);
	while (sendto(s, "ping", 5, 0, (sockaddr *)&to, sizeof(to)) == SOCKET_ERROR)
	{
		if (GetLastError() != WSAEWOULDBLOCK)
		{
			s_dwNextClientScan = 0;
			goto close;
		}
	}

	{
		char reply[128];
		sockaddr_in from;
		socklen_t fromLen = sizeof(from);
		int received;
		Sleep(750);
		// Three tries, half a second apart, for the first reply; after that
		// keep reading until the socket runs dry.
		for (int tries = 3;;)
		{
			received = recvfrom(s, reply, sizeof(reply), 0, (sockaddr *)&from, &fromLen);
			if (received > 0)
				break;
			Sleep(500);
			if (--tries == 0)
				break;
		}

		while (received != SOCKET_ERROR && received > 5)
		{
			DWORD addr = from.sin_addr.s_addr;
			std::string ip;
			StrFormat(ip, "%d.%d.%d.%d", addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, addr >> 24);
			if (inet_addr(ip.c_str()) != INADDR_NONE)
			{
				BOOL known = FALSE;
				for (size_t i = 0; i < g_netClients.size(); i++)
				{
					if (strcmp(g_netClients[i].ip, ip.c_str()) == 0)
					{
						known = TRUE;
						break;
					}
				}

				std::string text;
				text = reply;
				if (!known && StrFind(text, ";") != -1)
				{
					SNetClient client;
					memset(&client, 0, sizeof(client));
					std::string field;
					TakeField(text, ";", field);
					strncpy(client.name, field.c_str(), 32);
					if (TakeField(text, ";", field) != -1)
					{
						strncpy(client.ftpUser, field.c_str(), 15);
						if (TakeField(text, ";", field) != -1)
						{
							strncpy(client.ftpPassword, field.c_str(), 15);
							if (TakeField(text, ";", field) != -1)
							{
								client.ftpPort = atol(field.c_str());
								// The last field ends at '\r' (not cut from text).
								int end = StrFind(text, "\r");
								if (end != -1)
									client.unknown54 = atol(StrLeft(text, end).c_str());
								strncpy(client.ip, ip.c_str(), 16);
								if (strlen(client.name) == 0)
									sprintf(client.name, "[%s]", client.ip);
								g_netClients.push_back(client);
							}
						}
					}
				}
			}

			memset(reply, 0, sizeof(reply));
			memset(&from, 0, sizeof(from));
			received = recvfrom(s, reply, sizeof(reply), 0, (sockaddr *)&from, &fromLen);
		}
	}

close:
	shutdown(s, SD_BOTH);
	closesocket(s);
	return (int)g_netClients.size();
}

// 0x000AF5A8
// Polls XNet every 2 s until the stack has an address (or is stopped),
// keeping g_dwXnAddrStatus and g_dwIPAddress current. The first time the
// network is ready it scans for other Xboxes.
static unsigned __stdcall NetworkStatusThread(void *)
{
	DWORD lastStatus = 0;
	BOOL scanned = FALSE;
	DWORD lastIp = 0;   // uninitialized in the original

	if (g_bNetworkStarted == 1)
	{
		while (!g_bNetworkStopping && WaitForSingleObject(g_hNetworkEvent, 2000) != WAIT_OBJECT_0)
		{
			DWORD status = XNetGetTitleXnAddr(&g_xnAddr);
			DWORD ip = g_xnAddr.ina.s_addr;
			if (status != lastStatus)
			{
				g_dwXnAddrStatus = status;
				lastStatus = status;
			}

			if (status > XNET_GET_XNADDR_NONE && g_dwIPAddress != ip)
			{
				BOOL noAddress = FALSE;
				if (ip != lastIp)
				{
					char text[16];
					XNetInAddrToString(g_xnAddr.ina, text, sizeof(text));
					lastIp = ip;
					g_dwIPAddress = ip;
					std::string str;
					str = text;
					noAddress = ip == 0 || str.compare("0.0.0.0") == 0;
				}
				if (!noAddress && ip != 0)
				{
					Sleep(100);
					if (!scanned && IsNetworkReady())
					{
						ScanNetworkClients();
						scanned = TRUE;
					}
					if (g_netConfig.flags == XNET_CONFIG_STATIC ||
					    (g_dwXnAddrStatus & (XNET_GET_XNADDR_STATIC | XNET_GET_XNADDR_DHCP)) != 0)
						break;
				}
			}

			if (g_bNetworkStarted != 1)
				break;
		}
	}

	XNetLoadConfigParams(&g_netConfig);
	return 0;
}

// 0x000AF6DC
static void StartNetworkStatusThread()
{
	unsigned threadId;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, NetworkStatusThread, NULL, 0, &threadId);
	if (thread != NULL && thread != INVALID_HANDLE_VALUE)
	{
		SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
		CloseHandle(thread);
	}
}

// 0x000AF716
static void InitNetworkStack()
{
	XNetStartupParams params;
	memset(&params, 0, sizeof(params));
	params.cfgSizeOfStruct = sizeof(params);
	params.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
	params.cfgPrivatePoolSizeInPages = 0x40;
	params.cfgEnetReceiveQueueLength = 0x20;
	params.cfgIpFragMaxSimultaneous = 0x10;
	params.cfgIpFragMaxPacketDiv256 = 0x20;
	params.cfgSockDefaultRecvBufsizeInK = 0x40;
	params.cfgSockDefaultSendBufsizeInK = 0x40;
	if (XNetStartup(&params) != 0)
		return;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return;
	XNetSetSystemLinkPort(1);   // TODO: verify, the wrapper at 0x001E2C7B
	g_bNetworkStarted = 1;
	StartNetworkStatusThread();
	StartLinkMonitor();
}

// 0x000AF785
// Waits up to waitSeconds + 1 seconds for the link, then starts the stack if
// it's up.
static unsigned __stdcall WaitForLinkThread(void *arg)
{
	int waitSeconds = *(int *)arg;   // sic: points into the caller's stack frame
	int tries = 0;
	while (tries < waitSeconds + 1)
	{
		if (WaitForSingleObject(g_hNetworkEvent, 1000) == WAIT_OBJECT_0 || g_bNetworkStopping)
			break;
		if (IsEthernetLinkUp())
			break;
		tries++;
	}
	Sleep(500);
	if (IsEthernetLinkUp())
		InitNetworkStack();
	return 0;
}

// 0x000AF7DC
static void StartWaitForLinkThread(int waitSeconds)
{
	unsigned threadId;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, WaitForLinkThread, &waitSeconds, 0, &threadId);
	if (thread != NULL && thread != INVALID_HANDLE_VALUE)
	{
		OutputDebugStringA("NetWork_InitThread creation succeeded.\r\n");
		CloseHandle(thread);
	}
}

// 0x000AF81C
void StartNetwork(int waitSeconds)
{
	if (g_bNetworkStarted == 1)
		return;
	if (g_hNetworkEvent == NULL)
		g_hNetworkEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	g_dwIPAddress = 0;
	g_bNetworkStopping = FALSE;
	g_dwXnAddrStatus = XNET_GET_XNADDR_NONE;
	if (waitSeconds > 0 && IsEthernetLinkUp())
	{
		StartWaitForLinkThread(waitSeconds);
		return;
	}
	InitNetworkStack();
}
