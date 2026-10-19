// XNet (the Xbox's network stack) declarations UnleashX uses that <xtl.h>
// doesn't have, and host stand-ins for the rest.
#pragma once

#include "../common/Platform.h"

// The network settings in the Xbox config area, read and written with
// XNetLoadConfigParams/XNetSaveConfigParams (exported by XNet but not in the
// XDK headers). 0x6EC bytes; only the fields UnleashX touches are named. The
// MS dashboard writes the "V2" block (dwV2Tag == 'XBV2') and older ones the
// V1 block, so UnleashX reads and writes whichever is in use.
struct XNetConfigParams
{
	BYTE unknown00[0x14];
	DWORD v1Ip;               // +0x14
	DWORD v1Subnet;           // +0x18
	DWORD v1Gateway;          // +0x1C
	DWORD v1Dns1;             // +0x20
	DWORD v1Dns2;             // +0x24
	BYTE unknown28[0x14];
	DWORD v2Tag;              // +0x3C XNET_CONFIG_V2_TAG
	DWORD flags;              // +0x40 XNET_CONFIG_STATIC or 0 (DHCP)
	DWORD unknown44;
	DWORD v2Ip;               // +0x48
	DWORD v2Subnet;           // +0x4C
	DWORD v2Gateway;          // +0x50
	DWORD v2Dns1;             // +0x54
	DWORD v2Dns2;             // +0x58
	BYTE unknown5C[0x6EC - 0x5C];
};

#define XNET_CONFIG_V2_TAG 0x58425632   // 'XBV2'
#define XNET_CONFIG_STATIC 0x0C

extern "C" {
int __stdcall XNetLoadConfigParams(XNetConfigParams *params);
int __stdcall XNetSaveConfigParams(const XNetConfigParams *params);
}

#ifdef _XBOX
typedef int socklen_t;
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#define MAKEWORD(lo, hi) ((WORD)(((BYTE)(lo)) | ((WORD)((BYTE)(hi))) << 8))

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SD_BOTH 2
#define WSAEWOULDBLOCK 10035
#define WSAECONNRESET 10054
#define WSAENOTCONN 10057
#define WSAESHUTDOWN 10058
#define WSAETIMEDOUT 10060
#define WSAEINPROGRESS 10036
#ifndef FIONBIO
#define FIONBIO 0x8004667E
#endif
typedef struct in_addr IN_ADDR;

struct XNADDR
{
	IN_ADDR ina;
	IN_ADDR inaOnline;
	WORD wPortOnline;
	BYTE abEnet[6];
	BYTE abOnline[20];
};

// XNetGetTitleXnAddr() status bits
#define XNET_GET_XNADDR_PENDING 0x0000
#define XNET_GET_XNADDR_NONE 0x0001
#define XNET_GET_XNADDR_ETHERNET 0x0002
#define XNET_GET_XNADDR_STATIC 0x0004
#define XNET_GET_XNADDR_DHCP 0x0008
#define XNET_GET_XNADDR_PPPOE 0x0010
#define XNET_GET_XNADDR_GATEWAY 0x0020
#define XNET_GET_XNADDR_DNS 0x0040
#define XNET_GET_XNADDR_ONLINE 0x0080

// XNetGetEthernetLinkStatus() bits
#define XNET_ETHERNET_LINK_ACTIVE 0x01
#define XNET_ETHERNET_LINK_100MBPS 0x02
#define XNET_ETHERNET_LINK_10MBPS 0x04
#define XNET_ETHERNET_LINK_FULL_DUPLEX 0x08
#define XNET_ETHERNET_LINK_HALF_DUPLEX 0x10

struct XNetStartupParams
{
	BYTE cfgSizeOfStruct;
	BYTE cfgFlags;
	BYTE cfgPrivatePoolSizeInPages;
	BYTE cfgEnetReceiveQueueLength;
	BYTE cfgIpFragMaxSimultaneous;
	BYTE cfgIpFragMaxPacketDiv256;
	BYTE cfgSockMaxSockets;
	BYTE cfgSockDefaultRecvBufsizeInK;
	BYTE cfgSockDefaultSendBufsizeInK;
	BYTE cfgKeyRegMax;
	BYTE cfgSecRegMax;
	BYTE cfgQosDataLimitDiv4;
};
#define XNET_STARTUP_BYPASS_SECURITY 0x01

struct WSADATA
{
	BYTE opaque[400];
};

int XNetStartup(const XNetStartupParams *params);
int XNetCleanup();
int WSAStartup(WORD version, WSADATA *data);
int WSACleanup();
DWORD XNetGetTitleXnAddr(XNADDR *addr);
DWORD XNetGetEthernetLinkStatus();
// Host only: whether XNetGetEthernetLinkStatus() reports a link (tests set it).
extern BOOL g_bHostLinkUp;
int XNetInAddrToString(IN_ADDR ina, char *text, int textSize);
int XNetSetSystemLinkPort(WORD port);   // TODO: verify, see Network.cpp
int closesocket(SOCKET s);
int ioctlsocket(SOCKET s, long cmd, unsigned long *arg);
int WSAGetLastError();
int XNetRandom(BYTE *buffer, UINT size);
int HostSelect(int count, fd_set *readSet, fd_set *writeSet, fd_set *exceptSet, struct timeval *timeout);
// Winsock's SO_RCVTIMEO/SO_SNDTIMEO take milliseconds as a DWORD.
int HostSetSockOpt(SOCKET s, int level, int name, const void *value, int size);
#ifndef UNLEASHX_XNET_HOST_IMPL
#define select HostSelect
#define setsockopt HostSetSockOpt
#endif

// An XNetDnsLookup() result.
struct XNDNS
{
	int iStatus;            // 0, or WSAEINPROGRESS while looking up
	UINT cina;
	IN_ADDR aina[8];
};
int XNetDnsLookup(const char *name, HANDLE event, XNDNS **dns);
int XNetDnsRelease(XNDNS *dns);

HANDLE CreateEventA(void *attributes, BOOL manualReset, BOOL initialState, LPCSTR name);
BOOL SetEvent(HANDLE event);
#define WAIT_OBJECT_0 0
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_LOWEST (-2)
#endif
