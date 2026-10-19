// SyncTimeWithSntp: sets the clock from a time server. See Http.h.
#include <cstdlib>
#include <cstring>

#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../settings/AppSettings.h"
#include "../system/SystemControl.h"
#include "Http.h"
#include "Socket.h"

#define TIME_PORT 37   // RFC 868

// The US Naval Observatory's web clock, the last resort: its page has the
// UTC time as "<BR>Mon. dd, hh:mm:ss UTC<BR>" (only the month, day and
// time are taken; the year is today's, at least 2004). (inlined)
static void SyncTimeWithWebClock()
{
	CInternetSession session;
	session.Get("http://tycho.usno.navy.mil/cgi-bin/timer.pl", FALSE);
	while (!session.IsDone())
		Sleep(100);
	if (session.m_nStatus != 200 || session.m_bSavedToFile)
		return;
	std::string page = session.GetData();
	if (page.size() == 0)
		return;
	int at = StrFind(page, "<BR>", 0);
	if (at == -1)
		return;
	page = StrMid(page, at + 4, (int)page.size() - (at + 4));
	at = StrFind(page, "<BR>", 0);
	if (at == -1 || at <= 18)
		return;

	SYSTEMTIME st;
	memset(&st, 0, sizeof(st));
	GetSystemTime(&st);
	page = StrLeft(page, at);
	st.wDay = (WORD)atol(StrMid(page, 5, 2).c_str());
	st.wHour = (WORD)atol(StrMid(page, 9, 2).c_str());
	st.wMinute = (WORD)atol(StrMid(page, 12, 2).c_str());
	st.wSecond = (WORD)atol(StrMid(page, 15, 2).c_str());
	page = StrLeft(page, 3);
	switch (StrLeft(page, 1)[0])
	{
	case 'M': st.wMonth = StrRight(page, 1)[0] == 'y' ? 5 : 3; break;
	case 'A': st.wMonth = StrRight(page, 1)[0] == 'g' ? 8 : 4; break;
	case 'D': st.wMonth = 12; break;
	case 'F': st.wMonth = 2; break;
	case 'J':
		if (StrMid(page, 1, 1)[0] == 'a')
			st.wMonth = 1;
		else
			st.wMonth = StrRight(page, 1)[0] == 'l' ? 7 : 6;
		break;
	case 'N': st.wMonth = 11; break;
	case 'O': st.wMonth = 10; break;
	case 'S': st.wMonth = 9; break;
	}
	if (st.wYear < 2004)
		st.wYear = 2004;
	if (st.wDay != 0 && st.wMonth != 0)
		SetXboxSystemTime(st, FALSE);
}

// 0x00095B7D
int SyncTimeWithSntp(const char *server, int unused, int port)
{
	int result = -1;
	CWinsock winsock(2, TRUE, 2);
	CSocket socket(true, IPPROTO_UDP);

	if (!winsock.m_bStarted)
	{
		result = winsock.m_nError;
	}
	else
	{
		if (port == 0)
			port = TIME_PORT;
		CSocketAddress address;
		address.Set(server, port);
		if (address.m_bValid)
		{
			socket.SetTimeouts(30, 30);
			if (!socket.Connect(address))
			{
				if (port != TIME_PORT)
				{
					// Try the TIME protocol on the same server.
					socket.Close();
					result = SyncTimeWithSntp(server, unused, TIME_PORT);
					WSACleanup();
					return result;
				}
			}
			else
			{
				SYSTEMTIME st;
				GetSystemTime(&st);
				BYTE request[0x31];
				memset(request, 0, sizeof(request));
				BYTE reply[0x400];
				memset(reply, 0, sizeof(reply));
				int size = 0x30;
				if (port == TIME_PORT)
				{
					request[0] = ' ';
					size = 1;
				}
				else
				{
					request[0] = 0x1B;   // NTP version 3, client
				}
				DWORD sent = GetTickCount();
				socket.Send(request, size, 0);
				int received = socket.Receive(reply, sizeof(reply));
				DWORD answered = GetTickCount();
				memset(&st, 0, sizeof(st));
				if (received != SOCKET_ERROR)
				{
					if (port == TIME_PORT)
					{
						// Seconds since 1900, plus half the round trip.
						DWORD seconds = ReadBigEndian32(reply) + (answered - sent) / 2000;
						GetSystemTime(&st);
						ULONGLONG ticks = ((ULONGLONG)seconds + NTP_TO_FILETIME_SECONDS) * 10000000;
						FileTimeToSystemTime((const FILETIME *)&ticks, &st);
						SetXboxSystemTime(st, FALSE);
					}
					else
					{
						SYSTEMTIME now;
						GetSystemTime(&now);
						SetXboxSystemTime(NtpReplyToSystemTime(reply), FALSE);
					}
					result = 0;
				}
			}
			socket.Close();
		}
	}

	if (result == -1)
	{
		if (port != TIME_PORT)
		{
			socket.Close();
			result = SyncTimeWithSntp("time-a.nist.gov", unused, TIME_PORT);
		}
		else if (CAppSettings::GetInstance()->m_prefs.m_bSNTPAllowHTTP)
		{
			SyncTimeWithWebClock();
		}
	}
	WSACleanup();
	return result;
}
