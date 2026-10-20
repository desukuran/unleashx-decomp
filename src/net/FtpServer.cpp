// CFTPServer: the listening socket, the connection list, statistics and the
// LAN discovery reply. The connections themselves are in FtpConnection.cpp
// and FtpTransfer.cpp.
#include "FtpServer.h"

#include <cstring>

#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../settings/AppSettings.h"
#include "Network.h"

BYTE g_bFtpEnabled = TRUE;
CFTPServer g_ftpServer;

static BOOL s_bFileCacheSet = FALSE;     // 0x00278D74

// The discovery broadcast other dashboards send ("ping") and the port it
// arrives on; the same one ScanNetworkClients() uses.
static const WORD kDiscoveryPort = 4905;

static unsigned __stdcall ListenThread(void *param)   // 0x000BB225
{
	return ((CFTPServer *)param)->ListenThreadProc();
}

// 0x000B64CD
CFTPServer::CFTPServer()
{
	m_nPort = 21;
	m_nMaxUsers = 2;
	m_strWelcome = "Welcome to XBOX FTP Server";
	m_strGoodbye = "Bye";
	m_unknown098 = 0;
	m_bRunning = FALSE;
	m_nCurrentUsers = 0;
	m_dwBytesSent = 0;
	m_dwBytesReceived = 0;
	m_dwTotalConnections = 0;
	m_dwFilesSent = 0;
	m_dwFilesReceived = 0;
	m_dwFailedSends = 0;
	m_dwFailedReceives = 0;
	m_hStopEvent = NULL;
	m_hThread = NULL;
	m_strUser = "Xbox";
	m_strPassword = "xbox";
	m_pEvents = NULL;
	m_dwPasvPortMin = 0x400;
	m_dwPasvPortMax = 0xFFFF;
	m_strSyst.clear();
	m_bBoost = TRUE;
	m_nActiveTransfers = 0;
	m_bPaused = FALSE;
	m_hPauseEvent = NULL;
	// m_listenSocket, m_bStopping and m_bAllowAnonymous are left to the
	// zero-initialized global.
	m_pSettings = CAppSettings::GetInstance();
}

// 0x000B3FB6
CFTPServer::~CFTPServer()
{
	Stop();
	if (m_hStopEvent != NULL)
	{
		CloseHandle(m_hStopEvent);
		m_hStopEvent = NULL;
	}
	if (m_hThread != NULL)
	{
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	// 0x0002F967. Quirk: advances two nodes for each one it removes, so every
	// other connection stays in the list (and is never released).
	std::list<CFTPConnection *>::iterator it = m_connections.begin();
	while (it != m_connections.end())
	{
		CFTPConnection *connection = *it;
		std::list<CFTPConnection *>::iterator next = m_connections.erase(it);
		connection->Release();
		if (next == m_connections.end())
			next = m_connections.begin();
		if (next == m_connections.end())
			break;
		it = ++next;
	}
}

// 0x000B39A8
BOOL CFTPServer::IsRunning() const
{
	return m_bRunning;
}

// 0x000B3997
void CFTPServer::SetPort(int port)
{
	m_nPort = port;
}

// 0x000B399B
void CFTPServer::SetMaxUsers(int maxUsers)
{
	if (maxUsers > 0 && maxUsers < 16)
		m_nMaxUsers = maxUsers;
}

// 0x000B3E13
void CFTPServer::SetWelcome(const char *text)
{
	m_strWelcome = text;
}

// 0x000B3E55
void CFTPServer::SetUser(const char *user)
{
	if (user != NULL && user[0] != '\0')
		m_strUser = user;
}

// 0x000B3E20
void CFTPServer::SetPassword(const char *password)
{
	if (password != NULL && password[0] != '\0')
		m_strPassword = password;
}

// 0x0002B8DD
void CFTPServer::SetPasvPortMin(DWORD port)
{
	if (port < 0x400 || port > 0x10000)
		port = port < 0x400 ? 0x400 : 0x10000;
	m_dwPasvPortMin = port;
}

// 0x0002B8FD
void CFTPServer::SetPasvPortMax(DWORD port)
{
	if (port < 0x400 || port > 0x10000)
		port = port < 0x400 ? 0x400 : 0x10000;
	m_dwPasvPortMax = port;
}

// 0x0002B91D
void CFTPServer::SetSyst(const char *syst)
{
	if (syst == NULL || syst[0] == '\0')
		return;

	m_strSyst = _strdup(syst);   // Quirk: the copy is never freed
	if (StrLeft(m_strSyst, 5) != "UNIX ")
	{
		StrFormat(m_strSyst, "UNIX / UX %s", syst);
	}
	else if (m_strSyst.find("/ UX ") == std::string::npos &&
	         m_strSyst.find("UnleashX") == std::string::npos)
	{
		StrReplaceAll(m_strSyst, "UNIX", "UNIX / UX");
	}
}

// 0x000B3907
void CFTPServer::SetEventSink(CFTPEventSink *sink)
{
	m_pEvents = sink;
}

// 0x000B390B
void CFTPServer::Pause(BOOL pause)
{
	m_bPaused = pause;
	if (pause)
	{
		if (m_hPauseEvent == NULL)
			m_hPauseEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
		SuspendThread(m_hThread);
		return;
	}

	ResumeThread(m_hThread);
	Sleep(5);
	if (m_hPauseEvent != NULL)
		SetEvent(m_hPauseEvent);
	SwitchToThread();
}

// 0x000B38E7
BOOL CFTPServer::TooManyUsers()
{
	if (m_nCurrentUsers < 0)
		m_nCurrentUsers = 0;
	return m_nMaxUsers > 0 && m_nMaxUsers < m_nCurrentUsers;
}

// 0x000B3969
void CFTPServer::SetTransferActive(BOOL active)
{
	if (active)
		m_nActiveTransfers++;
	else if (m_nActiveTransfers != 0)
		m_nActiveTransfers--;
}

// 0x000B3E8A
// The user name is compared without case, the password with case. The
// built-in defaults apply if the configured ones were cleared.
HRESULT CFTPServer::CheckLogin(const char *user, const char *password, std::string *home)
{
	HRESULT hr = E_FAIL;
	std::string name = user;
	std::string pass = password;
	StrTrimRight(StrTrimLeft(name));
	StrTrimRight(StrTrimLeft(pass));

	if (name.size() != 0 && pass.size() != 0)
	{
		if (m_bAllowAnonymous && _mbsicmp(name.c_str(), "anonymous") == 0)
		{
			*home = m_strAnonymousHome;
			return S_OK;
		}
		if (m_strUser.size() == 0)
			m_strUser = "Xbox";
		if (m_strPassword.size() == 0)
			m_strPassword = "xbox";
		if (StrCmpNoCase(name, m_strUser.c_str()) == 0 && pass.compare(m_strPassword) == 0)
			hr = S_OK;
	}
	return hr;
}

// 0x0002F9A7
BOOL CFTPServer::AddConnection(CFTPConnection *connection)
{
	if (connection == NULL)
		return FALSE;
	connection->AddRef();
	m_connections.push_back(connection);
	return TRUE;
}

// 0x0002F9CC
CFTPConnection *CFTPServer::RemoveConnection(HANDLE thread, BOOL keep)
{
	for (std::list<CFTPConnection *>::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
	{
		if ((*it)->m_hThread == thread)
		{
			CFTPConnection *connection = *it;
			m_connections.erase(it);
			if (!keep)
				connection->Release();
			return connection;
		}
	}
	return NULL;
}

// 0x0002FA15
void CFTPServer::AddStat(int stat, DWORD amount)
{
	if (amount == 0)
		return;

	DWORD value = amount;
	switch (stat)
	{
	case FTPSTAT_BYTES_SENT:
		value = (m_dwBytesSent += amount);
		break;
	case FTPSTAT_BYTES_RECEIVED:
		value = (m_dwBytesReceived += amount);
		break;
	case FTPSTAT_FILES_SENT:
		m_dwFilesSent += amount;
		break;
	case FTPSTAT_FILES_RECEIVED:
		m_dwFilesReceived += amount;
		break;
	case FTPSTAT_FAILED_SENDS:
		m_dwFailedSends += amount;
		break;
	case FTPSTAT_FAILED_RECEIVES:
		m_dwFailedReceives += amount;
		// Quirk: notifies here without checking for a sink, and again below.
		m_pEvents->OnStatChanged(FTPSTAT_FAILED_RECEIVES, amount);
		break;
	}

	if (m_pEvents != NULL)
	{
		if (stat == FTPSTAT_BYTES_SENT)
			m_pEvents->OnBytesSent(value);
		else if (stat == FTPSTAT_BYTES_RECEIVED)
			m_pEvents->OnBytesReceived(value);
		else
			m_pEvents->OnStatChanged(stat, value);
	}
	m_activityTimer.Start();
}

// 0x0002FAAC
void CFTPServer::NotifyLogin(const char *user, const char *ip)
{
	if (m_pEvents != NULL)
	{
		m_pEvents->OnLogin(user, ip);
		m_pEvents->OnStatChanged(FTPSTAT_CURRENT_USERS, m_nCurrentUsers);
	}
	m_activityTimer.Start();
}

// 0x0002FAE1
void CFTPServer::NotifyLogout(const char *user)
{
	// The last user leaving undoes a Pause() a boost transfer may have left.
	if (m_nCurrentUsers > 0 && --m_nCurrentUsers == 0)
		Pause(FALSE);
	if (m_pEvents != NULL)
	{
		m_pEvents->OnLogout(user);
		m_pEvents->OnStatChanged(FTPSTAT_CURRENT_USERS, m_nCurrentUsers);
	}
	m_activityTimer.Start();
}

// 0x0002FB2C
int CFTPServer::NotifySiteCommand(const char *command, std::string *reply)
{
	int code = 0;
	if (m_pEvents != NULL)
		code = m_pEvents->OnSiteCommand(command, reply);
	m_activityTimer.Start();
	return code;
}

// 0x0002FB58
int CFTPServer::NotifyExec(const char *name, const char *args)
{
	int code = 0;
	if (m_pEvents != NULL)
		code = m_pEvents->OnExec(name, args);
	m_activityTimer.Start();
	return code;
}

// 0x000BB22A
HRESULT CFTPServer::Start()
{
	if (m_bRunning)
		return TRUE;
	if (!IsNetworkReady())
		return FALSE;

	// Transfers go through the file cache; make it at least 6.25 MB once.
	if (!s_bFileCacheSet)
	{
		if ((XGetFileCacheSize() & 0xFFFF0000) < 0x640000)
			XSetFileCacheSize(0x640000);
		s_bFileCacheSet = TRUE;
	}

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_listenSocket != INVALID_SOCKET)
	{
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons((WORD)m_nPort);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		char reuse = 1;
		setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, 1);
		if (bind(m_listenSocket, (sockaddr *)&addr, sizeof(addr)) == 0)
		{
			unsigned long nonBlocking = 1;
			ioctlsocket(m_listenSocket, FIONBIO, &nonBlocking);
			int backlog = 0x7FFFFFFF;
			if (m_nMaxUsers > 0x7FFFFFFF)   // never
				backlog = m_nMaxUsers + 1;
			if (listen(m_listenSocket, backlog) == 0)
			{
				m_dwBytesReceived = 0;
				m_dwBytesSent = 0;
				m_nCurrentUsers = 0;
				m_dwTotalConnections = 0;
				m_dwFilesSent = 0;
				m_dwFilesReceived = 0;
				m_dwFailedSends = 0;
				m_dwFailedReceives = 0;
				if (m_hStopEvent == NULL)
					m_hStopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
				else
					ResetEvent(m_hStopEvent);
				if (m_hThread == NULL)
					m_hThread = (HANDLE)_beginthreadex(NULL, 0, ListenThread, this, CREATE_SUSPENDED, NULL);
				if (m_hThread != NULL && m_hThread != INVALID_HANDLE_VALUE)
				{
					ResumeThread(m_hThread);
					m_bRunning = TRUE;
				}
			}
		}
	}

	if (!m_bRunning && m_listenSocket != INVALID_SOCKET)
	{
		shutdown(m_listenSocket, SD_BOTH);
		closesocket(m_listenSocket);
	}
	return m_bRunning;
}

// 0x000B3C5D
void CFTPServer::Stop()
{
	if (!m_bRunning)
		return;

	if (m_hThread != NULL)
		ResumeThread(m_hThread);   // in case boost mode paused it
	m_bRunning = FALSE;
	m_bStopping = TRUE;
	if (m_hStopEvent != NULL)
		SetEvent(m_hStopEvent);
	Sleep(500);

	// The connection threads notice this after their current command.
	for (std::list<CFTPConnection *>::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		(*it)->m_bConnected = FALSE;

	if (m_listenSocket != INVALID_SOCKET)
	{
		shutdown(m_listenSocket, SD_BOTH);
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
	}
	WSACleanup();
	if (m_hThread != NULL)
	{
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
}

// 0x000BADB3
// Accepts connections about once a second. While nobody connects it answers
// "ping" broadcasts on UDP 4905 (when network auto-detect is on) with
// "<nickname>;<user>;<password>;<port>;<boost>", which is how other
// dashboards list this Xbox as an FTP site. Quirk: that hands the FTP
// password to anyone on the LAN who asks.
unsigned CFTPServer::ListenThreadProc()
{
	int addrLen = sizeof(sockaddr_in);
	DWORD nextDiscovery = 0;          // never updated
	BOOL discoveryReady = FALSE;
	SOCKET discoverySocket = INVALID_SOCKET;
	sockaddr_in discoveryAddr;
	memset(&discoveryAddr, 0, sizeof(discoveryAddr));
	std::string nickname;

	m_bStopping = FALSE;
	while (!m_bStopping && m_listenSocket != INVALID_SOCKET)
	{
		if (m_hPauseEvent != NULL && WaitForSingleObject(m_hPauseEvent, INFINITE) == WAIT_OBJECT_0)
		{
			// Pause(FALSE) set the event.
			CloseHandle(m_hPauseEvent);
			m_hPauseEvent = NULL;
			continue;
		}

		if (m_nActiveTransfers != 0)
			Sleep(1000);

		SOCKET s = accept(m_listenSocket, NULL, (socklen_t *)&addrLen);
		if (s == INVALID_SOCKET)
		{
			if (m_pSettings->m_network.m_bAutoDetect && GetTickCount() > nextDiscovery)
			{
				if (discoverySocket == INVALID_SOCKET)
				{
					discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
					if (discoverySocket != INVALID_SOCKET)
					{
						discoveryAddr.sin_family = AF_INET;
						discoveryAddr.sin_addr.s_addr = 0;
						discoveryAddr.sin_port = htons(kDiscoveryPort);
						char reuse = 1;
						setsockopt(discoverySocket, SOL_SOCKET, SO_REUSEADDR, &reuse, 1);
						int broadcast = 1;
						int timeout = 1500;
						setsockopt(discoverySocket, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast, 4);
						setsockopt(discoverySocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, 4);
						if (bind(discoverySocket, (sockaddr *)&discoveryAddr, sizeof(discoveryAddr)) == 0)
						{
							unsigned long nonBlocking = 1;
							ioctlsocket(discoverySocket, FIONBIO, &nonBlocking);
							discoveryReady = TRUE;
						}
					}
				}
				else if (discoveryReady)
				{
					char request[0x41];
					memset(request, 0, sizeof(request));
					sockaddr_in from;
					int fromLen = sizeof(from);
					int n = recvfrom(discoverySocket, request, 0x40, 0, (sockaddr *)&from, (socklen_t *)&fromLen);
					if (n != -1 && _stricmp(request, "ping") == 0)
					{
						if (nickname.size() == 0)
						{
							wchar_t name[0x20];
							HANDLE find = XFindFirstNicknameW(FALSE, name, 0x20);
							if (find != INVALID_HANDLE_VALUE)
								CloseHandle(find);
							nickname = StrFromWide(name);
							nickname = StrTrimRight(StrTrimLeft(nickname));
						}

						std::string answer;
						StrFormat(answer, "%s;%s;%s;%d;%d\r\n", nickname.c_str(), m_strUser.c_str(),
						          m_strPassword.c_str(), m_nPort, m_bBoost != 0);
						sockaddr_in to;
						to.sin_family = AF_INET;
						to.sin_addr.s_addr = 0xFFFFFFFF;
						to.sin_port = htons(kDiscoveryPort);
						sendto(discoverySocket, answer.c_str(), answer.size(), 0, (sockaddr *)&to, sizeof(to));
						// and once more straight to whoever asked
						to.sin_addr = from.sin_addr;
						to.sin_port = from.sin_port;
						sendto(discoverySocket, answer.c_str(), answer.size(), 0, (sockaddr *)&to, sizeof(to));
					}
				}
			}
		}
		else
		{
			unsigned long blocking = 0;
			ioctlsocket(s, FIONBIO, &blocking);
			m_bRunning = TRUE;
			addrLen = sizeof(sockaddr_in);
			CFTPConnection *connection = new CFTPConnection(s, this);
			if (connection != NULL)
			{
				sockaddr_in peer;
				getpeername(s, (sockaddr *)&peer, (socklen_t *)&addrLen);
				connection->SetClientAddress(&peer);
				connection->StartThread();
				if (!AddConnection(connection))
				{
					delete connection;
				}
				else
				{
					m_dwTotalConnections++;
					if (m_pEvents != NULL)
						m_pEvents->OnStatChanged(FTPSTAT_TOTAL_CONNECTIONS, m_dwTotalConnections);
					m_nCurrentUsers++;
					connection->Resume();
				}
			}
			Sleep(500);
		}

		if (m_hStopEvent != NULL && WaitForSingleObject(m_hStopEvent, 1000) == WAIT_OBJECT_0)
			break;
		Sleep(750);
	}

	if (discoverySocket != INVALID_SOCKET)
	{
		shutdown(discoverySocket, SD_BOTH);
		closesocket(discoverySocket);
	}
	return 0;
}
