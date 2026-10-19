// HTTP downloads (CInternetSession, Http.cpp: a small HTTP/1.0 client used
// by the live update check, the newsfeeds, the skin browser and the FTP
// site list) and the clock sync (SyncTimeWithSntp, Sntp.cpp).
//
// Downloads run on a thread. Text replies ("text/...") are kept in memory;
// anything else goes to a file in Y:\WebCache\, which is also a cache: with
// useCache, a file already there is used without asking the server. The
// cache is emptied of files not from today when the first session is made.
#pragma once

#include <string>

#include "../common/Platform.h"

class CXBFont;
class CAppSettings;
struct IDirect3DTexture8;

#define WEB_CACHE_DIR "Y:\\WebCache\\"

// A parsed URL. 0x27A bytes.
struct SUrl
{
	int port;                       // +0x000 80 by default
	char scheme[10];                // +0x004 "http://", "ftp://", "mailto://", "https://"
	char host[100];                 // +0x00E at most 49 characters
	char path[0x208];               // +0x072 "/" at least; spaces as %20
};

// The download states (m_nState).
enum
{
	HTTP_IDLE = 0,
	HTTP_OPENING = 1,
	HTTP_CONNECTING = 2,
	HTTP_CONNECTED = 3,
	HTTP_DOWNLOADING = 4,
	HTTP_FINISHED = 5,
};

// vtable 0x002561EC. 0x6C0 bytes.
class CInternetSession
{
public:
	CInternetSession();                        // 0x000DB8A7 (this on the stack)
	virtual ~CInternetSession();               // 0x000D9354 (deleting 0x0002DA89)

	// Starts downloading url on the session's thread (after cancelling
	// one that's running). useCache: take Y:\WebCache's copy if there is
	// one. (this in ecx, url in eax)
	BOOL Get(const char *url, BOOL useCache) { return Start(url, NULL, useCache); }   // 0x000DB7D3
	// The same, as a POST of postData (form encoded) when it isn't empty.
	// FALSE when the link is down or the URL is empty. (this in ecx, url
	// in eax)
	BOOL Start(const char *url, const char *postData, BOOL useCache);   // 0x000DB6CE
	// TRUE once the thread has finished; a redirect (301/302) starts the
	// next request instead. (this in esi)
	BOOL IsDone();                             // 0x000DB7E1
	// The body (read from the cache file when it came from the cache).
	const char *GetData();                     // 0x00023396
	// Stops the download thread; the status becomes 404. (this in esi)
	void Cancel();                             // 0x000D92EA
	// Extra request header lines (CRLF added); "" or less than 3
	// characters clears them. (this in ecx)
	void SetExtraHeaders(const char *headers); // 0x000D997A
	// The cache file's name. (this in eax)
	const char *GetCachePath();                // 0x00023403
	// 0-100 of the Content-Length. (this in eax)
	UINT GetPercent();                         // 0x00023373
	// The download box: file name and percent, a texture (an RSS feed's
	// image, optional), source, time left, rate and state. (font in ebx)
	static void DrawProgress(CInternetSession *session, IDirect3DTexture8 *texture, CXBFont *font);   // 0x000D99F7

	SUrl m_url;                                // +0x004
	// From here to +0x5F0 is the reply, cleared before each request (the
	// original memsets the block, strings and all). (0x0002DAA5 constructs
	// its strings.)
	int m_nStatus;                             // +0x280 HTTP status code (or a socket error)
	std::string m_strUnused284;                // +0x284
	std::string m_strBody;                     // +0x2A0 the headers, then the body when kept in memory
	char m_szFile[0x104];                      // +0x2BC where the body was saved, when m_bSavedToFile
	std::string m_strContentType;              // +0x3C0 lower case
	BOOL m_bSavedToFile;                       // +0x3DC
	DWORD m_dwDataSize;                        // +0x3E0 Content-Length (or the cached file's size)
	DWORD m_dwReceived;                        // +0x3E4
	char m_szFileName[0x104];                  // +0x3E8 the URL's file name, for the progress box
	char m_szCachePath[0x104];                 // +0x4EC
	std::string m_strReferer;                  // +0x5F0 the last URL fetched into memory
	std::string m_strUrl;                      // +0x60C
	std::string m_strCookie;                   // +0x628 from Set-Cookie
	std::string m_strUserAgent;                // +0x644 "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)"
	std::string m_strPostData;                 // +0x660
	std::string m_strExtraHeaders;             // +0x67C
	int m_unknown698;                          // +0x698
	HANDLE m_hThread;                          // +0x69C
	BOOL m_bRunning;                           // +0x6A0
	BOOL m_bCancel;                            // +0x6A4
	BOOL m_bUseCache;                          // +0x6A8
	int m_nState;                              // +0x6AC HTTP_*
	float m_fRate;                             // +0x6B0 bytes per second
	DWORD m_dwSecondsLeft;                     // +0x6B4
	// The cache file is named by the MD5 of the URL (set by the Live
	// Update page; long names get this too). The progress box then also
	// leaves out the file name.
	BOOL m_bHashCacheName;                     // +0x6B8
	CAppSettings *m_pSettings;                 // +0x6BC

private:
	static unsigned __stdcall DownloadThread(void *param);   // 0x000DB69E
	// The request itself; returns the status. (this on the stack)
	DWORD Download();                          // 0x000DA2C0
	void ResetReply();                         // (the memset in Download)
};

// Splits url into out; the host's length (0 = bad URL).
int ParseUrl(const char *url, SUrl *out);      // 0x000D94F8
// Form encoding: spaces as '+', others as %xx (see the quirks in Http.cpp);
// encodeSeparators also encodes '=' and '&'. (text in eax)
std::string UrlEncode(const char *text, BOOL encodeSeparators);   // 0x000D93D1

// GETs http://host[:port]path and returns the body ("" on failure); falls
// back to a bare socket request when the session fails.
std::string HttpGetText(const char *host, const char *path, int port);   // 0x00096269

// Sets the clock from a time server: NTP (port 123 and others) or the TIME
// protocol (port 37, also the default). Falls back to TIME at
// time-a.nist.gov, then (if allowed) to the US Naval Observatory's web
// clock. 0 = set; otherwise -1 or a Winsock error.
int SyncTimeWithSntp(const char *server, int unused, int port);   // 0x00095B7D
