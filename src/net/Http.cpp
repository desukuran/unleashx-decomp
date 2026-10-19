// CInternetSession: the HTTP client, and HttpGetText. See Http.h.
#include "Http.h"

#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/SystemInfo.h"
#include "Network.h"
#include "Socket.h"


// Set once the web cache has been checked and cleaned. 0x00278D50
static BOOL s_bWebCacheChecked = FALSE;

#define HTTP_RECEIVE_SIZE 0x400

// 0x000DB8A7 (this on the stack)
CInternetSession::CInternetSession()
{
	memset(&m_url, 0, sizeof(m_url));
	ResetReply();
	m_strReferer.erase();
	m_strCookie.erase();
	m_unknown698 = 0;
	m_strExtraHeaders.erase();
	m_strUserAgent = "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)";
	m_strUrl.erase();
	m_hThread = INVALID_HANDLE_VALUE;
	m_bRunning = FALSE;
	m_bCancel = FALSE;
	m_nState = HTTP_IDLE;
	m_bUseCache = TRUE;
	m_fRate = 0.0f;
	m_dwSecondsLeft = 0;
	m_bHashCacheName = FALSE;
	m_pSettings = CAppSettings::GetInstance();

	if (s_bWebCacheChecked)
		return;
	s_bWebCacheChecked = TRUE;
	// Y: holds the cache; a partition that's unformatted, or small and
	// nearly full, is formatted.
	MountDrive('Y');
	SDriveInfo *info = GetDriveInfo('Y');
	if (info != NULL &&
	    (info->totalBytes == 0 || (info->freeBytes < 0xA00000 && info->totalBytes < 0x32000000)))
		FormatDrive('Y');
	// Files not from today go.
	if (DirectoryExists("Y:\\WebCache"))
	{
		WIN32_FIND_DATAA data;
		HANDLE find = FindFirstFileA(WEB_CACHE_DIR "*", &data);
		SYSTEMTIME today;
		GetSystemTime(&today);
		std::string path;
		if (find != INVALID_HANDLE_VALUE)
		{
			do
			{
				SYSTEMTIME created;
				FileTimeToSystemTime(&data.ftCreationTime, &created);
				if (today.wMonth != created.wMonth || today.wDay != created.wDay)
				{
					path = WEB_CACHE_DIR;
					path.append(data.cFileName);
					DeleteFileA(path.c_str());
				}
			} while (FindNextFileA(find, &data) == TRUE);
			CloseHandle(find);   // (sic, not FindClose)
		}
	}
}

// 0x000D9354
CInternetSession::~CInternetSession()
{
	// Quirk: only "closes" the handle when there is none.
	if (m_hThread == INVALID_HANDLE_VALUE)
		CloseHandle(INVALID_HANDLE_VALUE);
}

// (Download memsets +0x280..+0x5F0.)
void CInternetSession::ResetReply()
{
	m_nStatus = 0;
	m_strUnused284.erase();
	m_strBody.erase();
	memset(m_szFile, 0, sizeof(m_szFile));
	m_strContentType.erase();
	m_bSavedToFile = FALSE;
	m_dwDataSize = 0;
	m_dwReceived = 0;
	memset(m_szFileName, 0, sizeof(m_szFileName));
	memset(m_szCachePath, 0, sizeof(m_szCachePath));
}

// 0x000D92EA (this in esi)
void CInternetSession::Cancel()
{
	m_bCancel = TRUE;
	if (m_bRunning)
	{
		Sleep(5);
		if (m_hThread != NULL && m_hThread != INVALID_HANDLE_VALUE)
		{
			if (WaitForSingleObject(m_hThread, 200) != WAIT_OBJECT_0)
				Sleep(5);
			CloseHandle(m_hThread);
			m_hThread = INVALID_HANDLE_VALUE;
		}
		m_bRunning = FALSE;
	}
	m_nState = HTTP_IDLE;
	m_nStatus = 404;
}

// 0x000D997A (this in ecx)
void CInternetSession::SetExtraHeaders(const char *headers)
{
	if (headers != NULL && strlen(headers) > 2)
	{
		m_strExtraHeaders = headers;
		if (StrRight(m_strExtraHeaders, 2).compare("\r\n") != 0)
			m_strExtraHeaders.append("\r\n");
		return;
	}
	m_strExtraHeaders.erase();
}

// 0x00023403 (this in eax)
const char *CInternetSession::GetCachePath()
{
	(void)strchr(m_szCachePath, '?');   // (unused)
	return m_szCachePath;
}

// 0x00023373 (this in eax)
UINT CInternetSession::GetPercent()
{
	if (m_dwDataSize == 0)
		return 0;
	UINT percent = m_dwReceived * 100 / m_dwDataSize;
	return percent > 100 ? 100 : percent;
}

// 0x00023396
const char *CInternetSession::GetData()
{
	// From the cache: read the file in.
	if (m_bUseCache && m_strBody.size() == 0 && !m_bSavedToFile)
		m_strBody = ReadTextFile(GetCachePath());
	return m_strBody.c_str();
}

// 0x000DB6CE (this in ecx, url in eax)
BOOL CInternetSession::Start(const char *url, const char *postData, BOOL useCache)
{
	if (url == NULL || strlen(url) == 0)
		return FALSE;
	if (!IsEthernetLinkUp())
	{
		m_nStatus = 10065;   // WSAEHOSTUNREACH
		m_nState = HTTP_IDLE;
		m_bRunning = FALSE;
		return FALSE;
	}
	m_bUseCache = useCache;
	Cancel();
	if (ParseUrl(url, &m_url) == 0)
		return FALSE;
	m_strUrl = _strdup(url);   // (the copies are never freed)
	if (postData != NULL && strlen(postData) != 0)
		m_strPostData = _strdup(postData);
	else
		m_strPostData.erase();
	m_bCancel = FALSE;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, DownloadThread, this, CREATE_SUSPENDED, NULL);
	if (m_hThread == NULL || m_hThread == INVALID_HANDLE_VALUE)
		return FALSE;
	m_nState = HTTP_OPENING;
	ResumeThread(m_hThread);
	Sleep(50);
	return TRUE;
}

// 0x000DB7E1 (this in esi)
BOOL CInternetSession::IsDone()
{
	if (m_hThread == NULL || m_hThread == INVALID_HANDLE_VALUE)
		return TRUE;
	if (m_nState == HTTP_FINISHED)
	{
		CloseHandle(m_hThread);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_bRunning)
			m_bRunning = FALSE;
		if (m_nStatus == 302 || m_nStatus == 301)
		{
			// Follow the redirect (Download put the new place in m_url).
			std::string url;
			StrFormat(url, "%s%s:%d%s", m_url.scheme, m_url.host, m_url.port, m_url.path);
			Start(url.c_str(), NULL, m_bUseCache);
			return FALSE;
		}
	}
	return !m_bRunning;
}

// 0x000DB69E
unsigned __stdcall CInternetSession::DownloadThread(void *param)
{
	CInternetSession *session = (CInternetSession *)param;
	if (session == NULL)
		return 404;
	while (session->m_bRunning)
		Sleep(5);
	return session->Download();
}

// 0x000D94F8
int ParseUrl(const char *url, SUrl *out)
{
	if (out == NULL || url == NULL || strlen(url) == 0)
		return 0;
	std::string rest = url;
	std::string part;
	int slashes = StrFind(rest, "//", 0);
	memset(out, 0, sizeof(*out));

	if (slashes == -1)
	{
		out->port = 80;
		strcpy(out->scheme, "http://");
	}
	else
	{
		part = StrLeft(rest, slashes);
		if (StrCmpNoCase(part.c_str(), "FTP:") == 0)
		{
			out->port = 21;
			strcpy(out->scheme, "ftp://");
		}
		else if (StrCmpNoCase(part.c_str(), "mailto:") == 0)
		{
			out->port = 25;
			memcpy(out->scheme, "mailto://", 10);
		}
		else if (StrCmpNoCase(part.c_str(), "https:") == 0)
		{
			out->port = 443;
			strcpy(out->scheme, "https://");
		}
		else
		{
			// "HTTP:" and anything else
			out->port = 80;
			strcpy(out->scheme, "http://");
		}
		rest = StrMid(rest, slashes + 2, (int)rest.size() - (slashes + 2));
	}

	// host:port
	int colon = StrFind(rest, ":", 0);
	if (colon != -1)
	{
		int slash = StrFind(rest, "/", 0);
		if (slash == -1)
			slash = (int)rest.size();
		part = StrMid(rest, colon + 1, slash - colon - 1);
		long port = atol(part.c_str());
		if (port != 0)
			out->port = port;
		part = StrMid(rest, slash, (int)rest.size() - slash);
		rest = StrLeft(rest, colon);
		rest.append(part);
	}

	int slash = StrFind(rest, "/", 0);
	if (slash == -1)
	{
		strncpy(out->host, rest.c_str(), rest.size() > 49 ? 49 : rest.size());
		strcpy(out->path, "/");
	}
	else
	{
		strncpy(out->host, StrLeft(rest, slash).c_str(), slash < 50 ? slash : 49);
		rest = StrMid(rest, slash, (int)rest.size() - slash);
		int query = StrFind(rest, "?", 0);
		if (query == -1)
		{
			StrReplaceAll(rest, " ", "%20");
		}
		else
		{
			// Spaces: %20 in the path, '+' in the query.
			std::string args = StrMid(rest, query + 1, (int)rest.size() - (query + 1));
			StrReplaceAll(args, " ", "+");
			rest = StrLeft(rest, query);
			StrReplaceAll(rest, " ", "%20");
			rest.append("?");
			rest.append(args);
		}
		strncpy(out->path, rest.c_str(), rest.size() > 0x208 ? 0x208 : rest.size());
	}
	return (int)strlen(out->host);
}

// 0x000D93D1 (text in eax)
std::string UrlEncode(const char *text, BOOL encodeSeparators)
{
	if (text == NULL || strlen(text) == 0)
		return "";
	std::string out;
	std::string hex;
	for (const char *p = text; *p != 0; p++)
	{
		char c = *p;
		if (c == ' ')
		{
			out.append("+");
		}
		// Quirks: the uppercase range runs on to 'b' (so [ \ ] ^ _ ` stay
		// as they are), and characters above 127 come out as "%ffffff..".
		else if ((c < 'a' || c > 'z') && (c < 'A' || c > 'b') && (c < '-' || c > '9') && c != '*' &&
		         c != '-' && (encodeSeparators || (c != '=' && c != '&')))
		{
			StrFormat(hex, "%02x", (int)c);
			out.append("%");
			out.append(hex);
		}
		else
		{
			out.append(1, c);
		}
	}
	return out;
}

// Whether a file name ends in ".xml" or ".txt" (any case): those are kept
// in memory. (inlined twice in Download)
static BOOL IsTextFileName(const std::string &name)
{
	return StrCmpNoCase(StrRight(name, 4).c_str(), ".xml") == 0 || StrCmpNoCase(StrRight(name, 4).c_str(), ".txt") == 0;
}

// The value of a header line in headers, up to the line's end ("" if
// it isn't there). (inlined in Download)
static BOOL FindHeader(const std::string &headers, const char *name, std::string &value)
{
	int at = StrFind(headers, name, 0);
	if (at == -1)
		return FALSE;
	int start = at + (int)strlen(name);
	value = StrMid(headers, start, (int)headers.size() - start);
	int cr = StrFindChar(value, '\r');
	if (cr != -1)
		value = StrLeft(value, cr);
	return TRUE;
}

// 0x000DA2C0 (this on the stack)
DWORD CInternetSession::Download()
{
	m_nState = HTTP_OPENING;
	m_bRunning = TRUE;
	m_bCancel = FALSE;
	m_dwSecondsLeft = 0;
	m_fRate = 0.0f;
	ResetReply();
	DWORD status = 404;
	m_nStatus = 404;

	// The cache file: Y:\WebCache\ + host + file name (lowercase, without
	// slashes or "www."), or its MD5.
	std::string name = m_url.path;
	if (name.size() != 0)
	{
		int slash = StrReverseFind(name, "/", -1);
		if (slash != -1)
			name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
		strncpy(m_szFileName, name.c_str(), name.size() < 0x104 ? name.size() : 0x104);
		if (name.size() != 0)
		{
			std::string withHost;
			StrFormat(withHost, "%s%s", m_url.host, name.c_str());
			name = withHost;
			StrMakeLower(name);
			StrRemoveChar(name, '/');
			StrReplaceAll(name, "www.", "");
			if (m_bHashCacheName || name.size() > 43)
			{
				std::string base = StrLeft(name, (int)name.size() - 4);
				g_md5.HashMemory(base.c_str(), (int)base.size());
				std::string extension = StrReverseFind(name, ".", -1) == -1 ? std::string(".dat") : StrRight(name, 4);
				StrFormat(name, "%s%s", g_md5.m_szHex, extension.c_str());
				if (name.size() > 43)
					name = StrRight(name, 43);
			}
			std::string path;
			StrFormat(path, "%s%s", WEB_CACHE_DIR, name.c_str());
			name = path;
			// Quirk: also drops the character before the '?'.
			int query = StrReverseFind(name, "?", -1);
			if (query != -1)
				name = StrLeft(name, query - 1);
			StrReplaceAll(name, "\\\\", "\\");
			strcpy(m_szCachePath, name.c_str());
			strcpy(m_szFile, name.c_str());

			WIN32_FILE_ATTRIBUTE_DATA attributes;
			if (m_bUseCache && FileExists(name.c_str()) &&
			    GetFileAttributesExA(name.c_str(), GetFileExInfoStandard, &attributes))
			{
				// Already in the cache.
				m_dwDataSize = attributes.nFileSizeLow;
				m_dwReceived = attributes.nFileSizeLow;
				m_nStatus = 200;
				m_bSavedToFile = !(name.size() >= 5 && IsTextFileName(name));
				m_strBody.erase();
				m_strUnused284.erase();
				m_bRunning = FALSE;
				m_nState = HTTP_FINISHED;
				return 200;
			}
		}
	}

	CWinsock winsock(2, TRUE, 2);
	std::string request;
	std::string rest;
	BOOL post = m_strPostData.size() != 0;
	if (!winsock.m_bStarted)
	{
		DWORD error = winsock.m_nError;
		if (error == 0)
			error = WSAGetLastError();
		if (error == 0)
			error = 500;
		m_nStatus = error;
		m_bRunning = FALSE;
		WSACleanup();
		return error;
	}

	// Through the proxy, if one is set.
	int port = m_url.port;
	BOOL proxy = FALSE;
	std::string host = m_url.host;
	if (m_pSettings->m_network.m_bUseProxy && m_pSettings->m_network.m_nProxyPort != 0 &&
	    m_pSettings->m_network.m_strProxy.size() != 0)
	{
		port = m_pSettings->m_network.m_nProxyPort;
		host = m_pSettings->m_network.m_strProxy;
		proxy = TRUE;
	}
	CSocketAddress address;
	address.Set(host.c_str(), port);
	CSocket socket(true, IPPROTO_TCP);
	DWORD result;
	if (!address.m_bValid)
	{
		result = 11002;   // WSATRY_AGAIN
		m_nStatus = result;
		m_bRunning = FALSE;
		WSACleanup();
		return result;
	}
	socket.SetTimeouts(60000, 60000);
	m_nState = HTTP_CONNECTING;
	if (!socket.Connect(address))
	{
		result = WSAGetLastError();
		if (result == 0)
			result = 500;
		m_nStatus = result;
		m_bRunning = FALSE;
		WSACleanup();
		return result;
	}
	if (m_bCancel)
	{
		// (m_bRunning stays set; Cancel() clears it.)
		socket.Close();
		WSACleanup();
		return 404;
	}

	// The request.
	m_nState = HTTP_CONNECTED;
	std::string line;
	HANDLE file = INVALID_HANDLE_VALUE;
	char buffer[HTTP_RECEIVE_SIZE + 1];
	memset(buffer, 0, sizeof(buffer));
	BOOL toFile = FALSE;
	DWORD written = 0;
	std::string postBody;
	if (!post && !proxy)
	{
		if (strlen(m_url.path) == 0)
			request = "GET / HTTP/1.0\r\n";
		else
			StrFormat(request, "GET %s HTTP/1.0\r\n", m_url.path);
	}
	else
	{
		// (A POST always names the whole URL.)
		StrFormat(request, post ? "POST %s%s%s HTTP/1.0\r\n" : "GET %s%s%s HTTP/1.0\r\n", m_url.scheme, m_url.host,
		          m_url.path);
	}
	BOOL extraHeaders = m_strExtraHeaders.size() != 0;
	if (extraHeaders)
	{
		request.append(m_strExtraHeaders);
		StrFind(m_strExtraHeaders, "Icy-MetaData", 0);   // (result unused)
	}
	request.append(proxy ? "Accept: */*\r\n" : "Accept: image/x-xbitmap, image/jpeg, image/pjpeg, */*\r\n");
	request.append("Accept-Language: en-us\r\n");
	if ((!extraHeaders || StrFind(m_strExtraHeaders, "Referer:", 0) == -1) && m_strReferer.size() != 0)
	{
		StrFormat(line, "Referer: %s\r\n", m_strReferer.c_str());
		request.append(line);
	}
	if (m_strUserAgent.size() != 0)
	{
		StrFormat(line, "User-Agent: %s\r\n", m_strUserAgent.c_str());
		request.append(line);
	}
	if (m_url.port == 80)
		StrFormat(line, "Host: %s\r\n", m_url.host);
	else
		StrFormat(line, "Host: %s:%d\r\n", m_url.host, m_url.port);
	request.append(line);
	if (m_strCookie.size() != 0)
	{
		StrFormat(line, "Cookie: %s\r\n", m_strCookie.c_str());
		request.append(line);
	}
	if (post)
	{
		postBody = UrlEncode(m_strPostData.c_str(), FALSE);
		request.append("Content-Type: application/x-www-form-urlencoded\r\n");
		StrFormat(line, "Content-Length: %d\r\n", (int)postBody.size());
		request.append(line);
		request.append("Pragma: no-cache\r\n");
	}
	request.append("\r\n");
	// (Without a proxy the body only goes after a "100 Continue".)
	if (proxy && post)
		request.append(postBody);
	socket.SendString(request.c_str(), 0);

	// The reply.
	int received = socket.Receive(buffer, HTTP_RECEIVE_SIZE);
	m_dwReceived = 0;
	m_dwDataSize = 0;
	line.erase();
	m_nState = HTTP_DOWNLOADING;
	DWORD lastTick = GetTickCount();
	DWORD lastBytes = 0;
	m_strBody.erase();
	BOOL headersDone = FALSE;
	std::string headers;
	bool finished = false;
	while (!finished)
	{
		if (received == SOCKET_ERROR || received < 1 || m_bCancel)
			break;
		if (headersDone)
		{
			// Quirk: text goes in as a C string, so it ends at a NUL.
			if (!toFile)
				m_strBody.append(buffer);
			else if (WriteFile(file, buffer, received, &written, NULL) != TRUE)
				break;
			m_dwReceived += received;
		}
		else
		{
			line.append(buffer);
			int end = StrFind(line, "\r\n\r\n", 0);
			headersDone = end != -1;
			if (headersDone)
			{
				BOOL redirected = FALSE;
				int bodyStart = end + 4;
				if ((int)line.size() < bodyStart)
				{
					bodyStart = (int)line.size();
					rest.erase();
				}
				else
				{
					rest = StrMid(line, bodyStart, (int)line.size() - bodyStart);
				}
				m_strBody = StrLeft(line, end + 1);
				headers = m_strBody;
				StrRemoveChar(headers, '\n');
				int cr = StrFind(headers, "\r", 0);
				if (cr != -1)
				{
					// "HTTP/1.x 200 OK"
					line = StrLeft(headers, cr);
					BOOL gotStatus = FALSE;
					int space = StrFind(line, " ", 0);
					if (space != -1)
					{
						line = StrMid(line, space + 1, (int)line.size() - (space + 1));
						space = StrFind(line, " ", 1);
						if (space == -1 && line.size() == 3)
							space = 3;
						if (space != -1)
						{
							if (space > 3)
								line = StrLeft(line, space);
							status = atol(line.c_str());
							m_nStatus = status;
							gotStatus = TRUE;
						}
					}

					if (gotStatus && (status == 302 || status == 301))
					{
						std::string location;
						if (!FindHeader(headers, "Location:", location))
						{
							status = 404;
						}
						else
						{
							StrTrimRight(StrTrimLeft(location));
							if (StrFind(location, "//", 0) != -1 ||
							    StrCmpNoCase(StrLeft(location, 4).c_str(), "http") == 0 ||
							    StrCmpNoCase(StrLeft(location, 4).c_str(), "www.") == 0)
							{
								ParseUrl(location.c_str(), &m_url);
							}
							else
							{
								// Relative to the current folder.
								if (StrLeft(location, 1).compare("/") != 0)
								{
									std::string folder = m_url.path;
									int slash = StrReverseFind(folder, "/", -1);
									if (slash < 1)
									{
										folder = location;
										location = "/";
										location.append(folder);
									}
									else
									{
										folder = StrLeft(folder, slash);
										if (StrRight(folder, 1).compare("/") != 0)
											folder.append("/");
										folder.append(location);
										location = folder;
									}
								}
								strcpy(m_url.path, location.c_str());
							}
						}
						redirected = TRUE;
					}
					else if (gotStatus && post && status == 100)
					{
						// "100 Continue": now the form data.
						headersDone = FALSE;
						socket.SendString(postBody.c_str(), 0);
						Sleep(5);
						buffer[HTTP_RECEIVE_SIZE] = 0;
						received = socket.Receive(buffer, HTTP_RECEIVE_SIZE);
						continue;
					}
					else if (gotStatus && status != 200)
					{
						break;
					}

					std::string value;
					if (FindHeader(headers, "Set-Cookie:", value))
					{
						int semicolon = StrFind(value, ";", 0);
						if (semicolon != -1)
							value = StrLeft(value, semicolon);
						m_strCookie = StrTrimRight(StrTrimLeft(value));
					}
					if (redirected)
						break;

					if (FindHeader(headers, "Content-Length:", value))
						m_dwDataSize = atol(StrMid(value, 1, (int)value.size() - 1).c_str());
					if (!FindHeader(headers, "Content-Type:", value))
					{
						// Quirk: meant to keep .xml/.txt URLs in memory, but
						// the test is always false, so this goes to a file.
						toFile = TRUE;
					}
					else
					{
						value = StrMid(value, 1, (int)value.size() - 1);
						StrTrimRight(StrTrimLeft(value));
						StrMakeLower(value);
						m_strContentType = value;
						toFile = StrCmpNoCase(StrLeft(m_strContentType, 5).c_str(), "text/") != 0;
						if (StrFind(m_strContentType, "application/octet-stream", 0) != -1)
							toFile = TRUE;
					}
					if (toFile)
					{
						// Quirk: assumes the headers came in this packet.
						m_dwReceived = received - bodyStart;
						CreateDirectoryTree(WEB_CACHE_DIR);
						file = CreateFileA(m_szFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
						                   CREATE_ALWAYS, 0, NULL);
						if (file == INVALID_HANDLE_VALUE ||
						    WriteFile(file, buffer + bodyStart, m_dwReceived, &written, NULL) != TRUE)
							break;
					}
					else
					{
						m_dwReceived = (DWORD)rest.size() + 2;   // (sic, + 2)
						m_strBody = rest;
					}
				}
			}
		}

		if (m_dwReceived >= m_dwDataSize && m_dwDataSize != 0)
			break;
		memset(buffer, 0, HTTP_RECEIVE_SIZE);
		received = socket.Receive(buffer, HTTP_RECEIVE_SIZE);
		if (GetTickCount() - lastTick > 3000)
		{
			// The rate and time left, every 3 seconds.
			float rate = (float)(m_dwReceived - lastBytes) / ((float)(GetTickCount() - lastTick) * 0.001f);
			m_fRate = rate;
			if (rate < 1.0f || m_dwDataSize == 0)
				m_dwSecondsLeft = 0;
			else
				m_dwSecondsLeft = (m_dwDataSize - m_dwReceived) / (DWORD)rate;
			lastBytes = m_dwReceived;
			Sleep(5);
			lastTick = GetTickCount();
		}
	}

	if (!m_bCancel)
	{
		if (!toFile)
		{
			m_bSavedToFile = FALSE;
			m_szFile[0] = 0;
		}
		else
		{
			m_bSavedToFile = TRUE;
			m_strBody.erase();
			if (file != INVALID_HANDLE_VALUE)
				CloseHandle(file);
		}
	}
	else
	{
		status = 404;
	}
	m_nStatus = status;
	if (!toFile && status < 303)
		m_strReferer = m_strUrl;
	socket.Close();
	m_nState = HTTP_FINISHED;
	m_bRunning = FALSE;
	WSACleanup();
	return status;
}

// 0x00096269
std::string HttpGetText(const char *host, const char *path, int port)
{
	std::string url;
	if (port == 80)
		StrFormat(url, "%s%s", host, path);
	else
		StrFormat(url, "%s:%d%s", host, port, path);
	CInternetSession session;
	session.Get(url.c_str(), TRUE);
	while (!session.IsDone())
		Sleep(250);

	std::string text;
	if (session.m_nStatus == 200)
	{
		if (!session.m_bSavedToFile)
			text = session.GetData();
		else
			text = ReadTextFile(session.GetCachePath());
		return text;
	}

	// The session failed: a bare request (the reply's headers cut off).
	CWinsock winsock(2, TRUE, 2);
	std::string request;
	if (port == -1)
		port = 80;
	if (winsock.m_bStarted)
	{
		CSocketAddress address;
		address.Set(host, port);
		CSocket socket(true, IPPROTO_TCP);
		if (address.m_bValid && socket.Connect(address))
		{
			StrFormat(request, "GET %s HTTP/1.0\r\n\r\n", path);
			socket.SendString(request.c_str(), 0);
			char buffer[0x401];
			int received;
			while ((received = socket.Receive(buffer, 0x400)) != SOCKET_ERROR && received > 0)
			{
				buffer[0x400] = 0;   // (sic: not at received)
				text.append(buffer);
				Sleep(1000);
			}
			int end = StrFind(text, "\r\n\r\n", 0);
			if (end != -1)
				text = StrMid(text, end + 4, (int)text.size() - (end + 4));
		}
	}
	WSACleanup();
	return text;
}
