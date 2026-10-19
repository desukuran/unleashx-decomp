// HTTP downloads (CInternetSession, a small HTTP client used by the live
// update check, the newsfeed and the FTP site list) and SNTP. Only what the
// recovered code calls is declared. TODO: decompile
#pragma once

#include <string>

#include "../common/Platform.h"

// vtable 0x002561EC
class CInternetSession
{
public:
	CInternetSession();                        // 0x000DB8A7
	virtual ~CInternetSession();               // 0x000D9354

	// Starts downloading url on the session's thread. TODO: what the flag is
	void Get(const char *url, BOOL unknown);   // 0x000DB7D3
	BOOL IsDone();                             // 0x000DB7E1
	// The downloaded body when it was kept in memory.
	const char *GetData();                     // 0x00023396

	BYTE m_unknown004[0x27C];
	int m_nStatus;                             // +0x280 HTTP status code
	BYTE m_unknown284[0x38];
	char m_szFile[0x120];                      // +0x2BC where the body was saved, when m_bSavedToFile
	int m_bSavedToFile;                        // +0x3DC
	BYTE m_unknown3E0[0x260];
};

// GETs http://host[:port]path and returns the body ("" on failure).
std::string HttpGetText(const char *host, const char *path, int port);   // 0x00096269

// Sets the clock from an SNTP server. TODO: what the second argument is
BOOL SyncTimeWithSntp(const char *server, int unknown, int port);       // 0x00095B7D
