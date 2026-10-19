// Small Winsock wrappers the HTTP client and the time sync use: starting
// Winsock, a TCP/UDP socket, an IPv4 address (with DNS lookup), and the
// NTP/TIME protocol time conversions. Socket.cpp.
#pragma once

#include "../common/Platform.h"
#include "XNet.h"

// Starts Winsock for the lifetime of a function (the callers end with
// WSACleanup()). 0x198 bytes.
class CWinsock
{
public:
	// (this in eax, minor in dl)
	CWinsock(BYTE minor, BOOL startup, BYTE major);       // 0x000954CF
	// 0xFF keeps the version given before. (major in cl, minor in al,
	// this in esi)
	bool Startup(BYTE major, BYTE minor);                 // 0x00095476

	WSADATA m_data;                 // +0x000
	bool m_bStarted;                // +0x190
	BYTE m_major;                   // +0x191
	BYTE m_minor;                   // +0x192
	int m_nError;                   // +0x194 what WSAStartup returned
};

// An IPv4 address. 0x14 bytes.
struct CSocketAddress
{
	// host: a dotted address or a name to look up. (this in eax)
	void Set(const char *host, int port);                 // 0x00095B4B
	// Nothing for 0 or -1. (ip in eax, this in esi)
	void SetIP(DWORD ip, int port);                       // 0x00095451

	sockaddr_in m_addr;             // +0x00
	bool m_bValid;                  // +0x10
};

// A socket. 8 bytes.
class CSocket
{
public:
	// (this in eax)
	CSocket(bool create, int protocol);                   // 0x000953AB
	~CSocket() { if (m_bConnected) Close(); }             // 0x0009539B (this in eax)
	// protocol: IPPROTO_TCP (6), IPPROTO_UDP (17) or 254 (VDP). (this in esi)
	bool Create(int protocol);                            // 0x00095363
	// (this in esi)
	bool Connect(const CSocketAddress &address);          // 0x0009561D
	// Shuts down sending, drains what's left and closes. (this in esi)
	bool Close();                                         // 0x000952EC
	// Receive/send timeouts; under 500 they're seconds, else milliseconds.
	// 0 or less keeps the current one.
	void SetTimeouts(int receive, int send);              // 0x00095255
	// (this in eax)
	void SetBlocking(BOOL blocking);                      // 0x0009533B
	// Nothing unless connected; the result isn't kept (callers get
	// garbage). (this in ecx)
	int Send(const void *data, int size, int flags);      // 0x000952C3
	int Receive(void *buffer, int size);                  // 0x0009522F (this in ecx)
	// (this in esi)
	int SendString(const char *text, int flags);          // 0x000959A9

	SOCKET m_socket;                // +0x00 -1 = none
	bool m_bConnected;              // +0x04
};

// The address of a host name ("http://host/..." is cut down first, see the
// quirk in Socket.cpp), or -1.
DWORD ResolveHost(const char *name);                      // 0x00095A01
// XNetDnsLookup, waiting at most 15 seconds; -1 on failure.
DWORD DnsLookup(const char *name);                        // 0x000953DD

// NTP's epoch (1900) to FILETIME's (1601), in seconds.
#define NTP_TO_FILETIME_SECONDS 0x232661280ULL    // 9435484800

// The transmit time of an NTP reply. (reply on the stack)
SYSTEMTIME NtpReplyToSystemTime(const BYTE *reply);       // 0x00095511
// The current UTC time as an NTP timestamp at packet + offset.
void WriteNtpTime(BYTE *packet, int offset);              // 0x00095571
// A big-endian 32-bit number. (data in ecx)
DWORD ReadBigEndian32(const BYTE *data);                  // 0x000955F7
// A "daytime" text reply ("hh:mm:ss AM m/d/yyyy", NIST's "JJJJJ YR-MO-DA
// HH:MM:SS ..." or ctime-like "Day Mon dd hh:mm:ss yyyy") as a date. (st
// in ebx)
void ParseDaytime(const char *text, UINT length, SYSTEMTIME *st);   // 0x00095666
// Sets the clock from a local time. Unused.
void SetLocalSystemTime(const SYSTEMTIME *st);            // 0x00095201
