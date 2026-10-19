// CDirFTP: the FTP client's directory object. See FtpClient.h.
#include "FtpClient.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../system/FileOps.h"
#include "../thirdparty/FtpParse.h"
#include "Network.h"
#include "Socket.h"

CopyProgressRoutine g_pFtpCopyProgress = NULL;

// What GetFreeSpace() says when it can't ask: "plenty".
static const ULONGLONG kUnknownFreeSpace = 0x0050000000000000ULL;

// A time_t as a FILETIME. (FILETIME at esi, time in eax; 0x0008235F)
static void TimeToFileTime(time_t time, FILETIME *ft)
{
	ULONGLONG value = ((ULONGLONG)(LONGLONG)(long)time + 11644473600ULL) * 10000000ULL;
	ft->dwLowDateTime = (DWORD)value;
	ft->dwHighDateTime = (DWORD)(value >> 32);
}

// The progress figures while a file goes up or down: the percentage of the
// whole job, the time left and the rate. sent is this file's bytes so far.
// (inlined in each transfer loop)
static void UpdateTransferProgress(DWORD sent, DWORD *lastSecond)
{
	if (g_ullOperationBytes == 0)
		return;
	DWORD elapsed = (GetTickCount() - g_dwOperationUnknown640) / 1000;
	g_dwOperationElapsed = elapsed;
	g_dFilePercent = (double)(LONGLONG)(g_ullOperationDone * 100) / (double)(LONGLONG)g_ullOperationBytes;
	if (elapsed == 0 || elapsed == *lastSecond)
	{
		if (g_dOperationRate < 1.0)
			g_dOperationRate = (double)(int)sent;
	}
	else
	{
		*lastSecond = elapsed;
		ULONGLONG perSecond = g_ullOperationDone / elapsed;
		g_dwOperationRemaining = (DWORD)((g_ullOperationBytes - g_ullOperationDone) / (perSecond + 1)) + 1;
		// Quirk: this file's bytes over all the seconds since the first.
		g_dOperationRate = (double)(int)sent / (double)(elapsed << 10);
	}
}

// 0x000828A2 (this in esi, log in eax)
CDirFTP::CDirFTP(CFTPLog *log)
{
	m_pLog = log;
	m_pControl = NULL;
	m_nControlPort = 21;
	m_strPassword = "xbox";
	m_strUser = "xbox";
	m_strCurrentDir = "";
	m_dataSocket = INVALID_SOCKET;
	m_listenSocket = INVALID_SOCKET;
	m_bPassive = TRUE;
	m_bConnected = FALSE;
	m_bResume = FALSE;
	m_bBoost = FALSE;
	memset(m_pasvAddress, 0, sizeof(m_pasvAddress));
	memset(m_pasvPort, 0, sizeof(m_pasvPort));
	memset(m_portAddress, 0, sizeof(m_portAddress));
	memset(m_portPort, 0, sizeof(m_portPort));
	m_nFindState = -1;
	m_bBinary = TRUE;
	m_bBusy = FALSE;
	m_bAbort = FALSE;
	m_bUnleashXServer = FALSE;
	m_bUnixL8 = FALSE;
	m_nMinPort = 0x400;
	m_nMaxPort = 0xFFFF;
	m_nPort = 0x3FF;
	m_bReplyPending = FALSE;
	ResetStats();
}

// 0x0008241D
CDirFTP::~CDirFTP()
{
	Reset();
}

// 0x0001E55E (this in eax)
void CDirFTP::ResetStats()
{
	m_nFilesSent = 0;
	m_unknown4D0 = 0;
	m_unknown4B4[0] = 0;
	m_unknown4B4[1] = 0;
	m_dwStartTick = 0;
	m_dwEndTick = 0;
	m_dwTransferMs = 0;
	m_unknown4D8[0] = 0;
	m_ullBytesSent = 0;
	m_ullUnknown4E8 = 0;
}

// 0x00082381 (this in ebx)
void CDirFTP::Reset()
{
	if (m_pControl != NULL)
	{
		m_pControl->Close();
		delete m_pControl;
		m_pControl = NULL;
	}
	if (m_listenSocket != INVALID_SOCKET)
	{
		shutdown(m_listenSocket, SD_BOTH);
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
	}
	m_bConnected = FALSE;
	m_strListing.erase();
	m_bResume = FALSE;
	m_bBoost = FALSE;
	// Quirk: a data connection still open is forgotten, not closed.
	m_dataSocket = INVALID_SOCKET;
	m_listenSocket = INVALID_SOCKET;
	memset(m_pasvAddress, 0, sizeof(m_pasvAddress));
	memset(m_pasvPort, 0, sizeof(m_pasvPort));
	m_nFindState = -1;
	m_bBinary = FALSE;
	m_bBusy = FALSE;
	m_bAbort = FALSE;
	m_bReplyPending = FALSE;
}

// 0x000822EB (port in eax, this in ecx)
unsigned CDirFTP::SetMinPort(unsigned port)
{
	if (port == 0)
		port = 0x400;
	m_nMinPort = port % 0xFFFF;
	m_nPort = port % 0xFFFF - 1;
	return port / 0xFFFF;
}

// 0x0008230D (port in edx, this in eax)
void CDirFTP::SetMaxPort(unsigned port)
{
	if (port > 0xFFFF)
		port = 0xFFFF;
	m_nMaxPort = port;
	if (port == 0)
		m_nMaxPort = 0xFFFF;
}

// 0x000824AE (reply in ebx, this on the stack)
int CDirFTP::ParseReply(std::string &reply)
{
	StrTrimRight(StrTrimLeft(reply));
	int space = StrFind(reply, " ");
	if (space == -1)
		return 0;
	int code = atol(StrLeft(reply, space).c_str());
	if (code > 400)
	{
		Log(reply.c_str(), FTPLOG_ERROR);
		m_strError = reply;
	}
	else
	{
		Log(reply.c_str(), FTPLOG_REPLY);
	}
	reply = StrMid(reply, space, (int)reply.size() - space);
	return code;
}

// 0x0008259A (this in ecx)
const char *CDirFTP::ReadReply()
{
	memset(m_szReply, 0, sizeof(m_szReply));
	while (m_bBusy)
		Sleep(5);
	if (m_pControl == NULL)
	{
		if (m_bConnected)
		{
			std::string message;
			StrFormat(message, "Unable to receive reply from host. Command: %s", m_strLastCommand.c_str());
			Log(message.c_str(), FTPLOG_ERROR);
		}
		return NULL;
	}

	// One byte at a time up to the '\n'; 20 seconds of silence ends it.
	// Quirk: a closed connection (0 bytes) is waited on forever, and a full
	// buffer isn't terminated.
	int total = 0;
	m_bBusy = TRUE;
	char ch = 0;
	char *p = m_szReply;
	DWORD tick = GetTickCount();
	do
	{
		int count = m_pControl->Receive(&ch, 1);
		if (count < 1)
		{
			if (count == -1 && (GetTickCount() - tick > 20000 || WSAGetLastError() != WSAEWOULDBLOCK))
				break;
		}
		else
		{
			total += count;
			*p = ch;
			p += count;
			if (ch == '\n')
				break;
			tick = GetTickCount();
		}
	} while (total < 0x400);
	m_bBusy = FALSE;

	// "nnn text" ends the reply; "nnn-text", lines starting with a space
	// and other text are continuation lines. (The original copies the code
	// into a DWORD still holding the tick count, so its fourth byte isn't
	// always a NUL.)
	size_t length = strlen(m_szReply);
	if (m_szReply[0] != ' ' && (length == 0 || length > 2))
	{
		if (length < 4)
		{
			m_bReplyPending = FALSE;
			return m_szReply;
		}
		char code[4];
		strncpy(code, m_szReply, 3);
		code[3] = 0;
		if (length < 5 || (m_szReply[3] != '-' && atol(code) != 0))
		{
			m_bReplyPending = FALSE;
			return m_szReply;
		}
	}
	Log(m_szReply, FTPLOG_REPLY);
	return ReadReply();
}

// ReadReply() then ParseReply() (inlined everywhere).
int CDirFTP::ReadReplyCode(std::string &reply)
{
	const char *line = ReadReply();
	reply = line != NULL ? line : "";
	return ParseReply(reply);
}

// 0x000829D9 (data in ecx)
HRESULT CDirFTP::SendAll(SOCKET socket, const char *data, int size)
{
	for (;;)
	{
		int sent = send(socket, data, size, 0);
		if (sent > 0 && sent < size)
		{
			data += sent;
			size -= sent;
			continue;
		}
		if (sent == size)
			return S_OK;
		if (sent == -1)
		{
			DWORD error = WSAGetLastError();
			if (error != WSAEWOULDBLOCK)
			{
				if (error == WSAECONNRESET || error == WSAENOTCONN || error == WSAESHUTDOWN || error == WSAETIMEDOUT)
				{
					StrFormat(m_strError, "Disconnected from Host. Socket Error: %d", (int)error);
					m_bConnected = FALSE;
				}
				SetLastError(error);
				return E_FAIL;
			}
			if (g_bCancelOperation)
				return E_FAIL;
		}
	}
}

// 0x00082E17 (this in esi)
int CDirFTP::SendCommand(const char *command, BOOL closeData, BOOL log)
{
	if (m_bReplyPending)
	{
		// The last command's reply wasn't read: read it now.
		while (m_bBusy)
			Sleep(5);
		Log("Command in progress. Invalid Sequence", FTPLOG_ERROR);
		const char *line = ReadReply();
		std::string reply = line != NULL ? line : "";   // (NULL would crash the original)
		ParseReply(reply);
	}
	int result = -1;
	if (closeData)
		CloseData();
	if (command == NULL || strlen(command) == 0)
		return result;
	m_strLastCommand = command;
	while (m_bBusy)
		Sleep(5);
	std::string line;
	StrFormat(line, "%s\r\n", command);
	if (log)
		Log(command, FTPLOG_COMMAND);
	m_bBusy = TRUE;
	result = (int)strlen(command);
	if (FAILED(SendAll(m_pControl->m_socket, line.c_str(), (int)line.size())))
	{
		result = -1;
	}
	else
	{
		m_bReplyPending = TRUE;
		Sleep(5);
	}
	m_bBusy = FALSE;
	return result;
}

// 0x0008412E (binary in ecx, this in eax)
void CDirFTP::SetBinary(BOOL binary)
{
	if (m_bBinary == binary)
		return;
	m_bBinary = binary;
	SendCommand(binary ? "TYPE I" : "TYPE A", TRUE, TRUE);
	std::string reply;
	ReadReplyCode(reply);
}

// 0x00083830
BOOL CDirFTP::Connect(const char *host, int port, const char *user, const char *password)
{
	BOOL loggedIn = FALSE;
	Reset();
	CSocketAddress address;
	address.Set(host, port);
	if (!address.m_bValid)
	{
		Reset();
		return loggedIn;
	}
	m_pControl = new CSocket(true, IPPROTO_TCP);
	if (m_pControl == NULL)
		return FALSE;

	std::string message;
	StrFormat(message, "Connecting to %s...", host);
	Log(" ", FTPLOG_STATUS);
	Log(message.c_str(), FTPLOG_STATUS);
	m_pControl->SetTimeouts(30000, 60000);
	if (!m_pControl->Connect(address))
	{
		StrFormat(m_strError, "Socket Error %d", (int)WSAGetLastError());
	}
	else
	{
		if (g_bCancelOperation)
		{
			Reset();
			return loggedIn;
		}
		m_bBinary = TRUE;
		m_bUnixL8 = FALSE;
		m_pControl->SetTimeouts(15000, 20000);
		m_pControl->SetBlocking(FALSE);
		StrFormat(message, "Connected to %s Port %d...", host, port);
		Log(message.c_str(), FTPLOG_COMMAND);

		std::string reply, command;
		if (ReadReplyCode(reply) == 220 && !g_bCancelOperation)
		{
			BOOL anonymous = FALSE;
			if (user == NULL || strlen(user) == 0)
			{
				anonymous = TRUE;
				command = "USER Anonymous\r\n";
			}
			else
			{
				StrFormat(command, "USER %s\r\n", user);
			}
			Log(command.c_str(), FTPLOG_COMMAND);
			DWORD error;
			if (FAILED(SendAll(m_pControl->m_socket, command.c_str(), (int)command.size())))
			{
				error = WSAGetLastError();
				StrFormat(m_strError, "Socket Error %d", (int)error);
			}
			else if (ReadReplyCode(reply) == 331)
			{
				if (anonymous || password == NULL || strlen(password) == 0)
					command = "PASS UnleashX@TheBestDash.com\r\n";
				else
					StrFormat(command, "PASS %s\r\n", password);
				Log("PASS (Hidden)", FTPLOG_COMMAND);
				if (FAILED(SendAll(m_pControl->m_socket, command.c_str(), (int)command.size())))
				{
					error = WSAGetLastError();
					StrFormat(m_strError, "Socket Error %d", (int)error);
				}
				else
				{
					// The 230 may come after a "230-" banner; two empty
					// reads give up.
					reply.erase();
					BOOL banner = FALSE;
					memset(m_szReply, 0, sizeof(m_szReply));
					int emptyLeft = 2;
					do
					{
						const char *line = ReadReply();
						reply = line != NULL ? line : "";
						if ((int)reply.size() >= 5 && StrLeft(reply, 4).compare("230-") == 0)
						{
							banner = TRUE;
							Log(reply.c_str(), FTPLOG_REPLY);
						}
						else if (reply.size() == 0)
						{
							emptyLeft--;
						}
						else
						{
							if (StrLeft(reply, 4).compare("230 ") == 0)
								break;
							Log(reply.c_str(), FTPLOG_REPLY);
							if (!banner)
								break;
						}
					} while (emptyLeft != 0);

					if (ParseReply(reply) == 230)
					{
						loggedIn = TRUE;
						m_bConnected = TRUE;
						command = "SYST";
						SendCommand(command.c_str(), TRUE, TRUE);
						if (ReadReplyCode(reply) != 0)
						{
							m_strSystem = StrTrimRight(StrTrimLeft(reply));
							m_bUnleashXServer = StrFind(m_strSystem, "UnleashX") != -1 ||
							                    StrFind(m_strSystem, "/ UX ") != -1;
							m_bUnixL8 = StrCmpNoCase(m_strSystem, "UNIX Type: L8") == 0;
						}
						if (SendCommand("REST 100", TRUE, TRUE) != -1 && ReadReplyCode(reply) == 350)
						{
							SendCommand("REST 0", TRUE, TRUE);
							ReadReplyCode(reply);
							m_bResume = TRUE;
						}
					}
				}
			}
		}
		else
		{
			m_pControl->Close();
		}
	}
	if (loggedIn)
	{
		m_strCurrentDir = "/";
		return loggedIn;
	}
	Reset();
	return loggedIn;
}

// 0x00083055 (this in eax)
void CDirFTP::Disconnect()
{
	if (m_bConnected && m_pControl != NULL)
	{
		m_bReplyPending = FALSE;
		SendCommand("QUIT", TRUE, TRUE);
	}
	Reset();
}

// 0x000833EC
BOOL CDirFTP::ChangeDir(const char *path)
{
	std::string reply, command;
	if (path == NULL || strlen(path) == 0)
	{
		if (m_strCurrentDir.size() == 0)
			command = "CWD /";
		else
			StrFormat(command, "CWD %s", m_strCurrentDir.c_str());
	}
	else if (_mbsicmp(path, "CDUP") == 0)
	{
		if (StrReverseFind(m_strCurrentDir, "/") < 1)
		{
			command = "CWD /";
		}
		else
		{
			command = "CDUP";
			// (Not logged in: the current directory again.)
			if (!m_bConnected)
				StrFormat(command, "CWD %s", m_strCurrentDir.c_str());
		}
	}
	else
	{
		// "/C:..." loses the slash, "C:/" its trailing one.
		std::string dir = path;
		if (StrLeft(dir, 1).compare("/") == 0 && StrFind(dir, ":") == 2)
			dir = StrMid(dir, 1, (int)dir.size() - 1);
		if (dir.size() == 3 && StrRight(dir, 1).compare("/") == 0)
			dir = StrLeft(dir, 2);
		if (strchr(path, '/') == NULL)
		{
			// Relative to the current directory.
			if (m_strCurrentDir.size() == 0)
				StrFormat(command, "CWD /%s", dir.c_str());
			else
				StrFormat(command, "CWD %s%s", m_strCurrentDir.c_str(), dir.c_str());
		}
		else
		{
			StrFormat(command, "CWD %s", dir.c_str());
		}
	}

	m_strCurrentDir.erase();
	StrReplaceAll(command, "//", "/");
	if (SendCommand(command.c_str(), TRUE, TRUE) == -1)
		return FALSE;
	int code = ReadReplyCode(reply);
	if (code == 250 || code == 200 || code == 226)
	{
		if (SendCommand("PWD", TRUE, TRUE) == -1)
			return FALSE;
	}
	else if (code == 550)
	{
		// Quirk: a refused CWD still asks PWD and succeeds with the
		// directory it stayed in.
		if (SendCommand("PWD", TRUE, TRUE) == -1)
			return FALSE;
		Sleep(100);
	}
	else
	{
		return FALSE;
	}

	if (ReadReplyCode(reply) != 257)
	{
		m_strCurrentDir.erase();
		return FALSE;
	}
	// 257 "/C/Apps/" is the current directory.
	StrTrimRight(StrTrimLeft(reply));
	int quote = StrReverseFind(reply, "\"");
	if (quote == -1)
		return FALSE;
	m_strCurrentDir = StrLeft(reply, quote);
	StrRemoveChar(m_strCurrentDir, '"');
	StrReplaceAll(m_strCurrentDir, "//", "/");
	if (m_strCurrentDir.size() == 0 || StrRight(m_strCurrentDir, 1).compare("/") != 0)
		m_strCurrentDir.append("/");
	m_strListing.erase();
	return TRUE;
}

// 0x00084D64 (this in edi)
BOOL CDirFTP::CheckDir(const char *path)
{
	std::string previous = m_strCurrentDir;
	if (m_strCurrentDir.compare(path) == 0)
		return TRUE;
	BOOL result = ChangeDir(path);
	if (previous.size() != 0)
		ChangeDir(previous.c_str());
	return result;
}

// 0x000821BA (this in esi)
BOOL CDirFTP::Listen()
{
	if (m_bPassive)
		return FALSE;
	if (m_listenSocket == INVALID_SOCKET || m_listenSocket == 0)
		m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_listenSocket == INVALID_SOCKET)
		return FALSE;
	sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons((WORD)m_nPort);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	char reuse = 1;
	setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, 1);
	if (bind(m_listenSocket, (sockaddr *)&address, sizeof(address)) != 0)
		return FALSE;
	return listen(m_listenSocket, 0x7FFFFFFF) == 0;
}

// 0x00082103 (this in esi)
BOOL CDirFTP::AcceptData()
{
	if (m_dataSocket != INVALID_SOCKET)
		return TRUE;
	if (m_listenSocket == INVALID_SOCKET || m_bPassive)
		return FALSE;
	DWORD start = GetTickCount();
	do
	{
		socklen_t size;
		m_dataSocket = accept(m_listenSocket, NULL, &size);
		if (m_dataSocket != INVALID_SOCKET)
			break;
		Sleep(5);
	} while (GetTickCount() <= start + 15000);
	BOOL accepted = m_dataSocket != INVALID_SOCKET;
	if (accepted)
	{
		unsigned long blocking = 0;
		ioctlsocket(m_dataSocket, FIONBIO, &blocking);
	}
	shutdown(m_listenSocket, SD_BOTH);
	closesocket(m_listenSocket);
	m_listenSocket = INVALID_SOCKET;
	return accepted;
}

// 0x000321F9 (this in eax)
void CDirFTP::CloseData()
{
	if (m_dataSocket != INVALID_SOCKET)
	{
		shutdown(m_dataSocket, SD_BOTH);
		closesocket(m_dataSocket);
		m_dataSocket = INVALID_SOCKET;
	}
}

// 0x000841DC (this in ecx)
BOOL CDirFTP::OpenData()
{
	m_bAbort = FALSE;
	if (m_dataSocket != INVALID_SOCKET)
		return TRUE;
	std::string reply;
	BOOL result = FALSE;

	if (!m_bPassive)
	{
		// PORT a,b,c,d,hi,lo on the next port of the range.
		std::string command = GetIPAddressString();
		StrReplaceAll(command, ".", ",");
		m_nPort++;
		if (m_nPort > m_nMaxPort)
			m_nPort = m_nMinPort;
		unsigned high = m_nPort >> 8;
		unsigned low = m_nPort & 0xFF;
		if (m_portAddress[0] == 0 && m_portAddress[1] == 0 && m_portAddress[2] == 0 && m_portAddress[3] == 0)
		{
			std::string ip = command;
			StrFormat(command, "%s,%d,%d", ip.c_str(), high, low);
			m_portPort[0] = (BYTE)high;
			m_portPort[1] = (BYTE)low;
			unsigned a = 0, b = 0, c = 0, d = 0;
			sscanf(command.c_str(), "%u,%u,%u,%u,%u,%u", &a, &b, &c, &d, &high, &low);
			if (a != 0 || b != 0 || c != 0 || d != 0)
			{
				m_portAddress[0] = (BYTE)a;
				m_portAddress[1] = (BYTE)b;
				m_portAddress[2] = (BYTE)c;
				m_portAddress[3] = (BYTE)d;
			}
			std::string arguments = command;
			StrFormat(command, "PORT %s", arguments.c_str());
		}
		else
		{
			std::string ip = command;
			StrFormat(command, "PORT %s,%d,%d", ip.c_str(), high, low);
		}
		if (Listen())
		{
			std::string line = command;
			if (SendCommand(StrTrimRight(StrTrimLeft(line)).c_str(), TRUE, TRUE) != -1 && ReadReplyCode(reply) == 200)
				result = TRUE;
		}
		return result;
	}

	// PASV: "227 Entering Passive Mode (a,b,c,d,hi,lo)."
	if (SendCommand("PASV", TRUE, TRUE) == -1)
		return result;
	Sleep(5);
	if (ReadReplyCode(reply) != 227)
	{
		// Fall back to active mode for good.
		m_bPassive = FALSE;
		return OpenData();
	}
	StrTrimRight(StrTrimLeft(reply));
	int space = StrReverseFind(reply, " ");
	if (space == -1)
		return result;
	reply = StrMid(reply, space + 1, (int)reply.size() - (space + 1));
	reply = StrLeft(reply, (int)reply.size() - 1);
	StrRemoveChar(reply, '(');
	StrRemoveChar(reply, ')');
	unsigned a = 0, b = 0, c = 0, d = 0, high = 0, low = 0;
	sscanf(reply.c_str(), "%u,%u,%u,%u,%u,%u", &a, &b, &c, &d, &high, &low);
	if (a != 0 || b != 0 || c != 0 || d != 0)
	{
		m_pasvAddress[0] = (BYTE)a;
		m_pasvAddress[1] = (BYTE)b;
		m_pasvAddress[2] = (BYTE)c;
		m_pasvAddress[3] = (BYTE)d;
	}
	m_pasvPort[0] = (BYTE)high;
	m_pasvPort[1] = (BYTE)low;
	WORD port = (WORD)(high * 0x100 + low);
	if (port == 0)
	{
		StrFormat(reply, "Can not connect to %u.%u.%u.%u on Port %u", m_pasvAddress[0], m_pasvAddress[1],
		          m_pasvAddress[2], m_pasvAddress[3], (unsigned)port);
		Log(reply.c_str(), FTPLOG_ERROR);
		return result;
	}
	m_dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	std::string ip;
	StrFormat(ip, "%d.%d.%d.%d", m_pasvAddress[0], m_pasvAddress[1], m_pasvAddress[2], m_pasvAddress[3]);
	sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(ip.c_str());
	address.sin_port = htons(port);
	Sleep(5);
	// Quirk: a failed connect leaves m_dataSocket set, so the next OpenData()
	// thinks it's open (SendCommand closes it first, though).
	if (connect(m_dataSocket, (sockaddr *)&address, sizeof(address)) == 0)
	{
		result = TRUE;
		int timeout = 15000;
		setsockopt(m_dataSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, 4);
		timeout = 20000;
		setsockopt(m_dataSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, 4);
	}
	return result;
}

// 0x000846B7 (path in eax, this in ecx)
ULONGLONG CDirFTP::GetFreeSpace(const char *path)
{
	ULONGLONG freeSpace = kUnknownFreeSpace;
	if (!m_bUnleashXServer || path[0] != '/')
		return freeSpace;
	std::string reply;
	StrFormat(reply, "SITE FREESPACE %c", path[1]);
	if (SendCommand(reply.c_str(), TRUE, TRUE) != -1 && ReadReplyCode(reply) == 220)
		freeSpace = _atoi64(reply.c_str());
	return freeSpace;
}

// 0x00084777 (this in eax)
std::string CDirFTP::List()
{
	SetBinary(FALSE);
	if (OpenData() && SendCommand("LIST", FALSE, TRUE) != -1)
	{
		std::string reply;
		int code = ReadReplyCode(reply);
		if (code == 150 || code == 125)
		{
			std::string listing;
			listing.erase();
			if (m_bPassive || AcceptData())
			{
				char buffer[0x400];
				memset(buffer, 0, sizeof(buffer));
				while (recv(m_dataSocket, buffer, 0x3FF, 0) > 0)
				{
					listing.append(buffer);
					memset(buffer, 0, sizeof(buffer));
				}
				Sleep(100);
			}
			CloseData();
			// (ParseReply() trimmed it, so this always reads the 226.)
			if (StrFind(reply, "\r\n") == -1)
			{
				ReadReplyCode(reply);
				if (!m_bConnected && listing.size() != 0)
					m_bConnected = TRUE;
			}
			return listing;
		}
	}
	return std::string("");
}

// The next listing line ftpparse() takes, without "." and "..", into data.
// (inlined in FindFirst and FindNext)
BOOL CDirFTP::ParseNextEntry(WIN32_FIND_DATAA *data)
{
	int end = StrFind(m_strListLeft, "\n");
	while (end != -1)
	{
		std::string line = StrLeft(m_strListLeft, end);
		m_strListLeft = StrMid(m_strListLeft, end + 1, (int)m_strListLeft.size() - (end + 1));
		struct ftpparse entry;
		if (ftpparse(&entry, (char *)line.c_str(), (int)line.size()) != 0 &&
		    (entry.flagtrycwd != 0 || entry.flagtryretr != 0) &&
		    memcmp(entry.name, "..", 3) != 0 && memcmp(entry.name, ".", 2) != 0)
		{
			// (The name runs to the end of the line, so it's terminated.)
			std::string name = entry.name;
			StrRemoveChar(name, '\x01');
			StrTrimRight(StrTrimLeft(name));
			// Quirk: no limit on the length.
			strncpy(data->cFileName, name.c_str(), name.size());
			data->nFileSizeLow = (DWORD)entry.size;
			data->dwFileAttributes = entry.flagtrycwd != 0 ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
			TimeToFileTime(entry.mtime, &data->ftLastWriteTime);
			return TRUE;
		}
		end = StrFind(m_strListLeft, "\n");
	}
	return FALSE;
}

// 0x00084DC4
HANDLE CDirFTP::FindFirst(const char *pattern, WIN32_FIND_DATAA *data)
{
	memset(data, 0, sizeof(*data));
	if (!m_bConnected)
	{
		m_strCurrentDir.erase();
		return INVALID_HANDLE_VALUE;
	}
	// The pattern only matters for reusing the listing: the current
	// directory is what's listed.
	if (pattern == NULL || strlen(pattern) == 0 || m_strCurrentDir.compare(pattern) != 0 ||
	    m_strListing.size() == 0)
	{
		m_strListing = List();
		if (m_strListing.size() != 0)
		{
			// Every line ends in "\n" (lone '\n's become '\r', see the
			// order of the replacements).
			StrReplaceAll(m_strListing, "\n", "\r");
			StrReplaceAll(m_strListing, "\r\r", "\n");
			if (m_strListing.size() != 0 && StrRight(m_strListing, 1).compare("\n") != 0)
				m_strListing.append("\n");
			StrRemoveChar(m_strListing, (char)0xCC);
			StrRemoveChar(m_strListing, (char)0xB2);
		}
	}
	m_strListLeft = m_strListing;
	if (!ParseNextEntry(data))
	{
		m_nFindState = -1;
		return INVALID_HANDLE_VALUE;
	}
	m_nFindState = 1;
	return (HANDLE)1;
}

// 0x00082731
BOOL CDirFTP::FindNext(HANDLE /*find*/, WIN32_FIND_DATAA *data)
{
	memset(data, 0, sizeof(*data));
	if (m_nFindState == -1)
		return FALSE;
	return ParseNextEntry(data);
}

// 0x0003221D
BOOL CDirFTP::FindClose(HANDLE /*find*/)
{
	m_nFindState = -1;
	return TRUE;
}

// 0x00082F62
BOOL CDirFTP::MoveFile(const char *source, const char *destination)
{
	BOOL result = FALSE;
	std::string reply, command;
	StrFormat(command, "RNFR %s", source);
	if (SendCommand(command.c_str(), TRUE, TRUE) != -1 && ReadReplyCode(reply) == 350)
	{
		StrFormat(command, "RNTO %s", destination);
		if (SendCommand(command.c_str(), TRUE, TRUE) != -1)
			result = ReadReplyCode(reply) == 250;
	}
	return result;
}

// 0x00083083
BOOL CDirFTP::RemoteFileExists(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	std::string reply, command = path;
	if (StrLeft(command, 1).compare("/") == 0)
		StrFormat(command, "SIZE %s", path);
	else
		StrFormat(command, "SIZE %s%s", m_strCurrentDir.c_str(), path);
	return SendCommand(command.c_str(), TRUE, TRUE) != -1 && ReadReplyCode(reply) == 213;
}

// 0x0008319E
BOOL CDirFTP::DeleteFile(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	std::string reply, command = path;
	if (StrLeft(command, 1).compare("/") == 0)
		StrFormat(command, "DELE %s", path);
	else
		StrFormat(command, "DELE %s%s", m_strCurrentDir.c_str(), path);
	return SendCommand(command.c_str(), TRUE, TRUE) != -1 && ReadReplyCode(reply) == 250;
}

// 0x000832BC
BOOL CDirFTP::CreateDirectory(const char *path, void * /*security*/)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	std::string reply, command = path;
	if (StrLeft(command, 1).compare("/") == 0)
		StrFormat(command, "MKD %s", path);
	else
		StrFormat(command, "MKD %s%s", m_strCurrentDir.c_str(), path);
	if (SendCommand(command.c_str(), TRUE, TRUE) == -1)
		return FALSE;
	int code = ReadReplyCode(reply);
	return code == 250 || code == 257;
}

// 0x0008495D
BOOL CDirFTP::DeleteTree(const char *path, std::vector<std::string> *folders, int *files)
{
	BOOL result = TRUE;
	if (!ChangeDir(path))
		return result;
	// Each folder goes in front of its parent, so RMD runs deepest first.
	folders->insert(folders->begin(), m_strCurrentDir);
	std::vector<std::string> subfolders;
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirst(path, &data);
	std::string name;
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (g_bCancelOperation)
				break;
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				StrFormat(name, "%s%s", m_strCurrentDir.c_str(), data.cFileName);
				memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
				swprintf(g_szOperationFile, 0x12F, L"%hs", name.c_str());
				result = DeleteFile(name.c_str());
				if (!result)
					break;
				(*files)++;
			}
			else
			{
				StrFormat(name, "%s%s", m_strCurrentDir.c_str(), data.cFileName);
				subfolders.push_back(std::string(name.c_str()));
			}
		} while (FindNext(find, &data));
	}
	FindClose(find);
	if (result && subfolders.size() != 0)
	{
		for (std::vector<std::string>::iterator it = subfolders.begin(); it != subfolders.end(); ++it)
		{
			if (g_bCancelOperation)
			{
				Log("Upload Operation Canceled.", FTPLOG_STATUS);   // (sic)
				result = FALSE;
				break;
			}
			result = DeleteTree(it->c_str(), folders, files);
			if (!result)
				break;
		}
	}
	return result;
}

// 0x00084B9A
BOOL CDirFTP::RemoveDirectory(const char *path)
{
	if (!StrIsNotEmpty(path))
		return FALSE;
	int files = 0;
	std::vector<std::string> folders;
	DWORD start = GetTickCount();
	std::string previous = m_strCurrentDir;
	BOOL result = DeleteTree(path, &folders, &files);
	if (!result)
		return result;
	std::string reply;
	int folderCount = (int)folders.size();
	if (folderCount != 0)
	{
		// Out of the tree, then the folders.
		if (ChangeDir(previous.c_str()))
		{
			for (std::vector<std::string>::iterator it = folders.begin(); it != folders.end(); ++it)
			{
				StrFormat(reply, "RMD %s", it->c_str());
				if (SendCommand(reply.c_str(), TRUE, TRUE) != -1 && ReadReplyCode(reply) != 250)
				{
					result = FALSE;
					break;
				}
			}
		}
	}
	if (result)
	{
		DWORD seconds = (GetTickCount() - start) / 1000;
		std::string message;
		StrFormat(message, "Deleted %d Folders and %d Files in %02d:%02d.", folderCount, files, (int)(seconds / 60),
		          (int)(seconds % 60));
		Log(message.c_str(), FTPLOG_COMMAND);
	}
	return result;
}

// 0x00085045 (this in eax)
BOOL CDirFTP::UploadFile(const char *local, const char *remote)
{
	memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
	memset(g_szOperationTarget, 0, sizeof(g_szOperationTarget));
	swprintf(g_szOperationFile, 0x12F, L"%hs", local);
	swprintf(g_szOperationTarget, 0x12F, L"%hs", remote);
	std::string reply, command;
	BOOL result = FALSE;
	if (!OpenData())
		return result;
	IFile *file = CreateFileObject(local);
	if (file == NULL)
	{
		// Quirk: waits for a reply to a STOR that was never sent.
		CloseData();
		ReadReplyCode(reply);
		return result;
	}
	if (file->OpenRead(local, 0, TRUE))
	{
		StrFormat(command, "STOR %s", remote);
		if (SendCommand(command.c_str(), FALSE, TRUE) != -1)
		{
			int code = ReadReplyCode(reply);
			if (code == 150 || code == 125)
			{
				if (!m_bPassive && !AcceptData())
				{
					delete file;
					return result;
				}
				if (g_ullOperationBytes == 0)
					g_ullOperationBytes = file->GetSize();
				DWORD sent = 0;
				g_dwOperationUnknown640 = GetTickCount();
				m_dwStartTick = g_dwOperationUnknown640;
				g_dwOperationElapsed = 0;
				g_dwOperationRemaining = 0;
				DWORD lastSecond = 0;
				BOOL ok = TRUE;
				if (!file->IsEOF())
				{
					// Read ahead one block while the last one is sent.
					ok = SUCCEEDED(file->ReadNext(0));
					if (!ok)
						ok = file->IsEOF();
					for (;;)
					{
						if (m_bAbort || !ok || g_bCancelOperation)
							break;
						DWORD size;
						if (!file->WaitRead(TRUE, &size) || size == 0)
						{
							ok = file->IsEOF();
							break;
						}
						BYTE *data = file->GetData();
						ok = SUCCEEDED(file->ReadNext(0));
						if (data == NULL || FAILED(SendAll(m_dataSocket, (const char *)data, size)))
						{
							ok = FALSE;
							break;
						}
						sent += size;
						g_ullOperationDone += size;
						UpdateTransferProgress(sent, &lastSecond);
						if (file->IsEOF())
						{
							ok = TRUE;
							break;
						}
					}
				}
				m_dwEndTick = GetTickCount();
				m_dwTransferMs += m_dwEndTick - m_dwStartTick;
				CloseData();
				code = ReadReplyCode(reply);
				result = ok && (code == 226 || code == 250);
				if (ok)
				{
					m_ullBytesSent += (LONGLONG)(int)sent;
					m_nFilesSent++;
					if (m_dwTransferMs == 0)
					{
						if (g_dOperationRate < 1.0)
							g_dOperationRate = (double)(LONGLONG)m_ullBytesSent * (1.0 / 1024.0);
					}
					else
					{
						g_dOperationRate = (double)(LONGLONG)m_ullBytesSent / (double)((m_dwTransferMs / 1000) << 10);
					}
				}
			}
			else if (m_dataSocket != INVALID_SOCKET)
			{
				CloseData();
				ReadReplyCode(reply);
			}
		}
		file->Close();
	}
	delete file;
	return result;
}

// 0x00085539 (this in eax)
BOOL CDirFTP::DownloadFile(const char *remote, const char *local, BOOL failIfExists)
{
	memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
	memset(g_szOperationTarget, 0, sizeof(g_szOperationTarget));
	swprintf(g_szOperationFile, 0x12F, L"%hs", remote);
	swprintf(g_szOperationTarget, 0x12F, L"%hs", local);
	std::string reply, command;
	BOOL result = FALSE;
	BOOL haveSize = FALSE;
	g_ullOperationBytes = 0;
	g_ullOperationDone = 0;
	StrFormat(command, "SIZE %s", remote);
	if (SendCommand(command.c_str(), FALSE, TRUE) != -1 && ReadReplyCode(reply) == 213)
	{
		g_ullOperationBytes = (LONGLONG)atol(reply.c_str());
		haveSize = TRUE;
	}

	// A name FATX can't hold is changed (and said so).
	std::string target = local;
	std::string message;
	MakeFatxPath(local, target);
	if (target.compare(local) != 0)
	{
		StrFormat(message, "Invalid for FATX: %s", local);
		Log(message.c_str(), FTPLOG_ERROR);
		StrFormat(message, "Renamed to %s", target.c_str());
		Log(message.c_str(), FTPLOG_ERROR);
	}
	DWORD disposition = failIfExists ? CREATE_NEW : CREATE_ALWAYS;

	if (haveSize && g_ullOperationBytes == 0)
	{
		// An empty file is just created.
		CreateDirectoryTree(GetPathDirectory(target.c_str()).c_str());
		HANDLE handle = CreateFileA(target.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, disposition,
		                            FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (handle == INVALID_HANDLE_VALUE)
			return FALSE;
		// Quirk: "closed" with FindClose(), which leaves the handle open.
		FindClose(handle);
		return TRUE;
	}

	if (!OpenData())
		return result;
	IFile *file = CreateFileObject(target.c_str());
	if (file == NULL || !file->OpenWrite(target.c_str(), 0x100000, 0, 0, disposition))
	{
		CloseData();
		ReadReplyCode(reply);
		StrFormat(message, "Failed to retrieve %s.", local);
		Log(message.c_str(), FTPLOG_ERROR);
		if (file != NULL)
			delete file;
		return result;
	}

	// Two buffers: one fills from the socket while the writer thread
	// writes the other.
	DWORD bufferSize;
	BYTE *buffers[2];
	buffers[0] = file->GetBuffer(TRUE, &bufferSize);
	buffers[1] = file->GetBuffer(FALSE, &bufferSize);
	StrFormat(command, "RETR %s", remote);
	if (SendCommand(command.c_str(), FALSE, TRUE) != -1)
	{
		int code = ReadReplyCode(reply);
		if (code != 150 && code != 125)
		{
			if (m_dataSocket != INVALID_SOCKET)
			{
				CloseData();
				ReadReplyCode(reply);
			}
		}
		else if (!m_bPassive && !AcceptData())
		{
			// (the file isn't closed)
			delete file;
			return result;
		}
		else
		{
			g_dwOperationUnknown640 = GetTickCount();
			m_dwStartTick = g_dwOperationUnknown640;
			g_dwOperationElapsed = 0;
			g_dwOperationRemaining = 0;
			DWORD lastSecond = 0;
			int current = 0;
			DWORD filled = 0;
			DWORD received = 0;
			int count = recv(m_dataSocket, (char *)buffers[current], bufferSize, 0);
			// Quirk: only the first recv() and the writes decide this; a
			// later receive error ends the file as if it were complete
			// (the server's reply decides then).
			BOOL ok = count >= 0;
			for (;;)
			{
				if (count < 1 || m_bAbort || g_bCancelOperation)
				{
					if (ok && filled != 0 && FAILED(file->Write(buffers[current], filled)))
						ok = FALSE;
					break;
				}
				received += count;
				filled += count;
				g_ullOperationDone += count;
				if (filled != bufferSize)
				{
					count = recv(m_dataSocket, (char *)buffers[current] + filled, bufferSize - filled, 0);
					continue;
				}
				// A full buffer goes to the writer.
				if (FAILED(file->Write(buffers[current], filled)))
				{
					ok = FALSE;
					break;
				}
				UpdateTransferProgress(received, &lastSecond);
				current ^= 1;
				filled = 0;
				ok = TRUE;
				count = recv(m_dataSocket, (char *)buffers[current], bufferSize, 0);
			}
			m_dwEndTick = GetTickCount();
			if (ok)
			{
				m_ullBytesSent += (LONGLONG)(int)received;
				m_dwTransferMs += m_dwEndTick - m_dwStartTick;
				m_nFilesSent++;
				if (m_dwTransferMs == 0)
				{
					if (g_dOperationRate < 1.0)
						g_dOperationRate = (double)(LONGLONG)m_ullBytesSent * (1.0 / 1024.0);
				}
				else
				{
					g_dOperationRate = (double)(LONGLONG)m_ullBytesSent / (double)((m_dwTransferMs / 1000) << 10);
				}
			}
			CloseData();
			code = ReadReplyCode(reply);
			result = ok && (code == 226 || code == 250);
		}
	}
	file->Close();
	delete file;
	return result;
}

// 0x000860FC
BOOL CDirFTP::DownloadFolder(const char *remote, const char *local, BOOL failIfExists)
{
	BOOL result = TRUE;
	if (!ChangeDir(remote))
		return FALSE;
	std::string target, name;
	std::vector<std::string> subfolders;
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirst(remote, &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (g_bCancelOperation)
				break;
			StrFormat(name, "%s%s", m_strCurrentDir.c_str(), data.cFileName);
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				StrFormat(target, "%s\\%s", local, data.cFileName);
				StrReplaceAll(target, "\\\\", "\\");
				SetBinary(TRUE);
				result = DownloadFile(name.c_str(), target.c_str(), failIfExists);
				CloseData();
				if (!result)
					break;
			}
			else
			{
				subfolders.push_back(std::string(name.c_str()));
			}
		} while (FindNext(find, &data));
	}
	FindClose(find);
	if (result && !g_bCancelOperation && subfolders.size() != 0)
	{
		for (std::vector<std::string>::iterator it = subfolders.begin(); it != subfolders.end(); ++it)
		{
			name = *it;
			int slash = StrReverseFind(name, "/");
			if (slash != -1)
				name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
			StrFormat(target, "%s\\%s\\", local, name.c_str());
			StrReplaceAll(target, "\\\\", "\\");
			result = DownloadFolder(it->c_str(), target.c_str(), failIfExists);
			if (!result || g_bCancelOperation)
				break;
		}
	}
	return result;
}

// A local path under source as the FTP path under destination: "C:\a\b"
// with "C:\a" -> "/E/x" becomes "/E/x/b". (inlined)
static std::string MapUploadPath(const std::string &path, const std::string &source, const std::string &destination)
{
	std::string remote = path;
	StrReplaceAll(remote, source.c_str(), destination.c_str());
	StrReplaceAll(remote, "\\", "/");
	StrReplaceAll(remote, "//", "/");
	return remote;
}

// 0x00085C65
BOOL CDirFTP::UploadFolder(const char *local, const char *remote, BOOL /*failIfExists*/)
{
	BOOL result = TRUE;
	while (g_bFileOperationActive)
		Sleep(50);
	std::vector<SFileListItem> list;
	std::string source = local;
	std::string destination = remote;

	// The folder and everything under it, and the room it needs.
	g_bFileOperationActive = TRUE;
	list.push_back(SFileListItem(std::string(local), TRUE, 0));
	CollectFiles(local, &list);
	ULONGLONG freeSpace = GetFreeSpace(remote);
	if (g_ullOperationBytes != 0 && freeSpace != 0 && freeSpace < g_ullOperationBytes)
	{
		m_strError = "Not enough free space on target host.";
		Log(m_strError.c_str(), FTPLOG_ERROR);
		g_bFileOperationActive = FALSE;
		return FALSE;
	}
	ULONGLONG total = g_ullOperationBytes;
	g_bFileOperationActive = FALSE;
	BOOL parentCreated = FALSE;   // once a folder is new, everything after it is created without looking
	if (list.size() == 0)
		return FALSE;
	g_ullOperationDone = 0;

	std::string localFile, remotePath;
	for (std::vector<SFileListItem>::iterator it = list.begin(); it != list.end(); ++it)
	{
		if (g_bCancelOperation)
		{
			Log("Upload Operation Canceled.", FTPLOG_STATUS);
			break;
		}
		std::string path = it->path;
		if (!it->isFolder)
		{
			localFile = path;
			remotePath = MapUploadPath(path, source, destination);
			g_ullOperationBytes = total;
			SetBinary(TRUE);
			result = UploadFile(localFile.c_str(), remotePath.c_str());
			CloseData();
			if (!result)
			{
				// Logged, and the next file is tried. (The result is that
				// of the last file.)
				StrFormat(m_strError, "Failed to upload %s", localFile.c_str());
				Log(m_strError.c_str(), FTPLOG_ERROR);
			}
			continue;
		}
		// UnleashX servers create folders as files arrive.
		if (m_bUnleashXServer)
			continue;
		remotePath = MapUploadPath(path, source, destination);
		BOOL create = parentCreated;
		if (!parentCreated)
		{
			// MKD unless the parent's listing has it.
			int slash = StrReverseFind(remotePath, "/", (int)remotePath.size() - 2);
			if (slash != -1)
			{
				std::string name = StrMid(remotePath, slash + 1, (int)remotePath.size() - (slash + 1));
				std::string parent = StrLeft(remotePath, slash + 1);
				if (ChangeDir(parent.c_str()))
				{
					BOOL found = FALSE;
					WIN32_FIND_DATAA data;
					HANDLE find = FindFirst(parent.c_str(), &data);
					StrReplaceAll(name, "/", "");
					if (find != INVALID_HANDLE_VALUE)
					{
						while (!g_bCancelOperation)
						{
							if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
							    StrCmpNoCase(name.c_str(), data.cFileName) == 0)
							{
								found = TRUE;
								break;
							}
							if (!FindNext(find, &data))
								break;
						}
					}
					FindClose(find);
					parentCreated = !found;
					create = parentCreated;
				}
			}
		}
		if (create && !CreateDirectory(remotePath.c_str(), NULL))
		{
			result = FALSE;
			break;
		}
		Sleep(5);
	}
	return result;
}

// 0x000863E3
BOOL CDirFTP::UploadFolderBoost(const char *local, const char *remote, BOOL failIfExists)
{
	std::string reply, command;
	BOOL result = FALSE;
	BOOL started = FALSE;
	while (g_bFileOperationActive)
		Sleep(50);
	std::string source = local;
	std::string destination = remote;
	std::vector<SFileListItem> list;
	list.push_back(SFileListItem(std::string(local), TRUE, 0));
	g_bFileOperationActive = TRUE;
	CollectFiles(local, &list);
	if (g_ullOperationBytes != 0)
	{
		ULONGLONG freeSpace = GetFreeSpace(remote);
		if (freeSpace != 0 && freeSpace < g_ullOperationBytes)
		{
			m_strError = "Not enough free space on target host.";
			Log(m_strError.c_str(), FTPLOG_ERROR);
			g_bFileOperationActive = FALSE;
			return FALSE;
		}
	}
	g_bFileOperationActive = FALSE;
	g_ullOperationBytes = 0;
	g_ullOperationDone = 0;
	if (list.size() == 0)
	{
		CloseData();
		return result;
	}

	StrFormat(command, "SITE BOOST START UPLOAD %u", (unsigned)list.size());
	if (SendCommand(command.c_str(), TRUE, TRUE) == -1)
	{
		CloseData();
		return result;
	}
	if (ReadReplyCode(reply) != 226)
	{
		result = UploadFolder(local, remote, failIfExists);
		CloseData();
		return result;
	}
	Log("Initiating Boost mode transfer.", FTPLOG_STATUS);

	// The destination as an Xbox path: "/C/x" -> "C:\x".
	BOOL driveColon = StrLeft(destination, 1).compare("/") == 0 && StrFind(destination, ":") == 2;
	if (driveColon)
		destination = StrMid(destination, 1, (int)destination.size() - 1);
	if (StrFind(destination, "/", 1) == 2)
	{
		StrReplaceAll(destination, "/", "\\");
		std::string rest = StrMid(destination, 2, (int)destination.size() - 2);
		destination = StrMid(destination, 1, 1) + ':' + rest;
	}

	// 1. The list: "D,<folder>" and "F,<name>,<size>" lines, then an
	// empty line.
	std::string line, name;
	result = TRUE;
	for (std::vector<SFileListItem>::iterator it = list.begin(); it != list.end(); ++it)
	{
		if (g_bCancelOperation)
		{
			Log("Upload Operation Canceled.", FTPLOG_STATUS);
			result = FALSE;
			break;
		}
		name = it->path;
		if (!it->isFolder)
		{
			int slash = StrReverseFind(name, "\\");
			if (slash != -1)
				name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
			StrFormat(line, "F,%s,%u\r\n", name.c_str(), (unsigned)it->size);
			g_ullOperationBytes += it->size;
		}
		else
		{
			StrReplaceAll(name, source.c_str(), "");
			name = destination + name;
			StrFormat(line, "D,%s\r\n", name.c_str());
		}
		if (FAILED(SendAll(m_pControl->m_socket, line.c_str(), (int)line.size())))
		{
			result = FALSE;
			break;
		}
	}
	if (result)
	{
		line = "\r\n";
		if (FAILED(SendAll(m_pControl->m_socket, line.c_str(), (int)line.size())))
			result = FALSE;
		else
			Sleep(500);
	}

	// 2. "226 - <entries> <port>": the data connection to that port.
	// (Any failure before this reply goes straight to a normal upload.)
	if (result)
	{
		Sleep(50);
		result = FALSE;
		if (ReadReplyCode(reply) == 226)
		{
			if (SendBoostFiles(list, source, remote, reply, &started))
				result = TRUE;
			// 4. "226 Ending Boost Mode. Transfer Complete."
			if (!result)
				CloseData();
			ReadReplyCode(reply);
			if (result || started)
			{
				CloseData();
				return result;
			}
		}
	}
	CloseData();
	Log("Boost mode failed. Transfering in Normal mode.", FTPLOG_ERROR);
	result = UploadFolder(local, remote, failIfExists);
	CloseData();
	return result;
}

// Steps 2 and 3 of the boost upload: reply is the 226's text. started:
// a file was opened. (inlined in UploadFolderBoost)
BOOL CDirFTP::SendBoostFiles(const std::vector<SFileListItem> &list, const std::string &source, const char *remote,
                             std::string &reply, BOOL *started)
{
	StrReplaceAll(reply, "-", "");
	StrTrimRight(StrTrimLeft(reply));
	int space = StrFind(reply, " ");
	if (space == -1 || (unsigned)atol(StrLeft(reply, space).c_str()) != list.size())
		return FALSE;
	std::string text = StrMid(reply, space, (int)reply.size() - space);
	StrTrimRight(StrTrimLeft(text));
	reply = text;
	unsigned port = (unsigned)atol(reply.c_str());
	if (port == 0)
		return FALSE;

	m_dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	std::string ip;
	// (The address of the last PASV.)
	StrFormat(ip, "%d.%d.%d.%d", m_pasvAddress[0], m_pasvAddress[1], m_pasvAddress[2], m_pasvAddress[3]);
	sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(ip.c_str());
	address.sin_port = htons((WORD)port);
	BOOL connected = FALSE;
	for (int tries = 5; tries != 0; tries--)
	{
		if (connect(m_dataSocket, (sockaddr *)&address, sizeof(address)) == 0)
		{
			connected = TRUE;
			break;
		}
		Sleep(100);
	}
	if (!connected)
	{
		// (One more reply read than the server sends.)
		if (!m_bUnleashXServer)
			ReadReplyCode(reply);
		return FALSE;
	}

	// 3. The files back to back.
	int timeout = 15000;
	setsockopt(m_dataSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, 4);
	timeout = 20000;
	setsockopt(m_dataSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, 4);
	g_dwOperationUnknown640 = GetTickCount();
	m_dwStartTick = g_dwOperationUnknown640;
	g_dwOperationElapsed = 0;
	g_dwOperationRemaining = 0;
	DWORD lastSecond = 0;
	BOOL result = TRUE;
	std::string remotePath;
	for (std::vector<SFileListItem>::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if (g_bCancelOperation)
		{
			Log("Upload Operation Canceled.", FTPLOG_STATUS);
			result = FALSE;
			break;
		}
		// Folders and empty files have nothing to send.
		if (it->isFolder || it->size == 0)
			continue;
		remotePath = MapUploadPath(it->path, source, std::string(remote));
		memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
		memset(g_szOperationTarget, 0, sizeof(g_szOperationTarget));
		swprintf(g_szOperationFile, 0x12F, L"%hs", it->path.c_str());
		swprintf(g_szOperationTarget, 0x12F, L"%hs", remotePath.c_str());
		IFile *file = CreateFileObject(it->path.c_str());
		if (file == NULL || !file->OpenRead(it->path.c_str(), 0, TRUE))
		{
			result = FALSE;   // (the file object isn't freed)
			break;
		}
		*started = TRUE;
		DWORD sent = 0;
		if (!file->IsEOF())
		{
			result = SUCCEEDED(file->ReadNext(0));
			for (;;)
			{
				if (m_bAbort || !result || g_bCancelOperation)
					break;
				DWORD size;
				if (!file->WaitRead(TRUE, &size) || size == 0)
				{
					result = file->IsEOF();
					break;
				}
				BYTE *data = file->GetData();
				result = SUCCEEDED(file->ReadNext(0));
				if (data == NULL || FAILED(SendAll(m_dataSocket, (const char *)data, size)))
				{
					result = FALSE;
					break;
				}
				sent += size;
				g_ullOperationDone += size;
				// (Unlike the other loops: the elapsed time is always
				// updated, and the rate is of everything sent.)
				g_dwOperationElapsed = (GetTickCount() - g_dwOperationUnknown640) / 1000;
				if (g_ullOperationBytes != 0)
				{
					DWORD elapsed = (GetTickCount() - g_dwOperationUnknown640) / 1000;
					g_dwOperationElapsed = elapsed;
					g_dFilePercent = (double)(LONGLONG)(g_ullOperationDone * 100) / (double)(LONGLONG)g_ullOperationBytes;
					if (elapsed != 0 && elapsed != lastSecond)
					{
						lastSecond = elapsed;
						ULONGLONG perSecond = g_ullOperationDone / elapsed;
						g_dwOperationRemaining = (DWORD)((g_ullOperationBytes - g_ullOperationDone) / (perSecond + 1)) + 1;
						g_dOperationRate = (double)(LONGLONG)g_ullOperationDone / (double)(elapsed << 10);
					}
				}
				if (file->IsEOF())
				{
					result = TRUE;
					break;
				}
			}
		}
		else
		{
			result = TRUE;
		}
		delete file;
		if (!result)
			break;
		m_ullBytesSent += (LONGLONG)(int)sent;
		m_nFilesSent++;
	}
	m_dwEndTick = GetTickCount();
	m_dwTransferMs += m_dwEndTick - m_dwStartTick;
	return result;
}

// 0x00082346
BOOL CDirFTP::CopyFile(const char *source, const char *destination, BOOL failIfExists)
{
	return CopyFileEx(source, destination, NULL, NULL, NULL, failIfExists != 0);
}

// 0x000870E7
BOOL CDirFTP::CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress,
                         void * /*data*/, BOOL * /*cancel*/, DWORD flags)
{
	if (!StrIsNotEmpty(source) || !StrIsNotEmpty(destination))
		return FALSE;
	g_pFtpCopyProgress = progress;
	BOOL failIfExists = flags == COPY_FILE_FAIL_IF_EXISTS;
	BOOL result;
	if (strstr(source, ":\\") != NULL)
	{
		// Up.
		g_ullOperationBytes = 0;
		g_ullOperationDone = 0;
		if (flags & COPY_FTP_FOLDER)
		{
			if (m_bBoost)
				result = UploadFolderBoost(source, destination, failIfExists);
			else
				result = UploadFolder(source, destination, failIfExists);
		}
		else
		{
			SetBinary(TRUE);
			result = UploadFile(source, destination);
		}
	}
	else
	{
		// Down.
		std::string path = source;
		if (StrRight(path, 1).compare("/") != 0)
		{
			g_ullOperationBytes = 0;
			g_ullOperationDone = 0;
			SetBinary(TRUE);
			result = DownloadFile(source, destination, failIfExists);
		}
		else
		{
			result = DownloadFolder(source, destination, failIfExists);
		}
	}
	CloseData();
	g_pFtpCopyProgress = NULL;
	return result;
}
