// Host (non-Xbox) stand-ins for XNet: the network never comes up.
#ifndef _XBOX
#define UNLEASHX_XNET_HOST_IMPL
#include "XNet.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

extern "C" {
int __stdcall XNetLoadConfigParams(XNetConfigParams *params)
{
	memset(params, 0, sizeof(*params));
	return 0;
}

int __stdcall XNetSaveConfigParams(const XNetConfigParams *)
{
	return 0;
}
}

BOOL g_bHostLinkUp = FALSE;

int XNetStartup(const XNetStartupParams *)
{
	return -1;
}

int XNetCleanup()
{
	return 0;
}

int WSAStartup(WORD, WSADATA *)
{
	return g_bHostLinkUp ? 0 : -1;
}

int WSACleanup()
{
	return 0;
}

DWORD XNetGetTitleXnAddr(XNADDR *addr)
{
	memset(addr, 0, sizeof(*addr));
	return XNET_GET_XNADDR_NONE;
}

DWORD XNetGetEthernetLinkStatus()
{
	return g_bHostLinkUp ? XNET_ETHERNET_LINK_ACTIVE | XNET_ETHERNET_LINK_100MBPS : 0;
}

int XNetInAddrToString(IN_ADDR ina, char *text, int textSize)
{
	const BYTE *b = (const BYTE *)&ina.s_addr;
	snprintf(text, textSize, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
	return 0;
}

DWORD XNetGetBroadcastVersionStatus(BOOL)
{
	return 0;
}

int closesocket(SOCKET s)
{
	return close(s);
}

int ioctlsocket(SOCKET s, long cmd, unsigned long *arg)
{
	if ((unsigned long)cmd == (unsigned long)FIONBIO)
	{
		int flags = fcntl(s, F_GETFL, 0);
		fcntl(s, F_SETFL, *arg ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
	}
	return 0;
}

int WSAGetLastError()
{
	switch (errno)
	{
	case EWOULDBLOCK:
	case EINPROGRESS:
		return WSAEWOULDBLOCK;
	case ECONNRESET:
	case EPIPE:
		return WSAECONNRESET;
	case ENOTCONN:
		return WSAENOTCONN;
	case ESHUTDOWN:
		return WSAESHUTDOWN;
	case ETIMEDOUT:
		return WSAETIMEDOUT;
	}
	return errno;
}

int XNetRandom(BYTE *buffer, UINT size)
{
	for (UINT i = 0; i < size; i++)
		buffer[i] = (BYTE)rand();
	return 0;
}

// Winsock ignores select()'s first argument; POSIX needs the highest
// descriptor + 1.
int HostSelect(int, fd_set *readSet, fd_set *writeSet, fd_set *exceptSet, struct timeval *timeout)
{
	return select(FD_SETSIZE, readSet, writeSet, exceptSet, timeout);
}

int HostSetSockOpt(SOCKET s, int level, int name, const void *value, int size)
{
	if (level == SOL_SOCKET && (name == SO_RCVTIMEO || name == SO_SNDTIMEO) && size == (int)sizeof(int))
	{
		int ms = *(const int *)value;
		struct timeval tv;
		tv.tv_sec = ms / 1000;
		tv.tv_usec = (ms % 1000) * 1000;
		return setsockopt(s, level, name, &tv, sizeof(tv));
	}
	return setsockopt(s, level, name, value, (socklen_t)size);
}

// Looks the name up right away (the Xbox does it in the background).
int XNetDnsLookup(const char *name, HANDLE, XNDNS **dns)
{
	XNDNS *result = new XNDNS;
	memset(result, 0, sizeof(*result));
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	struct addrinfo *list = NULL;
	if (getaddrinfo(name, NULL, &hints, &list) != 0)
	{
		result->iStatus = 11001;   // WSAHOST_NOT_FOUND
	}
	else
	{
		for (struct addrinfo *it = list; it != NULL && result->cina < 8; it = it->ai_next)
			result->aina[result->cina++] = ((struct sockaddr_in *)it->ai_addr)->sin_addr;
		freeaddrinfo(list);
	}
	*dns = result;
	return 0;
}

int XNetDnsRelease(XNDNS *dns)
{
	delete dns;
	return 0;
}

#endif
