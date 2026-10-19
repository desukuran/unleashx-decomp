// Host (non-Xbox) stand-ins for XNet: the network never comes up.
#ifndef _XBOX
#include "XNet.h"

#include <cstdio>
#include <cstring>
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
	return -1;
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
	return 0;
}

int XNetInAddrToString(IN_ADDR ina, char *text, int textSize)
{
	const BYTE *b = (const BYTE *)&ina.s_addr;
	snprintf(text, textSize, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
	return 0;
}

int XNetSetSystemLinkPort(WORD)
{
	return 0;
}

int closesocket(SOCKET s)
{
	return close(s);
}

int ioctlsocket(SOCKET, long, unsigned long *)
{
	return 0;
}

DWORD GetLastError()
{
	return 0;
}

HANDLE CreateEventA(void *, BOOL, BOOL, LPCSTR)
{
	return (HANDLE)1;
}

BOOL SetEvent(HANDLE)
{
	return TRUE;
}
#endif
