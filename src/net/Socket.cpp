// Winsock wrappers and time conversions. See Socket.h.
#include "Socket.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"

// 0x000954CF (this in eax, minor in dl)
CWinsock::CWinsock(BYTE minor, BOOL startup, BYTE major)
{
	m_bStarted = false;
	m_major = major;
	m_minor = minor;
	memset(&m_data, 0, sizeof(m_data));
	m_nError = 0;
	if (startup)
		m_bStarted = Startup(major, minor);
}

// 0x00095476 (major in cl, minor in al, this in esi)
bool CWinsock::Startup(BYTE major, BYTE minor)
{
	if (m_bStarted)
		return true;
	if (major == 0xFF)
		major = m_major;
	else
		m_major = major;
	if (minor == 0xFF)
		minor = m_minor;
	else
		m_minor = minor;
	m_nError = WSAStartup(MAKEWORD(major, minor), &m_data);
	if (m_nError != 0)
		return false;
	m_bStarted = true;
	return true;
}

// 0x00095451 (ip in eax, this in esi)
void CSocketAddress::SetIP(DWORD ip, int port)
{
	if (ip == 0 || ip == 0xFFFFFFFF)
		return;
	m_addr.sin_family = AF_INET;
	m_addr.sin_addr.s_addr = ip;
	m_addr.sin_port = htons((unsigned short)port);
	m_bValid = true;
}

// 0x00095B4B (this in eax)
void CSocketAddress::Set(const char *host, int port)
{
	m_bValid = false;
	DWORD ip = inet_addr(host);
	if (ip == 0 || ip == 0xFFFFFFFF)
		ip = ResolveHost(host);
	SetIP(ip, port);
}

// 0x000953DD
DWORD DnsLookup(const char *name)
{
	XNDNS *dns = NULL;
	DWORD ip = 0xFFFFFFFF;
	XNetDnsLookup(name, NULL, &dns);
	DWORD start = GetTickCount();
	for (;;)
	{
		if (dns == NULL)
			return 0xFFFFFFFF;
		if (dns->iStatus != WSAEINPROGRESS)
			break;
		Sleep(250);
		if (GetTickCount() > start + 15000)
		{
			dns->iStatus = WSAETIMEDOUT;
			break;
		}
	}
	if (dns->iStatus == 0 && dns->cina != 0)
		ip = dns->aina[0].s_addr;
	XNetDnsRelease(dns);
	return ip;
}

// 0x00095A01
DWORD ResolveHost(const char *name)
{
	if (name == NULL || strlen(name) == 0)
		return 0xFFFFFFFF;
	std::string host = name;
	int slash = StrFind(host, "/", 0);
	if (slash != -1)
	{
		// "http://host/path": past the slashes...
		host = StrMid(host, slash + 1, (int)host.size() - (slash + 1));
		while (StrLeft(host, 1).compare("/") == 0)
			host = StrMid(host, 1, (int)host.size() - 1);
		// ...and then (quirk) what comes after the host, not the host.
		slash = StrFind(host, "/", 0);
		if (slash != -1)
			host = StrMid(host, slash + 1, (int)host.size() - (slash + 1));
	}
	return DnsLookup(host.c_str());
}

// 0x000953AB (this in eax)
CSocket::CSocket(bool create, int protocol)
{
	m_socket = INVALID_SOCKET;
	m_bConnected = false;
	if (create)
		Create(protocol);
}

// 0x00095363 (this in esi)
bool CSocket::Create(int protocol)
{
	if (protocol == IPPROTO_TCP)
		m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	else if (protocol == IPPROTO_UDP)
		m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	else if (protocol == 254)
		m_socket = socket(AF_INET, SOCK_DGRAM, 254);   // IPPROTO_VDP
	return m_socket != INVALID_SOCKET;
}

// 0x0009561D (this in esi)
bool CSocket::Connect(const CSocketAddress &address)
{
	if (m_socket == INVALID_SOCKET)
		return false;
	if (!m_bConnected)
	{
		// (0x000953CB)
		if (connect(m_socket, (const sockaddr *)&address.m_addr, sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			WSAGetLastError();
			return false;
		}
		m_bConnected = true;
	}
	return true;
}

// 0x000952EC (this in esi)
bool CSocket::Close()
{
	if (!m_bConnected)
		return false;
	bool closed = false;
	if (shutdown(m_socket, 1 /* SD_SEND */) != SOCKET_ERROR)
	{
		char buffer[256];
		int received;
		do
		{
			received = recv(m_socket, buffer, sizeof(buffer), 0);
		} while (received != SOCKET_ERROR && received != 0);
		closed = true;
	}
	closesocket(m_socket);
	// (m_socket and m_bConnected are left as they were.)
	return closed;
}

// 0x00095255
void CSocket::SetTimeouts(int receive, int send)
{
	if (m_socket == INVALID_SOCKET)
		return;
	if (receive > 0)
	{
		if (receive < 500)
			receive *= 1000;
		setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&receive, sizeof(receive));
	}
	if (send > 0)
	{
		if (send < 500)
			send *= 1000;
		setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&send, sizeof(send));
	}
}

// 0x0009533B (this in eax)
void CSocket::SetBlocking(BOOL blocking)
{
	if (m_socket == INVALID_SOCKET)
		return;
	unsigned long nonBlocking = !blocking;
	ioctlsocket(m_socket, FIONBIO, &nonBlocking);
}

// 0x000952C3 (this in ecx)
int CSocket::Send(const void *data, int size, int flags)
{
	if (data == NULL || m_socket == INVALID_SOCKET || !m_bConnected)
		return 0;
	return send(m_socket, (const char *)data, size, flags);
}

// 0x0009522F (this in ecx)
int CSocket::Receive(void *buffer, int size)
{
	if (buffer == NULL || m_socket == INVALID_SOCKET || !m_bConnected)
		return 0;
	return recv(m_socket, (char *)buffer, size, 0);
}

// 0x000959A9 (this in esi)
int CSocket::SendString(const char *text, int flags)
{
	int sent = 0;
	std::string copy = text != NULL ? text : "";
	if (text != NULL && copy.size() != 0 && m_socket != INVALID_SOCKET && m_bConnected)
		sent = Send(copy.c_str(), (int)copy.size(), flags);
	return sent;
}

// 0x000955F7 (data in ecx)
DWORD ReadBigEndian32(const BYTE *data)
{
	if (data == NULL)
		return 0;
	return ((DWORD)data[0] << 24) | ((DWORD)data[1] << 16) | ((DWORD)data[2] << 8) | data[3];
}

// 0x00095511
SYSTEMTIME NtpReplyToSystemTime(const BYTE *reply)
{
	// The transmit timestamp's seconds (the fraction is dropped).
	DWORD seconds = 0;
	for (int i = 0; i < 4; i++)
		seconds = seconds * 0x100 + reply[0x28 + i];
	ULONGLONG ticks = ((ULONGLONG)seconds + NTP_TO_FILETIME_SECONDS) * 10000000;
	SYSTEMTIME st;
	FileTimeToSystemTime((const FILETIME *)&ticks, &st);
	return st;
}

// 0x00095571
void WriteNtpTime(BYTE *packet, int offset)
{
	SYSTEMTIME now;
	FILETIME ft;
	GetSystemTime(&now);
	SystemTimeToFileTime(&now, &ft);
	// Quirk: the epoch difference is taken off in 100 ns units rather than
	// seconds, so this is really "seconds since 1601". Servers only echo it.
	ULONGLONG ticks = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	ULONGLONG ms = (ticks - NTP_TO_FILETIME_SECONDS) / 10000;
	DWORD seconds = (DWORD)(ms / 1000);
	DWORD fraction = (DWORD)(((ms % 1000) << 32) / 1000);
	for (int i = 3; i >= 0; i--)
	{
		packet[offset + i] = (BYTE)seconds;
		seconds >>= 8;
	}
	for (int i = 7; i > 3; i--)
	{
		packet[offset + i] = (BYTE)fraction;
		fraction >>= 8;
	}
}

// 0x00095666 (st in ebx)
void ParseDaytime(const char *text, UINT length, SYSTEMTIME *st)
{
	std::string reply = text;
	int hour = 0, minute = 0, second = 0;
	int month = 11, day = 15, year = 2003;
	char ampm = 'A';
	if (length < 25)
	{
		// "hh:mm:ss AM m/d/yyyy"
		sscanf(text, " %2d:%2d:%2d %cM %d/%d/%4d", &hour, &minute, &second, &ampm, &month, &day, &year);
		if (ampm == 'P' || ampm == 'p')
		{
			hour += 12;
			if (hour == 24)
				hour = 12;
		}
		else if (hour == 12)
		{
			hour = 0;
		}
	}
	else if (length >= 40)
	{
		// NIST: "JJJJJ YR-MO-DA HH:MM:SS TT L H msADV UTC(NIST) *"
		long mjd;
		int dst, leap, health;
		sscanf(text, " %ld %2d-%2d-%2d %2d:%2d:%2d %d %d %d", &mjd, &year, &month, &day, &hour, &minute, &second,
		       &dst, &leap, &health);
	}
	else
	{
		// "Day Mon dd hh:mm:ss yyyy" (with an optional "UTC ")
		StrReplaceAll(reply, "UTC ", "");
		std::string weekday = StrLeft(reply, 4);
		const char *const days[] = { "Sat ", "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri " };
		for (int i = 0; i < 7; i++)
		{
			if (StrCmpNoCase(weekday.c_str(), days[i]) == 0)
			{
				reply = StrMid(reply, 4, (int)reply.size() - 4);
				break;
			}
		}
		reply.erase(0, reply.find_first_not_of(" "));
		char m1 = 0, m2 = 0, m3 = 0;
		sscanf(reply.c_str(), "%c%c%c %2d %2d:%2d:%2d %4d", &m1, &m2, &m3, &day, &hour, &minute, &second, &year);
		switch (m1)
		{
		case 'M': month = m3 == 'y' ? 5 : 3; break;
		case 'A': month = m3 == 'g' ? 8 : 4; break;
		case 'D': month = 12; break;
		case 'F': month = 2; break;
		case 'J': month = m2 == 'a' ? 1 : (m3 == 'l' ? 7 : 6); break;
		case 'N': month = 11; break;
		case 'O': month = 10; break;
		case 'S': month = 9; break;
		}
	}
	st->wDay = (WORD)day;
	st->wSecond = (WORD)second;
	st->wMinute = (WORD)minute;
	st->wHour = (WORD)hour;
	st->wMonth = (WORD)month;
	if (year < 1001)
		year += 2000;
	st->wYear = (WORD)year;
	if (st->wYear > 2100)
		st->wYear = 2003;
}

// 0x00095201
void SetLocalSystemTime(const SYSTEMTIME *st)
{
	FILETIME local, utc;
	SystemTimeToFileTime(st, &local);
	LocalFileTimeToFileTime(&local, &utc);
	NtSetSystemTime((LARGE_INTEGER *)&utc, (LARGE_INTEGER *)&local);
}
