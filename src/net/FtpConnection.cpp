// CFTPConnection: one FTP control connection. Its thread logs the user in,
// then reads commands and dispatches them until the client quits, the
// connection drops or it has been idle for five minutes. Transfers are in
// FtpTransfer.cpp.
#include "FtpServer.h"

#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../render/D3D.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/FileIO.h"
#include "../system/Storage.h"
#include "../system/SystemInfo.h"
#include "Network.h"

static const char *const s_months[13] =   // 0x0026BB00
{
	" ", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// The command table, 0x0026CE20. Commands are matched by name; id is what
// the dispatcher switches on (RSYST and SPRT are out of order).
enum FtpCommandId
{
	FTPCMD_ABOR, FTPCMD_APPE, FTPCMD_BYE, FTPCMD_CDUP, FTPCMD_CWD, FTPCMD_DELE, FTPCMD_EXEC,
	FTPCMD_HELP, FTPCMD_LIST, FTPCMD_MKD, FTPCMD_NLST, FTPCMD_NOOP, FTPCMD_PASS, FTPCMD_PASV,
	FTPCMD_PASW, FTPCMD_PORT, FTPCMD_PWD, FTPCMD_QUIT, FTPCMD_REST, FTPCMD_RETR, FTPCMD_RMD,
	FTPCMD_RNFR, FTPCMD_RNTO, FTPCMD_SIZE, FTPCMD_STOR, FTPCMD_SYST, FTPCMD_TYPE, FTPCMD_USER,
	FTPCMD_XCUP, FTPCMD_XCWD, FTPCMD_XMKD, FTPCMD_XPWD, FTPCMD_XRMD, FTPCMD_SITE, FTPCMD_RSYST,
	FTPCMD_SPRT, FTPCMD_COUNT
};

struct SFtpCommand
{
	int id;
	const char *name;
	BOOL needsArg;
	const char *help;
};

static const SFtpCommand s_commands[FTPCMD_COUNT] =
{
	{ FTPCMD_ABOR, "ABOR", FALSE, "Abort transfer: ABOR" },
	{ FTPCMD_APPE, "APPE", TRUE, "Append file to existing file: APPE file-name" },
	{ FTPCMD_BYE, "BYE", FALSE, "Logout or break the connection: BYE" },
	{ FTPCMD_CDUP, "CDUP", FALSE, "Change to parent directory: CDUP" },
	{ FTPCMD_CWD, "CWD", TRUE, "Change working directory: CWD [directory-name]" },
	{ FTPCMD_DELE, "DELE", TRUE, "Delete file: DELE file-name" },
	{ FTPCMD_EXEC, "EXEC", TRUE, "Execute raw command: EXEC [xbe-name]" },
	{ FTPCMD_HELP, "HELP", FALSE, "Show help: HELP [command]" },
	{ FTPCMD_LIST, "LIST", FALSE, "Get directory listing: LIST [path-name]" },
	{ FTPCMD_MKD, "MKD", TRUE, "Make directory: MKD path-name" },
	{ FTPCMD_NLST, "NLST", FALSE, "List filenames only: NLST [path-name]" },
	{ FTPCMD_NOOP, "NOOP", FALSE, "Do nothing: NOOP" },
	{ FTPCMD_PASS, "PASS", TRUE, "Supply a user password: PASS password" },
	{ FTPCMD_PASV, "PASV", FALSE, "Set server in passive mode: PASV" },
	{ FTPCMD_PASW, "PASW", FALSE, "Set server in passive mode: P@SW" },
	{ FTPCMD_PORT, "PORT", TRUE, "Specify the client port number: PORT a0,a1,a2,a3,a4,a5" },
	{ FTPCMD_PWD, "PWD", FALSE, "Get current directory: PWD" },
	{ FTPCMD_QUIT, "QUIT", FALSE, "Logout or break the connection: QUIT" },
	{ FTPCMD_REST, "REST", TRUE, "Set restart transfer marker: REST marker" },
	{ FTPCMD_RETR, "RETR", TRUE, "Get file: RETR file-name" },
	{ FTPCMD_RMD, "RMD", TRUE, "Remove directory: RMD path-name" },
	{ FTPCMD_RNFR, "RNFR", TRUE, "Specify old path name of file to be renamed: RNFR file-name" },
	{ FTPCMD_RNTO, "RNTO", TRUE, "Specify new path name of file to be renamed: RNTO file-name" },
	{ FTPCMD_SIZE, "SIZE", TRUE, "Get filesize: SIZE file-name" },
	{ FTPCMD_STOR, "STOR", TRUE, "Store file: STOR file-name" },
	{ FTPCMD_SYST, "SYST", FALSE, "Get operating system type: SYST" },
	{ FTPCMD_RSYST, "RSYST", FALSE, "Get operating system type: RSYST" },
	{ FTPCMD_SPRT, "SPRT", TRUE, "SPRT" },
	{ FTPCMD_TYPE, "TYPE", TRUE, "Set filetype: TYPE [A | I]" },
	{ FTPCMD_USER, "USER", TRUE, "Supply a username: USER username" },
	{ FTPCMD_XCUP, "XCUP", FALSE, "Change to parent directory: XCUP" },
	{ FTPCMD_XCWD, "XCWD", TRUE, "Change working directory: XCWD [directory-name]" },
	{ FTPCMD_XMKD, "XMKD", TRUE, "Make directory: XMKD path-name" },
	{ FTPCMD_XPWD, "XPWD", FALSE, "Get current directory: XPWD" },
	{ FTPCMD_XRMD, "XRMD", TRUE, "Remove directory: XRMD path-name" },
	{ FTPCMD_SITE, "SITE", TRUE,
	  "execute Site commands: REBOOT, SHUTDOWN, POWERCYCLE, TRAYCLOSE, EJECT or TRAYOPEN, FTPRESET, NETRESET" },
};

static unsigned __stdcall ConnectionThread(void *param)   // 0x000BAD77
{
	return ((CFTPConnection *)param)->ThreadProc();
}

// 0x000B7FD0
CFTPConnection::CFTPConnection(SOCKET socket, CFTPServer *server)
{
	m_pServer = server;
	m_pTransferBuffer = NULL;
	m_hThread = NULL;
	m_nRefCount = 0;
	m_dataSocket = INVALID_SOCKET;
	m_bConnected = FALSE;
	m_bPassive = FALSE;
	m_bCanDelete = TRUE;
	m_wPasvPort = 0;
	m_dwRestOffset = 0;
	m_ctrlSocket = socket;
	srand(GetTickCount());
	memset(m_recvBuffer, 0, sizeof(m_recvBuffer));
	m_pTransferBuffer = new BYTE[0x4000];

	DWORD ip = GetIPAddress();
	m_localIp[0] = (BYTE)ip;
	m_localIp[1] = (BYTE)(ip >> 8);
	m_localIp[2] = (BYTE)(ip >> 16);
	m_localIp[3] = (BYTE)(ip >> 24);
	m_unknown014 = 0;
	m_bAbort = FALSE;
	m_localDataPort[0] = 0;
	m_localDataPort[1] = (BYTE)(server->m_nPort - 1);
	m_strCwd = "/";
	m_unknown4D8 = 1;
	m_files.clear();
	memset(m_unknown4EC, 0, sizeof(m_unknown4EC));
	m_bBoostMode = FALSE;
	m_nActiveTransfers = 0;
	m_wDataPort = 0;
	m_strTitleImageDir.clear();
	m_pBoostBuffer[0] = NULL;
	m_pBoostBuffer[1] = NULL;
	m_nBoostBuffer = 0;
	m_bBoostReceiveDone = FALSE;
	m_hBoostEvent = NULL;
	m_bPriorityRaised = FALSE;
	m_pSettings = CAppSettings::GetInstance();
	// m_bAnonymous is set by Login() before any command runs.
}

// 0x000B3CF5
CFTPConnection::~CFTPConnection()
{
	CloseDataConnection();
	m_bConnected = FALSE;
	if (m_ctrlSocket != INVALID_SOCKET)
	{
		shutdown(m_ctrlSocket, SD_BOTH);
		closesocket(m_ctrlSocket);
		m_ctrlSocket = INVALID_SOCKET;
	}
	delete[] m_pTransferBuffer;
	if (m_hThread != NULL)
	{
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
	// Quirk: boost buffers come from XPhysicalAlloc but would be freed with
	// operator delete here; BoostReceive() frees and clears them itself.
	if (m_pBoostBuffer[0] != NULL)
	{
		operator delete(m_pBoostBuffer[0]);
		m_pBoostBuffer[0] = NULL;
	}
	if (m_pBoostBuffer[1] != NULL)
	{
		operator delete(m_pBoostBuffer[1]);
		m_pBoostBuffer[1] = NULL;
	}
}

// 0x000B39AF
LONG CFTPConnection::AddRef()
{
	return ++m_nRefCount;
}

// 0x000B3DD4
LONG CFTPConnection::Release()
{
	LONG count = --m_nRefCount;
	if (count < 1)
	{
		delete this;
		return 0;
	}
	return count;
}

// 0x000BAD81
BOOL CFTPConnection::StartThread()
{
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, ConnectionThread, this, CREATE_SUSPENDED, NULL);
	if (m_hThread == NULL || m_hThread == INVALID_HANDLE_VALUE)
		return FALSE;
	SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);
	return TRUE;
}

// 0x000B3AF1
// Quirk: the port bytes are the high byte of the port divided by 255 and
// that byte modulo 255 (meant to be port / 256 and port % 256). They're only
// used for an active data connection without a PORT command.
void CFTPConnection::SetClientAddress(const sockaddr_in *addr)
{
	if (addr == NULL)
		return;
	const BYTE *ip = (const BYTE *)&addr->sin_addr;
	m_clientIp[0] = ip[0];
	m_clientIp[1] = ip[1];
	m_clientIp[2] = ip[2];
	m_clientIp[3] = ip[3];
	BYTE portHigh = ((const BYTE *)&addr->sin_port)[0];
	m_clientPort[0] = portHigh / 0xFF;
	m_clientPort[1] = portHigh % 0xFF;
}

// 0x000B3A51
// Sends all of data. Returns S_OK, or E_FAIL on a socket error.
HRESULT FtpSendAll(SOCKET s, const char *data, int length)
{
	for (;;)
	{
		int sent = send(s, data, length, 0);
		if (sent > 0 && sent < length)
		{
			data += sent;
			length -= sent;
			continue;
		}
		if (sent == length)
			return S_OK;
		if (sent == -1)
		{
			SetLastError(WSAGetLastError());
			return E_FAIL;
		}
		// sent == 0: try again
	}
}

// 0x000B39F3
// One recv() into m_recvBuffer (at most 0x3FF bytes, zero-terminated). A
// closed or reset connection clears m_bConnected.
HRESULT FtpRecvSome(CFTPConnection *connection, SOCKET s)
{
	int received = recv(s, connection->m_recvBuffer, 0x3FF, 0);
	if (received > 0)
	{
		connection->m_recvBuffer[received] = '\0';
		return S_OK;
	}
	if (received == 0)
	{
		connection->m_bConnected = FALSE;
	}
	else if (received == -1)
	{
		int error = WSAGetLastError();
		if (error == WSAECONNRESET || error == WSAENOTCONN || error == WSAESHUTDOWN || error == WSAETIMEDOUT)
			connection->m_bConnected = FALSE;
	}
	return E_FAIL;
}

// 0x000B44FF
// Reads until the text has a '\r' (or a '\n', or is 4 KB long). Quirk: once
// the first piece arrived, a failing recv() doesn't end the loop, so a client
// that disconnects in the middle of a line leaves the thread spinning.
static BOOL ReadLine(CFTPConnection *connection, std::string *line)
{
	memset(connection->m_recvBuffer, 0, sizeof(connection->m_recvBuffer));
	if (FtpRecvSome(connection, connection->m_ctrlSocket) != S_OK)
		return FALSE;

	*line = connection->m_recvBuffer;
	while (line->find('\r') == std::string::npos)
	{
		if (line->find('\n') != std::string::npos || (int)line->size() >= 0x1000)
			break;
		memset(connection->m_recvBuffer, 0, sizeof(connection->m_recvBuffer));
		if (FtpRecvSome(connection, connection->m_ctrlSocket) == S_OK)
			StrAppend(*line, connection->m_recvBuffer);
	}
	return TRUE;
}

// 0x000B45B1
// Quirk: whether fmt is run through vsnprintf is decided by strlen() of the
// bytes of the first variadic argument slot, which is whatever the caller
// had on the stack when it passed no arguments. Callers often pass text
// that was already formatted (and may contain user input) as fmt.
void CFTPConnection::Reply(int code, const char *fmt, ...)
{
	std::string line;
	if (fmt != NULL && fmt[0] != '\0')
	{
		std::string text;
		va_list args;
		va_start(args, fmt);
		StrFormatV(text, fmt, args);
		va_end(args);
		if (code > 10)
			StrFormat(line, "%d %s\r\n", code, text.c_str());
		else
			StrFormat(line, "%s\r\n", text.c_str());
	}
	else
	{
		StrFormat(line, "%d\r\n", code);
	}

	if (FtpSendAll(m_ctrlSocket, line.c_str(), line.size()) < 0)
	{
		m_bConnected = FALSE;
	}
	else if (line.size() != 0)
	{
		CFTPServer *server = m_pServer;
		server->m_dwBytesSent += line.size();
		if (server->m_pEvents != NULL)
			server->m_pEvents->OnBytesSent(server->m_dwBytesSent);
		server->m_activityTimer.Start();
	}
}

// 0x000B489E
void CFTPConnection::ReplyNotImplemented(const char *text)
{
	if (text == NULL || text[0] == '\0')
		text = "Command not implemented.";
	Reply(502, text);
}

// 0x000B4729
void CFTPConnection::SendWelcome()
{
	std::string welcome = m_pServer->m_strWelcome;
	Reply(230, welcome.c_str());
}

// 0x000B4046
void CFTPConnection::ResolvePath(std::string *out, const char *cwd, const char *arg)
{
	std::string dir = cwd;
	std::string path = arg != NULL ? arg : "";
	StrReplaceAll(dir, ":", "");
	StrReplaceAll(path, ":", "");
	if (path.size() == 0)
	{
		*out = cwd;
		return;
	}

	StrReplaceAll(path, "\\", "/");
	while (StrReplaceAll(path, "//", "/") != 0)
		;
	StrTrimRightChars(path, "/");
	if (StrLeft(path, 1) != "/")
	{
		if (dir.size() == 0)
			dir = m_strCwd;
		// Quirk: the original cwd (with any ':' and possibly empty) is what
		// gets used, the cleaned-up copy only picks the separator.
		if (StrRight(dir, 1) != "/")
			StrFormat(path, "%s/%s", cwd, path.c_str());
		else
			StrFormat(path, "%s%s", cwd, path.c_str());
	}
	*out = path;
}

// 0x000B4419
// The first path component is the drive. Quirk: it's uppercased only if more
// path follows ("/c" gives "c:\"), and nothing checks the 260-byte buffer.
void CFTPConnection::FtpPathToXbox(std::string *out, const char *ftpPath)
{
	char buffer[260];
	memset(buffer, 0, sizeof(buffer));
	int length = 0;

	int skip = 0;
	while (ftpPath[skip] == '/')
		skip++;
	const char *p = ftpPath + skip;
	const char *slash = strchr(p, '/');
	if (slash == NULL)
	{
		for (; *p != '\0'; p++)
			buffer[length++] = *p;
		buffer[length] = ':';
		buffer[length + 1] = '\\';
		buffer[length + 2] = '\0';
		length += 2;
	}
	else
	{
		for (; p < slash; p++)
			buffer[length++] = (char)toupper(*p);
		buffer[length] = ':';
		buffer[length + 1] = '\\';
		buffer[length + 2] = '\0';
		// the separator after the drive becomes the '\' after ':'
		for (p = slash; *p != '\0'; p++)
		{
			length++;
			buffer[length] = *p == '/' ? '\\' : *p;
		}
		length++;
	}
	buffer[length] = '\0';
	*out = buffer;
	StrReplaceAll(*out, "::", ":");
}

// 0x000B6848
// arg as a directory: an Xbox path if it contains ":\", else resolved
// against the current directory. Returns whether the directory exists.
BOOL CFTPConnection::ResolveDirPath(std::string *out, const char *arg)
{
	std::string ftpPath;
	std::string xboxPath;
	std::string argument = arg;
	if (argument.find(":\\") == std::string::npos)
	{
		ResolvePath(&ftpPath, m_strCwd.c_str(), argument.c_str());
		FtpPathToXbox(&xboxPath, ftpPath.c_str());
	}
	else
	{
		xboxPath = argument;
	}
	*out = xboxPath;
	return PathExists(xboxPath.c_str(), TRUE);
}

// 0x000B6929
BOOL CFTPConnection::ResolveFilePath(std::string *out, const char *arg, BOOL mustExist)
{
	std::string ftpPath;
	std::string xboxPath;
	std::string argument = arg;
	ResolvePath(&ftpPath, m_strCwd.c_str(), argument.c_str());
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	*out = xboxPath;
	if (!mustExist)
		return TRUE;
	return PathExists(xboxPath.c_str(), FALSE);
}

// 0x000B4769
// Reads a line and splits it at the first space (or tab) into the command,
// uppercased, and its argument.
HRESULT CFTPConnection::ReadCommand(std::string *command, std::string *arg)
{
	HRESULT hr = E_FAIL;
	std::string line;
	if (ReadLine(this, &line))
	{
		StrTrimRight(StrTrimLeft(line));
		int pos = StrFindChar(line, ' ');
		if (pos <= 0)
			pos = StrFindChar(line, '\t');
		if (pos > 0)
		{
			*command = StrLeft(line, pos);
			*arg = StrMid(line, pos + 1, line.size() - (pos + 1));
		}
		else
		{
			*command = line;
			*arg = "";
		}
		StrTrimRight(StrTrimLeft(*command));
		StrTrimRight(StrTrimLeft(*arg));
		StrMakeUpper(*command);

		CFTPServer *server = m_pServer;
		hr = S_OK;
		if (line.size() != 0)
		{
			server->m_dwBytesReceived += line.size();
			if (server->m_pEvents != NULL)
				server->m_pEvents->OnBytesReceived(server->m_dwBytesReceived);
			server->m_activityTimer.Start();
		}
	}
	return hr;
}

// 0x000B8166
// Sends the banner (client address, local time, server statistics and the
// space of every mounted drive) and runs USER/PASS until a login succeeds,
// the client quits or three passwords were wrong. Quirk: once USER was given,
// any other command (even a second USER) gets "Login with USER first.".
HRESULT CFTPConnection::Login()
{
	HRESULT hr = E_FAIL;
	BOOL gotUser = FALSE;
	BOOL loggedIn = FALSE;
	int failures = 0;
	std::string command;
	std::string arg;
	std::string banner;
	CFTPServer *server = m_pServer;

	StrFormat(command, "220-Client IP: %d.%d.%d.%d\r\n", m_clientIp[0], m_clientIp[1], m_clientIp[2],
	          m_clientIp[3]);
	banner.append(command);
	SYSTEMTIME now;
	GetLocalTime(&now);
	StrFormat(command, "220-Local Date and Time: %s %02d, %02d  %02d:%02d:%02d\r\n", s_months[now.wMonth],
	          now.wDay, now.wYear, now.wHour, now.wMinute, now.wSecond);
	banner.append(command);
	command = "220-\r\n220-Server Stats\r\n";
	banner.append(command);
	StrFormat(command, "220-Current Users: %u   Total Connections: %u\r\n", server->m_nCurrentUsers,
	          server->m_dwTotalConnections);
	banner.append(command);
	StrFormat(command, "220-Total Received: %d Bytes, %d Files, %d Failed\r\n", server->m_dwBytesReceived,
	          server->m_dwFilesReceived, server->m_dwFailedReceives);
	banner.append(command);
	StrFormat(command, "220-Total Sent: %d Bytes, %d Files, %d Failed\r\n", server->m_dwBytesSent,
	          server->m_dwFilesSent, server->m_dwFailedSends);
	banner.append(command);

	RefreshDrives(TRUE);
	for (int i = 0; i < DRIVE_INFO_COUNT; i++)
	{
		SDriveInfo *info = &g_drives[i];
		if (!info->mounted)
			continue;
		const double mb = 1.0 / 1048576;
		std::string total = FormatDouble(2, (double)(LONGLONG)info->totalBytes * mb);
		std::string free = FormatDouble(2, (double)(LONGLONG)info->freeBytes * mb);
		std::string used = FormatDouble(2, (double)(LONGLONG)info->usedBytes * mb);
		StrFormat(command, "220-Drive %c\t Used: %sMB\tFree: %sMB\tTotal: %sMB\r\n", info->letter, used.c_str(),
		          free.c_str(), total.c_str());
		banner.append(command);
	}
	Reply(0, banner.c_str());
	Reply(220, "UnleashX FTP Server ready.");

	while (!loggedIn)
	{
		if (ReadCommand(&command, &arg) < 0)
		{
			Reply(503, "Command Parse Error");
			break;
		}
		if (_mbsicmp(command.c_str(), "quit") == 0)
			break;

		if (!gotUser)
		{
			if (_mbsicmp(command.c_str(), "user") != 0)
			{
				Reply(503, "Login with USER first.");
			}
			else
			{
				StrTrimRight(StrTrimLeft(arg));
				if (arg.size() != 0)
				{
					if (server->m_bAllowAnonymous && _mbsicmp(arg.c_str(), "anonymous") == 0)
					{
						Reply(331, "Guest login ok, send email as password.");
					}
					else
					{
						std::string text;
						StrFormat(text, "Password required for %s", arg.c_str());
						Reply(331, text.c_str());
					}
					m_strUser = arg;
					gotUser = TRUE;
				}
			}
		}
		else if (_mbsicmp(command.c_str(), "pass") != 0)
		{
			Reply(503, "Login with USER first.");
		}
		else
		{
			StrTrimRight(StrTrimLeft(arg));
			if (arg.size() != 0)
			{
				if (server->CheckLogin(m_strUser.c_str(), arg.c_str(), &m_strHome) < 0)
				{
					failures++;
					Reply(530, "Not logged in, user or password incorrect!");
				}
				else
				{
					loggedIn = TRUE;
					m_strPassword = arg;
					hr = S_OK;
					m_bAnonymous = server->m_bAllowAnonymous && StrCmpNoCase(m_strUser, "anonymous") == 0;
					if (m_bAnonymous)
						m_bCanDelete = FALSE;
				}
			}
		}

		if (failures >= 3)
			break;
	}
	return hr;
}

// 0x000BAABB
unsigned CFTPConnection::ThreadProc()
{
	CFTPServer *server = m_pServer;
	server->NotifyLogin(NULL, NULL);

	if (server->TooManyUsers())
	{
		Reply(421, "Too many users are connected, please try again later.");
	}
	else if (Login() >= 0)
	{
		CXBStopWatch idleTimer;
		std::string ip;
		StrFormat(ip, "%d.%d.%d.%d", m_clientIp[0], m_clientIp[1], m_clientIp[2], m_clientIp[3]);
		server->NotifyLogin(m_strUser.c_str(), ip.c_str());
		m_strCwd = "/";
		m_bConnected = TRUE;
		SendWelcome();

		DWORD sendTimeout = 300000;
		DWORD recvTimeout = 305000;
		setsockopt(m_ctrlSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&sendTimeout, 4);
		setsockopt(m_ctrlSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recvTimeout, 4);
		idleTimer.Start();

		while (m_bConnected)
		{
			if (server->m_bPaused)
			{
				Sleep(500);
				continue;
			}

			int idleMs = (int)idleTimer.GetElapsedMilliseconds();
			std::string command;
			std::string arg;
			if (ReadCommand(&command, &arg) == S_OK)
			{
				if (command.size() != 0)
				{
					Dispatch(command, arg);
					idleTimer.Start();
					// a listing lets the raised priority drop right away
					idleMs = StrFind(command, "LIST") != -1 ? 8000 : 0;
				}
				else
				{
					Sleep(5);
				}
			}
			else
			{
				m_bConnected = FALSE;
				idleMs = (int)idleTimer.GetElapsedMilliseconds();
			}

			if (m_nActiveTransfers == 0)
			{
				if (idleMs > 300000)
				{
					Reply(421, "Connection timed out (%d seconds), closing control connection", 300);
					Sleep(1000);
					m_bConnected = FALSE;
				}
				if (m_bPriorityRaised && idleMs > 5000)
				{
					m_bPriorityRaised = FALSE;
					SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);
				}
			}
		}
		m_files.clear();
	}

	server->NotifyLogout(m_strUser.c_str());
	Close();
	return 0;
}

// 0x000B46F1
HRESULT CFTPConnection::Close()
{
	CloseDataConnection();
	if (m_ctrlSocket != INVALID_SOCKET)
	{
		shutdown(m_ctrlSocket, SD_BOTH);
		closesocket(m_ctrlSocket);
	}
	// releases the server's reference, which deletes this
	m_pServer->RemoveConnection(m_hThread, FALSE);
	return S_OK;
}

// 0x000BA451
// Commands are looked up case-sensitively (they're uppercased already), then
// without case. An argument is required where the table says so; ':' is
// removed from it and '\' becomes '/'. Commands without an argument have it
// cleared, except HELP. Quirk: the exemption meant for SITE checks the table
// index of XPWD instead, so SITE arguments are path-mangled too.
HRESULT CFTPConnection::Dispatch(std::string command, std::string arg)
{
	HRESULT hr = S_OK;
	if (command.size() == 0)
		return E_FAIL;

	int index;
	int id = FTPCMD_COUNT;
	for (int pass = 0; pass < 2 && id == FTPCMD_COUNT; pass++)
	{
		for (index = 0; index < FTPCMD_COUNT; index++)
		{
			const char *name = s_commands[index].name;
			if ((pass == 0 ? command.compare(name) : StrCmpNoCase(command, name)) != 0)
				continue;

			if (!s_commands[index].needsArg)
			{
				if (index != FTPCMD_HELP && arg.size() != 0)
					arg.clear();
			}
			else
			{
				if (arg.size() == 0)
				{
					Reply(501, "Syntax error in parameters or arguments.");
					return E_FAIL;
				}
				if (index != FTPCMD_XPWD)   // index 0x21, meant to be SITE
				{
					StrReplaceAll(arg, ":", "");
					StrReplaceAll(arg, "\\", "/");
				}
			}
			id = s_commands[index].id;
			break;
		}
	}

	switch (id)
	{
	case FTPCMD_ABOR:
		CmdAbor(arg);
		break;
	case FTPCMD_APPE:
		CmdAppe(arg);
		break;
	case FTPCMD_BYE:
	case FTPCMD_QUIT:
		CmdQuit(arg);
		break;
	case FTPCMD_CDUP:
	case FTPCMD_XCUP:
		CmdCdup(arg);
		break;
	case FTPCMD_CWD:
	case FTPCMD_XCWD:
		CmdCwd(arg);
		break;
	case FTPCMD_DELE:
		CmdDele(arg);
		break;
	case FTPCMD_EXEC:
		CmdExec(arg);
		break;
	case FTPCMD_HELP:
		if (arg.size() == 0)
		{
			std::string text;
			text = "The following commands are recognized:\r\n";
			for (int i = 0; i < FTPCMD_COUNT; i++)
			{
				text.append(s_commands[i].name);
				text.append((i + 1) % 4 == 0 ? "\r\n" : "\t");
			}
			text.append("\r\n");
			Reply(214, text.c_str());
			text = "HELP command successful.";
			Reply(214, text.c_str());
		}
		else
		{
			int i;
			for (i = 0; i < FTPCMD_COUNT; i++)
			{
				if (StrCmpNoCase(arg, s_commands[i].name) == 0)
					break;
			}
			if (i == FTPCMD_COUNT)
				Reply(501, "Unknown command \"%s\"", arg.c_str());
			else
				Reply(214, "%s", s_commands[i].help);
		}
		break;
	case FTPCMD_LIST:
		if (arg.size() != 0 && StrLeft(arg, 1) == "-")
			arg.clear();   // "LIST -al" lists the current directory
		CmdList(arg);
		break;
	case FTPCMD_MKD:
	case FTPCMD_XMKD:
		CmdMkd(arg);
		break;
	case FTPCMD_NLST:
		CmdNlst(arg);
		break;
	case FTPCMD_NOOP:
		CmdNoop(arg);
		break;
	case FTPCMD_PASS:
	case FTPCMD_USER:
		break;   // already logged in
	case FTPCMD_PASV:
	case FTPCMD_PASW:
		CmdPasv(arg);
		break;
	case FTPCMD_PORT:
		CmdPort(arg);
		break;
	case FTPCMD_PWD:
	case FTPCMD_XPWD:
		CmdPwd(arg);
		break;
	case FTPCMD_REST:
		CmdRest(arg);
		break;
	case FTPCMD_RETR:
		CmdRetr(arg);
		break;
	case FTPCMD_RMD:
	case FTPCMD_XRMD:
		CmdRmd(arg);
		break;
	case FTPCMD_RNFR:
		CmdRnfr(arg);
		break;
	case FTPCMD_RNTO:
		CmdRnto(arg);
		break;
	case FTPCMD_SIZE:
		CmdSize(arg);
		break;
	case FTPCMD_STOR:
		CmdStor(arg);
		break;
	case FTPCMD_SYST:
		CmdSyst(arg);
		break;
	case FTPCMD_TYPE:
		CmdType(arg);
		break;
	case FTPCMD_SITE:
		CmdSite(arg);
		break;
	case FTPCMD_RSYST:
		CmdRsyst(arg);
		break;
	case FTPCMD_SPRT:
		CmdSprt(arg);
		break;
	default:
	{
		hr = E_FAIL;
		std::string text;
		StrFormat(text, "\"%s\" not implemented - Try HELP.", command.c_str());
		ReplyNotImplemented(text.c_str());
		break;
	}
	}
	return hr;
}

// ---- simple commands ----

// 0x000B5526
void CFTPConnection::CmdAbor(std::string)
{
	m_bAbort = TRUE;
	Reply(226, "ABOR command successful.");
}

// 0x000B5843
void CFTPConnection::CmdQuit(std::string)
{
	std::string goodbye = m_pServer->m_strGoodbye;
	Reply(221, goodbye.c_str());
	m_bConnected = FALSE;
}

// 0x000B5896
void CFTPConnection::CmdCdup(std::string)
{
	if (m_strCwd.size() == 0)
	{
		m_strCwd = "/";
		Reply(250, "\"/\" is current directory.");
		return;
	}
	if (m_strCwd.compare("/") == 0)
	{
		Reply(250, "\"/\" is the top directory.");
		return;
	}

	int pos = StrReverseFind(m_strCwd, "/", -1);
	if (pos >= 1 && (int)m_strCwd.size() > 1 && pos == (int)m_strCwd.size() - 1)
	{
		// trailing '/'
		m_strCwd = StrLeft(m_strCwd, pos);
		pos = StrReverseFind(m_strCwd, "/", -1);
	}
	if (pos >= 1)
		m_strCwd = StrLeft(m_strCwd, pos);
	else if (pos == 0)
		m_strCwd = "/";

	std::string text;
	StrFormat(text, "\"%s\" is current directory.", m_strCwd.c_str());
	StrReplaceAll(text, "\\", "/");
	Reply(250, text.c_str());
}

// 0x000B69EE
// Quirk: a relative path is appended as given, so "CWD a/../b" keeps the
// "..", and nothing but ".." itself goes up.
void CFTPConnection::CmdCwd(std::string arg)
{
	std::string xboxPath;
	std::string unused;
	StrReplaceAll(arg, "\\", "/");
	if (arg.compare("/") == 0 || arg.compare("/..") == 0)
	{
		m_strCwd = "/";
		Reply(250, "CWD command successful.");
		return;
	}
	if (arg.compare("..") == 0)
	{
		CmdCdup(arg);
		return;
	}

	if (!ResolveDirPath(&xboxPath, arg.c_str()))
	{
		Reply(553, "\"%s\": Directory not found.", arg.c_str());
		return;
	}

	if (StrLeft(arg, 1) == "/")
	{
		m_strCwd = arg;
		StrReplaceAll(m_strCwd, ":", "");
		if ((int)m_strCwd.size() > 1 && StrRight(m_strCwd, 1) == "/")
			StrTrimRightChar(m_strCwd, '/');
		Reply(250, "CWD command successful.");
		return;
	}

	if ((int)(m_strCwd.size() + arg.size() + 1) >= 0x104)
	{
		Reply(501, "Path too long.");
		return;
	}
	if (StrRight(m_strCwd, 1) != "/")
		m_strCwd.append("/");
	m_strCwd.append(arg);
	if ((int)m_strCwd.size() > 1 && StrRight(m_strCwd, 1) == "/")
		StrTrimRightChar(m_strCwd, '/');
	Reply(250, "CWD command successful.");
}

// 0x000B53F5
void CFTPConnection::CmdDele(std::string arg)
{
	if (!m_bCanDelete || m_bAnonymous)
	{
		Reply(550, "Permission denied.");
		return;
	}

	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	std::string unused;
	SetFileAttributesA(xboxPath.c_str(), FILE_ATTRIBUTE_NORMAL);
	if (DeleteFileA(xboxPath.c_str()))
		Reply(250, "File \"%s\" was deleted successfully", arg.c_str());
	else
		Reply(450, "Internal error deleting the file: \"%s\".", arg.c_str());
}

// 0x000B5A20
// EXEC <name> [args] goes to the dashboard (CFTPEventReceiver::OnExec).
void CFTPConnection::CmdExec(std::string arg)
{
	if (m_bAnonymous)
	{
		Reply(550, "Permission denied - (Anonymous user).");
		return;
	}

	// Quirk: the current directory is resolved and then not used.
	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), NULL);
	FtpPathToXbox(&xboxPath, ftpPath.c_str());

	std::string name;
	std::string args;
	StrTrimRight(StrTrimLeft(arg));
	ftpPath.append("\\");
	StrReplaceAll(ftpPath, "\\\\", "\\");
	StrReplaceAll(ftpPath, "\\\\", "\\");
	int pos = StrFindChar(arg, ' ');
	if (pos < 0)
	{
		name = arg;
	}
	else
	{
		name = StrLeft(arg, pos);
		StrTrimRight(StrTrimLeft(name));
		args = StrMid(arg, pos, arg.size() - pos);
		StrTrimRight(StrTrimLeft(args));
	}

	int code = m_pServer->NotifyExec(name.c_str(), args.c_str());
	if (code == 0)
		ReplyNotImplemented(NULL);
	else
		Reply(code, "Ok.");
}

// 0x000B51C7
void CFTPConnection::CmdMkd(std::string arg)
{
	if (m_bAnonymous)
	{
		Reply(550, "Permission denied - (Anonymous user).");
		return;
	}

	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	DWORD attributes = GetFileAttributesA(xboxPath.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		if (CreateDirectoryA(xboxPath.c_str(), NULL))
			Reply(250, "Directory created successfully.");
		else
			Reply(550, "Internal error creating the directory.");
	}
	else if (attributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		Reply(250, "Directory already exists.");
	}
	else
	{
		Reply(550, "Internal error creating the directory.");
	}
}

// 0x000B59FA
void CFTPConnection::CmdNoop(std::string)
{
	Reply(200, "OK.");
}

// 0x000B5C09
void CFTPConnection::CmdPwd(std::string)
{
	if (m_strCwd.size() == 0)
	{
		Reply(257, "\"/\" is current directory.");
		return;
	}
	std::string text;
	StrFormat(text, "\"%s\" is current directory.", m_strCwd.c_str());
	StrReplaceAll(text, "\\", "/");
	Reply(257, text.c_str());
}

// 0x000B5720
void CFTPConnection::CmdRest(std::string arg)
{
	m_dwRestOffset = atol(arg.c_str());
	std::string text;
	StrFormat(text, "Restarting at %d.", m_dwRestOffset);
	Reply(350, text.c_str());
}

// 0x000B52D5
void CFTPConnection::CmdRmd(std::string arg)
{
	if (!m_bCanDelete || m_bAnonymous)
	{
		Reply(550, "Permission denied.");
		return;
	}

	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	if (ftpPath.compare("/") == 0 || (int)ftpPath.size() < 4)
	{
		Reply(550, "Permission denied.");
		return;
	}
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	if ((int)xboxPath.size() > 3 && RemoveDirectoryA(xboxPath.c_str()))
	{
		Reply(250, "Directory deleted successfully.");
	}
	else if (GetLastError() == ERROR_DIR_NOT_EMPTY)
	{
		Reply(550, "Directory not empty.");
	}
	else
	{
		Reply(550, "Internal error deleting the directory.");
	}
}

// 0x000B564B
void CFTPConnection::CmdRnfr(std::string arg)
{
	if (m_bAnonymous)
	{
		Reply(550, "Permission denied - (Anonymous user).");
		return;
	}

	std::string ftpPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	FtpPathToXbox(&m_strRenameFrom, ftpPath.c_str());
	DWORD attributes = GetFileAttributesA(m_strRenameFrom.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
		Reply(550, "File/directory not found.");
	else if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		Reply(350, "Directory exists, ready for destination name.");
	else
		Reply(350, "File exists, ready for destination name.");
}

// 0x000B5550
// Quirk: m_strRenameFrom isn't cleared, so a second RNTO renames again.
void CFTPConnection::CmdRnto(std::string arg)
{
	if (m_bAnonymous)
	{
		Reply(550, "Permission denied - (Anonymous user).");
		return;
	}
	if (m_strRenameFrom.size() == 0)
	{
		Reply(503, "Bad sequence of commands.");
		return;
	}

	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	if (MoveFileA(m_strRenameFrom.c_str(), xboxPath.c_str()))
		Reply(250, "Rename was successful.");
	else
		Reply(553, "Internal error. Rename was not successful.");
}

// 0x000B6DB4
void CFTPConnection::CmdSize(std::string arg)
{
	std::string xboxPath;
	if (!ResolveFilePath(&xboxPath, arg.c_str(), TRUE))
	{
		Reply(550, "File not found.");
		return;
	}
	WIN32_FILE_ATTRIBUTE_DATA data;
	GetFileAttributesExA(xboxPath.c_str(), GetFileExInfoStandard, &data);
	std::string text;
	StrFormat(text, "%u", data.nFileSizeLow);
	Reply(213, text.c_str());
}

// 0x000B4A14
// With boost mode allowed the server claims to be Avalaunch, which is how
// boost-capable clients recognize it.
void CFTPConnection::CmdSyst(std::string)
{
	CFTPServer *server = m_pServer;
	const char *text;
	if (server->m_bBoost)
		text = "UNIX emulated by [UnleashX] Avalaunch 0.47.5";
	else if (server->m_strSyst.size() == 0)
		text = "UNIX emulated by UnleashX FTP Server";
	else
		text = server->m_strSyst.c_str();
	Reply(215, text);
}

// 0x000B49EE
void CFTPConnection::CmdRsyst(std::string)
{
	Reply(215, "UNIX emulated by UnleashX FTP Server");
}

// 0x000B4995
void CFTPConnection::CmdSprt(std::string arg)
{
	if (arg.size() != 0 && StrCmpNoCase(arg.c_str(), "BOOST") == 0 && m_bBoostMode)
	{
		Reply(226, "Boost mode OK");
		return;
	}
	ReplyNotImplemented(NULL);
}

// 0x000B57D4
void CFTPConnection::CmdType(std::string arg)
{
	if (StrCmpNoCase(arg.c_str(), "A") == 0)
		Reply(200, "Type set to ASCII.");
	else if (StrCmpNoCase(arg.c_str(), "I") == 0)
		Reply(200, "Type set to IMAGE.");
	else
		Reply(504, "Bad parameter. Must be I for binary or A for ASCII type");
}

// ---- SITE ----

SFtpFileEntry MakeFtpFileEntry(BOOL directory, const std::string &name, DWORD size, DWORD attributes,
                                   DWORD creationTime)   // 0x0002F8FF
{
	SFtpFileEntry entry;
	entry.bDirectory = directory;
	entry.strName = name;
	entry.dwSize = size;
	entry.dwAttributes = attributes;
	entry.dwCreationTime = creationTime;
	return entry;
}

// 0x000B4204
// Adds root and, recursively, everything below it to m_files: a directory
// entry with the full path, then that directory's files (names only), then
// its subdirectories.
void CFTPConnection::BuildFileTree(std::string root)
{
	std::string dir = root;
	if (root.c_str() == NULL || dir.size() == 0)
		return;

	StrReplaceAll(dir, "\\\\?\\", "");
	m_files.push_back(MakeFtpFileEntry(TRUE, dir, 0, 0, 0));
	if (StrRight(dir, 1) != "\\")
		dir.append("\\");

	char pattern[0x10C];
	strcpy(pattern, dir.c_str());
	strcat(pattern, "*");
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirstFileA(pattern, &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				m_files.push_back(MakeFtpFileEntry(FALSE, data.cFileName, data.nFileSizeLow, data.dwFileAttributes,
				                                data.ftCreationTime.dwLowDateTime));
		} while (FindNextFileA(find, &data) == TRUE);
		CloseHandle(find);
	}

	find = FindFirstFileA(pattern, &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				std::string subdir = dir;
				subdir.append(data.cFileName);
				BuildFileTree(subdir);
			}
		} while (FindNextFileA(find, &data) == TRUE);
		CloseHandle(find);
	}
}

// 0x000B9D35
void CFTPConnection::CmdSite(std::string arg)
{
	if (m_bAnonymous)
	{
		Reply(550, "Permission denied - (Anonymous user).");
		return;
	}

	if (StrCmpNoCase(arg.c_str(), "FREEROOTSPACEDISABLE") == 0)
	{
		Reply(200, "OK");
		return;
	}

	// SITE BOOST <...>
	if (StrCmpNoCase(StrLeft(arg, 5), "BOOST") == 0)
	{
		std::string boostArg = StrMid(arg, 6, arg.size() - 6);
		SiteBoost(StrTrimRight(StrTrimLeft(boostArg)));
		return;
	}

	// SITE FREESPACE <drive>: free bytes of the drive, "0" if not mounted
	if (StrLeft(arg, 9).compare("FREESPACE") == 0 && arg.size() == 11)
	{
		SDriveInfo *info = GetDriveInfo(StrRight(arg, 1)[0]);
		std::string text;
		text = "0";
		if (info != NULL && info->mounted)
		{
			UpdateDriveSpace(info);
			StrFormat(text, "%I64u", info->freeBytes);
		}
		Reply(220, text.c_str());
		return;
	}

	// SITE MAKE_FILE_TREE <dir>: writes the tree to <drive>:\content.txt,
	// one "D,<path>" or "F,<name>,<size>,<attributes>,<created>" per line.
	if (StrLeft(arg, 14).compare("MAKE_FILE_TREE") == 0)
	{
		std::string path = StrMid(arg, 15, arg.size() - 15);
		// Quirk: only a path whose first '/' is its second character, and that
		// has no ':', is taken as an FTP path, and then only drive-converted.
		if (StrFind(path, "/") == 1 && StrFind(path, ":") == -1)
		{
			ResolvePath(&path, path.c_str(), NULL);
			FtpPathToXbox(&path, path.c_str());
		}
		if (!DirectoryExists(path.c_str()))
		{
			ReplyNotImplemented(NULL);
			return;
		}

		m_files.clear();
		BuildFileTree(path);
		if (m_files.size() < 2)
		{
			Reply(550, "Folder is Empty.");
			return;
		}

		// <drive>: is the first of Z:, X:, Y: with 1,000,000 bytes free.
		// Quirk: an unmounted X: or Y: is taken as is, and a missing one would
		// be dereferenced.
		SDriveInfo *drive = GetDriveInfo('Z');
		if (drive == NULL || !drive->mounted)
		{
			ReplyNotImplemented(NULL);
			return;
		}
		if (drive->freeBytes < 1000000)
		{
			drive = GetDriveInfo('X');
			if (drive != NULL && drive->mounted && drive->freeBytes < 1000000)
			{
				drive = GetDriveInfo('Y');
				if (drive != NULL && drive->mounted && drive->freeBytes < 1000000)
				{
					Reply(550, "Insufficient Drive Space.");
					return;
				}
			}
		}

		std::string fileName;
		StrFormat(fileName, "%c:\\content.txt", drive->letter);
		path.clear();
		std::string line;
		for (std::vector<SFtpFileEntry>::iterator it = m_files.begin(); it != m_files.end(); ++it)
		{
			if (it->bDirectory)
				StrFormat(line, "D,%s\r\n", it->strName.c_str());
			else
				StrFormat(line, "F,%s,%u,%u,%u\r\n", it->strName.c_str(), it->dwSize, it->dwAttributes,
				          it->dwCreationTime);
			StrAppend(path, line.c_str());
		}

		DeleteFileA(fileName.c_str());
		FILE *file = fopen(fileName.c_str(), "w+b");
		// Quirk: a failed write leaves the file open.
		if (file != NULL && fwrite(path.c_str(), path.size(), 1, file) != 0)
		{
			fclose(file);
			StrFormat(path, "content made and put to %s.", fileName.c_str());
			Reply(200, path.c_str());
			return;
		}
		ReplyNotImplemented(NULL);
		return;
	}

	// SITE TITLES <dir>
	if (StrLeft(arg, 7).compare("TITLES ") == 0)
	{
		std::string titlesArg = StrMid(arg, 7, arg.size() - 7);
		arg = StrTrimRight(StrTrimLeft(titlesArg));
		SiteTitles(arg);
		return;
	}

	// SITE TITLEIMAGE: whether the last SITE TITLES found a place for title
	// images.
	if (arg.compare("TITLEIMAGE") == 0)
	{
		if (m_strTitleImageDir.size() != 0)
			Reply(200, "OK");
		else
			Reply(550, "Creation of Title Images failed.");
		return;
	}

	// the rest is up to the dashboard
	std::string text;
	int code = m_pServer->NotifySiteCommand(arg.c_str(), &text);
	if (code == 0 || text.size() == 0)
	{
		ReplyNotImplemented(NULL);
		return;
	}
	StrTrimRight(StrTrimLeft(text));
	if (text.size() != 0)
		Reply(code, text.c_str());
	else
		Reply(200, "\"%s\" OK.", arg.c_str());
}

// Part of SITE TITLES: the XBE's title image as <title id>.bmp in
// m_strTitleImageDir, copied to a 16-bit linear texture first.
void CFTPConnection::SaveTitleImage(const char *folder, DWORD titleId)
{
	std::string path;
	IDirect3DTexture8 *icon = NULL;
	StrFormat(path, "%s\\default.xbe", folder);
	if (!LoadXbeIcon(path.c_str(), &icon) || icon == NULL)
		return;

	D3DSURFACE_DESC desc;
	Get2DSurfaceDesc((D3DResource *)icon, 0, &desc);
	IDirect3DTexture8 *copy = NULL;
	IDirect3DSurface8 *source = NULL;
	IDirect3DSurface8 *destination = NULL;
	if (SUCCEEDED(g_pd3dDevice->CreateTexture(desc.Width, desc.Height, 1, 0, D3DFMT_LIN_R5G6B5, 0, &copy)))
	{
		if (SUCCEEDED(icon->GetSurfaceLevel(0, &source)) && SUCCEEDED(copy->GetSurfaceLevel(0, &destination)) &&
		    D3DXLoadSurfaceFromSurface(destination, NULL, NULL, source, NULL, NULL, D3DX_FILTER_NONE, 0) >= 0)
		{
			StrFormat(path, "%s\\%08x.bmp", m_strTitleImageDir.c_str(), titleId);
			if (XGWriteSurfaceToFile(destination, path.c_str()) < 0)
				Sleep(5);
			destination->Release();
		}
		if (copy != NULL)
			copy->Release();
		if (source != NULL)
			source->Release();
	}
	icon->Release();
}

// 0x000B95F9
// SITE TITLES <dir>: one 150 reply per game folder in dir (a folder with a
// default.xbe and more than the XBE in it):
//   "<folder>\default.xbe<TAB><title><TAB><bytes><TAB><bytes on disk><TAB>
//    <title id><TAB><folders><TAB><files><TAB><region><TAB><ratings>"
// and the title images are saved as <title id>.bmp in <drive>:\titleimages
// and zipped to <drive>:\titleimage.zip, on the first of Z:, X:, Y: with
// 1,000,000 bytes free. Then 200 OK, or 550 if there were no games.
void CFTPConnection::SiteTitles(std::string arg)
{
	BOOL found = FALSE;
	std::string dir = arg;
	std::string pattern;
	if (arg.c_str() == NULL || arg.size() == 0)
		return;

	// Quirk: as in MAKE_FILE_TREE an unmounted drive is taken as is.
	SDriveInfo *drive = GetDriveInfo('Z');
	if (drive != NULL && drive->mounted && drive->freeBytes < 1000000)
	{
		drive = GetDriveInfo('X');
		if (drive != NULL && drive->mounted && drive->freeBytes < 1000000)
		{
			drive = GetDriveInfo('Y');
			if (drive != NULL && drive->mounted && drive->hasMedia && drive->freeBytes < 1000000)
				drive = NULL;
		}
	}

	m_strTitleImageDir.clear();
	if (drive != NULL)
	{
		StrFormat(m_strTitleImageDir, "%c:\\titleimages", tolower(drive->letter));
		RemoveDirectoryA(m_strTitleImageDir.c_str());
		CreateDirectoryTree(m_strTitleImageDir.c_str());
		pattern = m_strTitleImageDir;
		pattern.append("\\*.bmp");
		WIN32_FIND_DATAA old;
		HANDLE find = FindFirstFileA(pattern.c_str(), &old);
		if (find != INVALID_HANDLE_VALUE)
		{
			BOOL more;
			do
			{
				std::string file;
				StrFormat(file, "%s\\%s", m_strTitleImageDir.c_str(), old.cFileName);
				DeleteFileA(file.c_str());
				more = FindNextFileA(find, &old) == TRUE;
			} while (more);
			CloseHandle(find);
		}
	}

	// Quirk: a path without ':' is drive-converted but not resolved against
	// the current directory.
	if (StrFind(dir, ":") == -1)
	{
		ResolvePath(&dir, dir.c_str(), NULL);
		FtpPathToXbox(&dir, dir.c_str());
	}
	StrReplaceAll(dir, "\\\\?\\", "");
	if (StrRight(dir, 1) != "\\")
		dir.append("\\");
	StrFormat(pattern, "%s*", dir.c_str());

	WIN32_FIND_DATAA data;
	memset(&data, 0, sizeof(data));
	HANDLE find = FindFirstFileA(pattern.c_str(), &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;

			std::string xbePath;
			StrFormat(xbePath, "%s%s\\default.xbe", dir.c_str(), data.cFileName);
			SXbeHeaderInfo xbe;
			if (!FileExists(xbePath.c_str()) || !ReadXbeHeader(xbePath.c_str(), &xbe))
				continue;

			m_files.clear();
			StrFormat(xbePath, "%s%s", dir.c_str(), data.cFileName);
			BuildFileTree(xbePath);
			if (m_files.size() <= 1)
				continue;

			int files = 0;
			int folders = 0;
			DWORD bytes = 0;
			DWORD diskBytes = 0;
			for (std::vector<SFtpFileEntry>::iterator it = m_files.begin(); it != m_files.end(); ++it)
			{
				if (it->bDirectory)
				{
					folders++;
					continue;
				}
				files++;
				bytes += it->dwSize;
				diskBytes += (DWORD)(ceil((double)it->dwSize * (1.0 / 16384)) * 16384.0);
			}

			wchar_t name[41];
			for (int i = 0; i < 40; i++)
				name[i] = xbe.certificate.titleName[i];
			name[40] = 0;
			std::string title = StrFromWide(name);
			std::string text;
			StrFormat(text, "%s\\default.xbe\t%s\t%u\t%u\t%08x\t%d\t%d\t%08x\t%u", xbePath.c_str(), title.c_str(),
			          bytes, diskBytes, xbe.certificate.titleId, folders, files, xbe.certificate.gameRegion,
			          xbe.certificate.gameRatings);

			if (m_strTitleImageDir.size() != 0)
				SaveTitleImage(xbePath.c_str(), xbe.certificate.titleId);

			Reply(150, text.c_str());
			found = TRUE;
		} while (FindNextFileA(find, &data) == TRUE);
		CloseHandle(find);

		if (found)
		{
			if (m_strTitleImageDir.size() != 0)
			{
				std::string zipPath;
				StrFormat(zipPath, "%s\\titleimage.zip", StrLeft(m_strTitleImageDir, 2).c_str());
				DeleteFileA(zipPath.c_str());
				ZipFiles(m_strTitleImageDir.c_str(), zipPath.c_str(), TRUE);
				while (g_bFileOperationQueued)
					Sleep(100);
				if (!FileExists(zipPath.c_str()))
					m_strTitleImageDir.clear();
			}
			Reply(200, "OK");
			return;
		}
	}
	Reply(550, "Unable to find Xbox Titles");
}
