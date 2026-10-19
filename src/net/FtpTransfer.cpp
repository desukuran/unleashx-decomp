// CFTPConnection transfers: data connections (PORT and PASV), listings,
// RETR/STOR/APPE and the Avalaunch-compatible boost mode (SITE BOOST).
#include "FtpServer.h"

#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/FileIO.h"
#include "../system/SystemControl.h"
#include "Network.h"

static const char *const s_months[13] =   // 0x0026BB00
{
	" ", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// The PASV reply's address, "a,b,c,d", worked out once for all connections.
// Quirk: it's never refreshed if the IP address changes. 0x0027A3B4
static std::string s_pasvAddress;

static const DWORD kBoostBufferSize = 0x100000;

// 0x0002F934
void CFTPConnection::CloseDataConnection()
{
	if (m_dataSocket != INVALID_SOCKET)
	{
		shutdown(m_dataSocket, SD_BOTH);
		closesocket(m_dataSocket);
		m_dataSocket = INVALID_SOCKET;
	}
	m_bPassive = FALSE;
	m_dwRestOffset = 0;
	m_bAbort = FALSE;
}

// 0x000B3C00
// Counts the transfer for the connection and the server, and the first time
// raises the connection thread's priority (above normal while transfers
// run; ThreadProc() lowers it again after five idle seconds).
void CFTPConnection::SetTransferActive(BOOL active)
{
	if (active)
		m_nActiveTransfers++;
	else if (m_nActiveTransfers != 0)
		m_nActiveTransfers--;
	m_pServer->SetTransferActive(active);
	if (active && !m_bPriorityRaised)
	{
		SetThreadPriority(m_hThread, m_nActiveTransfers != 0 ? THREAD_PRIORITY_ABOVE_NORMAL : THREAD_PRIORITY_NORMAL);
		m_bPriorityRaised = TRUE;
	}
}

// 0x000B6611
// Active mode: connects to the client's PORT address (or, without a PORT
// command, the control connection's address with the garbled port from
// SetClientAddress()). Waits up to the receive timeout (15 seconds when
// replying, else 10) for the connection.
BOOL CFTPConnection::OpenDataConnection(BOOL replyOnError)
{
	if (m_bPassive && m_dataSocket != INVALID_SOCKET)
		return m_dataSocket != INVALID_SOCKET;
	if (m_dataSocket != INVALID_SOCKET)
		return TRUE;

	m_dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	std::string unused;
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = m_clientIp[0] | (m_clientIp[1] << 8) | (m_clientIp[2] << 16) | ((DWORD)m_clientIp[3] << 24);
	if (m_wDataPort == 0)
		m_wDataPort = (WORD)((m_clientPort[0] << 8) + m_clientPort[1]);
	addr.sin_port = htons(m_wDataPort);

	int bufferSize = 0x10000;
	setsockopt(m_dataSocket, SOL_SOCKET, SO_SNDBUF, (char *)&bufferSize, 4);
	bufferSize = 0x20000;
	setsockopt(m_dataSocket, SOL_SOCKET, SO_RCVBUF, (char *)&bufferSize, 4);
	int sendTimeout = replyOnError ? 15000 : 3000;
	setsockopt(m_dataSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&sendTimeout, 4);
	int recvTimeout = replyOnError ? 15000 : 10000;
	setsockopt(m_dataSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recvTimeout, 4);
	unsigned long nonBlocking = 1;
	ioctlsocket(m_dataSocket, FIONBIO, &nonBlocking);

	int result = connect(m_dataSocket, (sockaddr *)&addr, sizeof(addr));
	if (result == -1)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			fd_set writable;
			FD_ZERO(&writable);
			FD_SET(m_dataSocket, &writable);
			timeval timeout;
			timeout.tv_sec = recvTimeout / 1000;
			timeout.tv_usec = 0;
			if (select(1, NULL, &writable, NULL, &timeout) > 0)
				result = 0;
		}
	}
	if (result != 0)
	{
		CloseDataConnection();
		if (replyOnError)
			Reply(425, "Can't open data connection.");
		return FALSE;
	}

	if (m_dataSocket != INVALID_SOCKET)
	{
		unsigned long blocking = 0;
		ioctlsocket(m_dataSocket, FIONBIO, &blocking);
	}
	return m_dataSocket != INVALID_SOCKET;
}

// 0x000B5CA5
// Listens on m_wPasvPort, sends the 227 reply and waits (without a timeout)
// for the client to connect.
BOOL CFTPConnection::OpenPassiveConnection()
{
	BOOL connected = FALSE;
	CloseDataConnection();
	if (m_localIp[0] == 0 && m_localIp[1] == 0 && m_localIp[2] == 0 && m_localIp[3] == 0)
		return FALSE;

	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET)
		return FALSE;

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	int addrLen = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_wPasvPort);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	char reuse = 1;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, 1);
	if (bind(listener, (sockaddr *)&addr, addrLen) == 0 && listen(listener, 0x7FFFFFFF) == 0)
	{
		if (m_wPasvPort != 0)
		{
			if (s_pasvAddress.size() == 0)
			{
				StrFormat(s_pasvAddress, "%d.%d.%d.%d", m_localIp[0], m_localIp[1], m_localIp[2], m_localIp[3]);
				StrReplaceAll(s_pasvAddress, ".", ",");
			}
			if (s_pasvAddress.compare("0,0,0,0") == 0)
			{
				s_pasvAddress = "";
				DWORD ip = GetIPAddress();
				m_localIp[0] = (BYTE)ip;
				m_localIp[1] = (BYTE)(ip >> 8);
				m_localIp[2] = (BYTE)(ip >> 16);
				m_localIp[3] = (BYTE)(ip >> 24);
				if (ip == 0)
					goto done;
				StrFormat(s_pasvAddress, "%d,%d,%d,%d", m_localIp[0], m_localIp[1], m_localIp[2], m_localIp[3]);
			}

			std::string text;
			StrFormat(text, "Entering Passive Mode (%s,%d,%d)", s_pasvAddress.c_str(), m_wPasvPort >> 8,
			          m_wPasvPort % 256);
			Reply(227, text.c_str());
			int bufferSize = 0x10000;
			setsockopt(listener, SOL_SOCKET, SO_SNDBUF, (char *)&bufferSize, 4);
			bufferSize = 0x20000;
			setsockopt(listener, SOL_SOCKET, SO_RCVBUF, (char *)&bufferSize, 4);
			m_dataSocket = accept(listener, NULL, (socklen_t *)&addrLen);
		}
		connected = m_dataSocket != INVALID_SOCKET;
	}
done:
	shutdown(listener, SD_BOTH);
	closesocket(listener);
	return connected;
}

// The random part of picking a passive port (inlined three times): a random
// port in [min, max), with the high byte of the random number at least 4.
static DWORD RandomPasvPort(WORD range)
{
	BYTE random[2];
	XNetRandom(random, 2);
	DWORD high = random[0];
	if (high < 4)
		high += 4;
	WORD value = (WORD)((high << 8) + random[1]);
	return value % range;
}

// 0x000B6CAE
void CFTPConnection::CmdPasv(std::string)
{
	CFTPServer *server = m_pServer;
	int difference = (int)(server->m_dwPasvPortMax - server->m_dwPasvPortMin);
	WORD range = (WORD)(difference < 0 ? -difference : difference);
	if (range < 1)
	{
		m_wPasvPort = (WORD)(server->m_dwPasvPortMin >= 0x400 ? server->m_dwPasvPortMin : 0x400);
	}
	else
	{
		int port = (int)RandomPasvPort(range) + (WORD)server->m_dwPasvPortMin;
		if (port < 0x400)
			port = 0x400;
		WORD previous = m_wPasvPort;
		m_wPasvPort = (WORD)port;
		if (previous == m_wPasvPort)
		{
			// don't hand out the same port twice in a row
			WORD max = (WORD)server->m_dwPasvPortMax;
			if (m_wPasvPort != max)
				m_wPasvPort = (WORD)(m_wPasvPort + 1 < max ? m_wPasvPort + 1 : max);
			else if (m_wPasvPort == server->m_dwPasvPortMin)
				m_wPasvPort = (WORD)(m_wPasvPort + 1);
			else
				m_wPasvPort = (WORD)server->m_dwPasvPortMin;
		}
	}

	if (!OpenPassiveConnection())
	{
		m_bPassive = FALSE;
		ReplyNotImplemented(NULL);
		CloseDataConnection();
	}
	else
	{
		m_bPassive = TRUE;
	}
}

// 0x000B6E7E
// PORT a,b,c,d,p1,p2. An address of 0,0,0,0 keeps the control connection's.
// Connects right away.
void CFTPConnection::CmdPort(std::string arg)
{
	CloseDataConnection();
	unsigned a = 0, b = 0, c = 0, d = 0, p1 = 0, p2 = 0;
	sscanf(arg.c_str(), "%u,%u,%u,%u,%u,%u", &a, &b, &c, &d, &p1, &p2);
	if (a != 0 || b != 0 || c != 0 || d != 0)
	{
		m_clientIp[0] = (BYTE)a;
		m_clientIp[1] = (BYTE)b;
		m_clientIp[2] = (BYTE)c;
		m_clientIp[3] = (BYTE)d;
	}
	m_clientPort[0] = (BYTE)p1;
	m_clientPort[1] = (BYTE)p2;
	m_wDataPort = (WORD)((p1 << 8) + p2);
	m_bPassive = FALSE;
	if (m_wDataPort == 0)
	{
		Reply(501, "Syntax error in parameters or arguments.");
		return;
	}
	Sleep(10);
	if (OpenDataConnection(TRUE))
		Reply(200, "PORT command successful.");
}

// ---- listings ----

// 0x000B86E3
// The listing of arg (or the current directory), long ("ls -l" style) or
// names only. The root lists the drives. Quirks: options other than "-aL"
// (case-sensitive) end up in the path; the root listing ignores namesOnly;
// the date shown is the creation time.
std::string CFTPConnection::ListDirectory(BOOL namesOnly, const char *arg)
{
	std::string listing;
	std::string ftpPath;

	if ((int)m_strCwd.size() < 2 && (arg == NULL || arg[0] == '\0' || _mbsicmp(arg, "-al") == 0))
	{
		// the drives, all with the date of C:\xboxdash.xbe (or Oct 15 2001)
		RefreshDrives(FALSE);
		std::string line;
		char date[0x20];
		memset(date, 0, sizeof(date));
		WIN32_FILE_ATTRIBUTE_DATA data;
		FILETIME local;
		SYSTEMTIME time;
		GetFileAttributesExA("C:\\xboxdash.xbe", GetFileExInfoStandard, &data);
		FileTimeToLocalFileTime(&data.ftLastWriteTime, &local);
		FileTimeToSystemTime(&local, &time);
		if (time.wYear < 2001)
		{
			time.wMonth = 10;
			time.wDay = 15;
			time.wYear = 2001;
		}
		for (int i = 0; i < DRIVE_INFO_COUNT; i++)
		{
			SDriveInfo *info = &g_drives[i];
			if (!info->mounted || !info->hasMedia || info->letter == 'U' || info->letter == 'T')
				continue;
			sprintf(date, "%s %02d %02d:%02d", s_months[time.wMonth], time.wDay, time.wHour, time.wMinute);
			StrFormat(line, "drw-r--r--   1 XBOX      XBOX        %d %s %c\r\n", 0, date, info->letter);
			listing.append(line);
		}
		return listing;
	}

	std::string path = arg != NULL ? arg : "";
	std::string xboxPath;
	StrReplaceAll(path, "-aL", "");
	StrReplaceAll(path, ":", "");
	if (arg != NULL && arg[0] != '\0')
		ResolvePath(&ftpPath, m_strCwd.c_str(), path.c_str());
	else
		ResolvePath(&ftpPath, m_strCwd.c_str(), NULL);
	FtpPathToXbox(&xboxPath, ftpPath.c_str());

	std::string line;
	char date[0x20];
	memset(date, 0, sizeof(date));
	if (StrRight(xboxPath, 1) == "\\")
		xboxPath.append("*");
	else if (StrRight(xboxPath, 1) != "*")
		xboxPath.append("\\*");

	listing = namesOnly ? "..\r\n" : "drw-r--r--   1 XBOX      XBOX        0 Nov 15 08:00 ..\r\n";

	CDir *dir = CreateDirObject(xboxPath.c_str());
	if (dir != NULL)
	{
		WIN32_FIND_DATAA data;
		HANDLE find = dir->FindFirst(xboxPath.c_str(), &data);
		if (find != INVALID_HANDLE_VALUE)
		{
			do
			{
				line = "";
				if (!namesOnly)
				{
					FILETIME local;
					SYSTEMTIME time;
					FileTimeToLocalFileTime(&data.ftCreationTime, &local);
					FileTimeToSystemTime(&local, &time);
					sprintf(date, "%s %02d %02d:%02d", s_months[time.wMonth], time.wDay, time.wHour, time.wMinute);
					StrFormat(line, "%cr%c-r--r--   1 XBOX      XBOX        %u %s %s\r\n",
					          (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 'd' : '-',
					          (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? '-' : 'w', data.nFileSizeLow,
					          date, data.cFileName);
				}
				else
				{
					StrFormat(line, "%s\r\n", data.cFileName);
				}
				listing.append(line);
			} while (dir->FindNext(find, &data));
			dir->FindClose(find);
		}
		delete dir;
	}
	return listing;
}

// 0x000B6F64
HRESULT CFTPConnection::SendListing(std::string listing)
{
	if (listing.size() == 0)
	{
		CloseDataConnection();
		Reply(226, "Transfer Complete.");
		return S_OK;
	}
	if (!OpenDataConnection(TRUE))
		return E_FAIL;

	DWORD size = listing.size();
	if (FtpSendAll(m_dataSocket, listing.c_str(), size) < 0)
	{
		CloseDataConnection();
		Reply(426, "Connection closed, transfer aborted.");
		return E_FAIL;
	}
	CloseDataConnection();
	Reply(226, "Transfer complete.");
	if (size != 0)
		m_pServer->AddStat(FTPSTAT_BYTES_SENT, size);
	return S_OK;
}

// 0x000B91E7
void CFTPConnection::CmdList(std::string arg)
{
	Reply(150, "Opening ASCII mode data connection for list.");
	SendListing(ListDirectory(FALSE, arg.c_str()));
}

// 0x000B918A
// Quirk: lists the current directory whatever the argument.
void CFTPConnection::CmdNlst(std::string)
{
	Reply(150, "Opening ASCII mode data connection for ls.");
	SendListing(ListDirectory(TRUE, NULL));
}

// ---- RETR ----

// 0x000B7042
// Reads with the virtual file layer (double-buffered) and sends each block.
// A REST offset is rounded down to 2 KB for the seek and the rest skipped in
// the first block. The byte count goes to the statistics every sixth block.
HRESULT CFTPConnection::SendFile(const char *path, DWORD offset)
{
	HRESULT hr = E_FAIL;
	Reply(150, "Opening BINARY mode data connection for file transfer.");
	if (m_bAbort || !OpenDataConnection(TRUE))
	{
		m_pServer->AddStat(FTPSTAT_FAILED_SENDS, 1);
		return E_FAIL;
	}

	CFile *file = CreateFileObject(path);
	if (file == NULL)
	{
		Reply(550, "Internal Server Error.");
		m_pServer->AddStat(FTPSTAT_FAILED_SENDS, 1);
		CloseDataConnection();
		return hr;
	}

	if (file->OpenRead(path, 0, TRUE) && !file->IsEOF())
	{
		if (offset != 0)
		{
			file->Seek(offset & ~0x7FF, NULL, FILE_BEGIN);
			offset &= 0x7FF;
		}
		BOOL ok = file->ReadNext(0) >= 0;
		int blocks = 0;
		DWORD unreported = 0;
		while (!m_bAbort)
		{
			if (!ok)
				break;
			DWORD length;
			if (!file->WaitRead(TRUE, &length) || length <= offset)
			{
				ok = file->IsEOF();
				break;
			}
			BYTE *data = file->GetData();
			ok = file->ReadNext(0) >= 0;   // the next block reads while this one is sent
			if (data == NULL)
			{
				ok = FALSE;
				break;
			}
			if (FtpSendAll(m_dataSocket, (const char *)data + offset, length - offset) < 0)
			{
				ok = FALSE;
				break;
			}
			if (blocks > 5)
			{
				if (unreported != 0)
					m_pServer->AddStat(FTPSTAT_BYTES_SENT, unreported);
				blocks = 0;
				unreported = length;
			}
			else
			{
				blocks++;
				unreported += length;
			}
			if (file->IsEOF())
			{
				ok = TRUE;
				break;
			}
			offset = 0;
		}
		file->Close();
		if ((int)unreported > 0)
			m_pServer->AddStat(FTPSTAT_BYTES_SENT, unreported);

		if (m_bAbort || !ok)
		{
			CloseDataConnection();
			Reply(426, "Connection closed, transfer aborted.");
			m_pServer->AddStat(FTPSTAT_FAILED_SENDS, 1);
		}
		else
		{
			hr = S_OK;
			Reply(226, "Transfer complete.");
			m_pServer->AddStat(FTPSTAT_FILES_SENT, 1);
		}
	}
	else if (file->IsEOF())
	{
		// an empty file
		hr = S_OK;
		Reply(226, "Transfer complete.");
		m_pServer->AddStat(FTPSTAT_FILES_SENT, 1);
		file->Close();
	}
	else
	{
		Reply(550, "File not found.");
		m_pServer->AddStat(FTPSTAT_FAILED_SENDS, 1);
	}
	delete file;
	CloseDataConnection();
	return hr;
}

// 0x000B94EA
// Quirk: the existence check is skipped (mustExist is FALSE), so "File not
// found." comes from SendFile() instead.
void CFTPConnection::CmdRetr(std::string arg)
{
	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	if (!ResolveFilePath(&xboxPath, arg.c_str(), FALSE))
	{
		Reply(550, "File not found.");
	}
	else
	{
		BOOL led = SetLedPattern(m_pSettings->m_prefs.m_nLEDFTPSend, FALSE);
		m_bAbort = FALSE;
		SetTransferActive(TRUE);
		SendFile(xboxPath.c_str(), m_dwRestOffset);
		SetTransferActive(FALSE);
		if (led)
			SetLedPattern(0, FALSE);
	}
	m_dwRestOffset = 0;
}

// ---- STOR / APPE ----

// 0x000B737D
// APPE's receiver: plain Win32 file I/O with the 16 KB transfer buffer.
// resume (with a REST offset) opens the existing file and seeks there,
// otherwise the file is truncated.
HRESULT CFTPConnection::ReceiveFile(std::string path, BOOL resume)
{
	HRESULT hr = E_FAIL;
	BYTE *buffer = m_pTransferBuffer;
	DWORD bufferSize = 0x4000;
	BYTE stackBuffer[0x1000];

	if (m_clientIp[0] == 0)
	{
		Reply(425, "Can't open data connection.");
		return E_FAIL;
	}

	int slash = (int)path.rfind('\\');
	if (!m_pSettings->m_prefs.m_bAutoRename && slash != -1 && (int)path.size() - slash - 1 > 42)
	{
		Reply(450, "Filename too long. Requested file action not taken.");
		m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
		return E_FAIL;
	}

	Reply(150, "Opening BINARY mode data connection for file transfer.");
	if (!OpenDataConnection(TRUE))
	{
		m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
		return E_FAIL;
	}

	if (buffer == NULL)
	{
		buffer = stackBuffer;
		bufferSize = sizeof(stackBuffer);
	}
	BOOL renamed = FALSE;
	if (m_pSettings->m_prefs.m_bAutoRename)
		renamed = MakeFatxPath(path.c_str(), path);

	if (m_dwRestOffset == 0 && resume)
		resume = FALSE;
	HANDLE file = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, resume ? OPEN_EXISTING : CREATE_ALWAYS,
	                          FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		Reply(450, "Cannot access file.");
		m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
		CloseDataConnection();
		return E_FAIL;
	}

	// Quirk: tries to extend the file to 0x1FFFFFFFE bytes (meant as a
	// preallocation); that fails on FATX and is skipped.
	LONG high = 1;
	SetFilePointer(file, -2, &high, FILE_BEGIN);
	if (GetLastError() == 0)
		SetEndOfFile(file);
	SetFilePointer(file, resume && m_dwRestOffset != 0 ? m_dwRestOffset : 0, NULL, FILE_BEGIN);

	int received = recv(m_dataSocket, (char *)buffer, bufferSize, 0);
	int blocks = 0;
	DWORD unreported = 0;
	BOOL written = TRUE;
	if (received != 0)
	{
		while (received != -1 && !m_bAbort)
		{
			DWORD done = 0;
			while (done != (DWORD)received)
			{
				DWORD count = 0;
				written = WriteFile(file, buffer, received, &count, NULL);
				if (!written)
				{
					Reply(451, "Cannot access file.");
					hr = E_FAIL;
					break;
				}
				done += count;
			}
			if (!written)
				break;

			// Quirk: every seventh block isn't counted in the statistics.
			if (blocks > 5)
			{
				if (unreported != 0)
					m_pServer->AddStat(FTPSTAT_BYTES_RECEIVED, unreported);
				blocks = 0;
				unreported = 0;
			}
			else
			{
				blocks++;
				unreported += received;
			}

			received = recv(m_dataSocket, (char *)buffer, bufferSize, 0);
			if (received == -1 || received == 0)
				break;
		}
		if ((int)unreported > 0)
			m_pServer->AddStat(FTPSTAT_BYTES_RECEIVED, unreported);
	}

	CloseDataConnection();
	if (received == 0)
	{
		if (renamed)
		{
			std::string text;
			StrFormat(text, "Transfer Complete. File renamed to '%s'", path.c_str());
			Reply(226, text.c_str());
		}
		else
		{
			Reply(226, "Transfer Complete.");
		}
		hr = S_OK;
		m_pServer->AddStat(FTPSTAT_FILES_RECEIVED, 1);
	}
	else
	{
		// Quirk: a write error gets this second reply too, and a closed
		// connection returns the (positive) WSA error code.
		if (received == -1)
		{
			Reply(426, "Connection closed, transfer aborted.");
			hr = WSAGetLastError();
		}
		else
		{
			Reply(451, "Socket error during transfer.");
		}
		m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
	}
	SetEndOfFile(file);
	CloseHandle(file);
	CloseDataConnection();
	return hr;
}

// 0x000B924F
// APPE: resumes at the end of an existing file (REST is set to its size).
// Quirk: the "XBOX root" check refuses a relative name in the root but not
// an absolute one.
void CFTPConnection::CmdAppe(std::string arg)
{
	if (m_bAnonymous)
	{
		Reply(550, "Permission denied - (Anonymous user).");
		return;
	}

	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	if (ftpPath.size() == arg.size() + 1)
	{
		Reply(550, "Permission denied - Storing in XBOX Root not allowed.");
		return;
	}
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	HANDLE file = CreateFileA(xboxPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	                          FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != NULL && file != INVALID_HANDLE_VALUE)
	{
		BY_HANDLE_FILE_INFORMATION info;
		memset(&info, 0, sizeof(info));
		GetFileInformationByHandle(file, &info);
		m_dwRestOffset = info.nFileSizeLow;
		CloseHandle(file);
	}
	ReceiveFile(xboxPath, TRUE);
	m_dwRestOffset = 0;
}

// 0x000B7C77
// STOR's receiver: the virtual file layer's double buffer (two 1 MB blocks
// written by its own thread). Quirks: if no file object can be made, or the
// file can't be opened, the client still gets "Transfer Complete." (after
// "Cannot access file." in the second case); a dropped connection or ABOR
// also ends as a complete transfer.
HRESULT CFTPConnection::ReceiveFileBuffered(std::string path, BOOL resume)
{
	HRESULT hr;
	if (m_clientIp[0] == 0)
	{
		Reply(425, "Can't open data connection.");
		return E_FAIL;
	}

	int slash = (int)path.rfind('\\');
	if (!m_pSettings->m_prefs.m_bAutoRename && slash != -1 && (int)path.size() - slash - 1 > 42)
	{
		Reply(450, "Filename too long. Requested file action not taken.");
		m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
		return E_FAIL;
	}

	Reply(150, "Opening BINARY mode data connection for file transfer.");
	if (!OpenDataConnection(TRUE))
	{
		m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
		return E_FAIL;
	}

	DWORD filled = 0;
	BOOL ok = TRUE;
	BOOL renamed = FALSE;
	if (m_pSettings->m_prefs.m_bAutoRename)
		renamed = MakeFatxPath(path.c_str(), path);
	if (m_dwRestOffset == 0 && resume)
		resume = FALSE;

	CFile *file = CreateFileObject(path.c_str());
	if (file != NULL)
	{
		if (!file->OpenWrite(path.c_str(), kBoostBufferSize, 0, 0, resume ? OPEN_ALWAYS : CREATE_ALWAYS))
		{
			Reply(450, "Cannot access file.");
			m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
		}
		else
		{
			if (m_dwRestOffset != 0 && resume)
				file->Seek(m_dwRestOffset, NULL, FILE_BEGIN);

			int current = 0;
			DWORD size;
			BYTE *buffers[2];
			buffers[0] = file->GetBuffer(TRUE, &size);
			buffers[1] = file->GetBuffer(FALSE, &size);
			int received = recv(m_dataSocket, (char *)buffers[0], size, 0);
			if (received > 0)
			{
				do
				{
					if (m_bAbort)
						break;
					filled += received;
					if (filled == size)
					{
						if (file->Write(buffers[current], filled) < 0)
						{
							ok = FALSE;
							goto closeFile;
						}
						m_pServer->AddStat(FTPSTAT_BYTES_RECEIVED, filled);
						filled = 0;
						current ^= 1;
					}
					received = recv(m_dataSocket, (char *)buffers[current] + filled, size - filled, 0);
				} while (received > 0);
				// the rest, at the end of the data or on ABOR
				if (filled != 0 && file->Write(buffers[current], filled) < 0)
					ok = FALSE;
			}
			if (filled != 0)
				m_pServer->AddStat(FTPSTAT_BYTES_RECEIVED, filled);
		closeFile:
			file->Close();
			if (file->GetError() != 0)
				ok = FALSE;
		}
		delete file;
		if (!ok)
		{
			Reply(451, "Error writing file.");
			CloseDataConnection();
			return E_FAIL;
		}
	}

	if (renamed)
	{
		std::string text;
		StrFormat(text, "Transfer Complete. File renamed to '%s'", path.c_str());
		Reply(226, text.c_str());
	}
	else
	{
		Reply(226, "Transfer Complete.");
	}
	hr = S_OK;
	m_pServer->AddStat(FTPSTAT_FILES_RECEIVED, 1);
	CloseDataConnection();
	return hr;
}

// 0x000B93A1
void CFTPConnection::CmdStor(std::string arg)
{
	if (m_bAnonymous)
	{
		Reply(550, "Permission denied - (Anonymous user).");
		return;
	}

	std::string ftpPath;
	std::string xboxPath;
	ResolvePath(&ftpPath, m_strCwd.c_str(), arg.c_str());
	if (ftpPath.size() == arg.size() + 1)
	{
		Reply(550, "Permission denied - Storing in XBOX Root not allowed.");
		return;
	}
	FtpPathToXbox(&xboxPath, ftpPath.c_str());
	if ((int)xboxPath.size() > 0x130)
	{
		Reply(450, "Filename too long, requested file action not taken.");
	}
	else
	{
		BOOL led = SetLedPattern(m_pSettings->m_prefs.m_nLEDFTPReceive, FALSE);
		SetTransferActive(TRUE);
		ReceiveFileBuffered(xboxPath, m_dwRestOffset != 0);
		SetTransferActive(FALSE);
		if (led)
			SetLedPattern(0, FALSE);
	}
	m_dwRestOffset = 0;
}

// ---- boost mode ----
//
// Avalaunch's way of uploading a whole folder tree fast: the client sends
// "SITE BOOST START UPLOAD <n>" and then n lines on the control connection,
// "D,<folder>" or "F,<name>,<size>" (files belong to the last folder),
// listens to the server's "226 - <entries> <port>" reply and streams all
// file contents back to back over one data connection to that port.

static unsigned __stdcall BoostWriterThread(void *param)   // 0x000B5F5A
{
	return ((CFTPConnection *)param)->BoostWriterThreadProc();
}

// 0x000B8B49
// SITE BOOST SUPPORTED / START UPLOAD <n>. Quirk: START DOWNLOAD is
// recognized and then ignored, without a reply.
void CFTPConnection::SiteBoost(std::string arg)
{
	if (!m_pServer->m_bBoost)
	{
		ReplyNotImplemented(NULL);
		m_bBoostMode = FALSE;
		return;
	}
	m_bBoostMode = TRUE;

	if (StrCmpNoCase(arg.c_str(), "SUPPORTED") == 0)
	{
		if (m_bBoostMode)
			Reply(226, "Boost mode OK");
		else
			ReplyNotImplemented(NULL);
		return;
	}
	if ((int)arg.size() <= 11)
	{
		Reply(501, "Syntax error in parameters or arguments.");
		return;
	}

	std::string line;
	std::string ftpPath;
	if (StrCmpNoCase(StrLeft(arg, 12), "START UPLOAD") != 0)
	{
		StrCmpNoCase(StrLeft(arg, 14), "START DOWNLOAD");
		return;
	}

	m_files.clear();
	int pos = StrReverseFind(arg, " ", -1);
	if (pos == -1)
	{
		Reply(501, "Syntax error in parameters or arguments.");
		return;
	}
	DWORD count = atol(StrMid(arg, pos + 1, arg.size() - (pos + 1)).c_str());
	Reply(226, "Initiating Boost Mode. Start sending list.");

	// The list ends after count lines or at an empty line.
	std::string list;
	DWORD lines = 0;
	m_dwBoostTotalBytes = 0;
	for (;;)
	{
		memset(m_recvBuffer, 0, sizeof(m_recvBuffer));
		if (FtpRecvSome(this, m_ctrlSocket) != S_OK)
			break;
		line = m_recvBuffer;
		StrReplaceAll(line, "\n", "\r");
		StrReplaceAll(line, "\r\r", "\r");
		list.append(line);
		for (pos = StrFind(line, "\r", 0); pos != -1; pos = StrFind(line, "\r", pos + 1))
			lines++;
		if (lines >= count)
			break;
		if (StrFind(line, "\r\r", 0) != -1)
			break;
	}

	while ((pos = StrFind(list, "\r", 0)) != -1)
	{
		line = StrLeft(list, pos);
		list.erase(0, pos + 1);
		if (StrLeft(line, 1).compare("D") == 0)
		{
			line = StrMid(line, 2, line.size() - 2);
			if (StrFind(line, ":", 0) == -1)
			{
				ResolvePath(&ftpPath, m_strCwd.c_str(), line.c_str());
				FtpPathToXbox(&line, ftpPath.c_str());
			}
			m_files.push_back(MakeFtpFileEntry(TRUE, line, 0, 0, 0));
		}
		else if (StrLeft(line, 1).compare("F") == 0)
		{
			line = StrMid(line, 2, line.size() - 2);
			int comma = StrReverseFind(line, ",", -1);
			if (comma == -1)
			{
				Reply(501, "Syntax error in parameters or arguments.");
				return;
			}
			DWORD size = atol(StrMid(line, comma + 1, line.size() - (comma + 1)).c_str());
			line = StrLeft(line, comma);
			m_files.push_back(MakeFtpFileEntry(FALSE, line, size, 0, 0));
			m_dwBoostTotalBytes += size;
		}
	}

	// Everything else waits: the listening thread is suspended, this thread
	// runs at the highest priority with a five times longer quantum, and the
	// file cache grows to 12.5 MB.
	SetTransferActive(TRUE);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	m_pServer->Pause(TRUE);
	BOOL led = SetLedPattern(m_pSettings->m_prefs.m_nLEDFTPBoost, FALSE);
	DWORD cacheSize = XGetFileCacheSize();
	XSetFileCacheSize(0xC80000);
	DWORD quantum = XGetProcessQuantumLength();
	XSetProcessQuantumLength(quantum * 5);
	BoostReceive();
	XSetProcessQuantumLength(quantum);
	XSetFileCacheSize(cacheSize);
	m_pServer->Pause(FALSE);
	SetTransferActive(FALSE);
	if (led)
		SetLedPattern(0, FALSE);
	m_files.clear();
}

// The data port for boost mode: like PASV but without avoiding the last
// port, and not stored in m_wPasvPort (unless the range is empty).
static DWORD ChooseBoostPort(CFTPConnection *connection)
{
	CFTPServer *server = connection->m_pServer;
	int difference = (int)(server->m_dwPasvPortMax - server->m_dwPasvPortMin);
	WORD range = (WORD)(difference < 0 ? -difference : difference);
	if (range < 1)
	{
		DWORD port = server->m_dwPasvPortMin >= 0x400 ? server->m_dwPasvPortMin : 0x400;
		connection->m_wPasvPort = (WORD)port;
		return port;
	}
	DWORD offset = RandomPasvPort(range);
	if ((int)((WORD)server->m_dwPasvPortMin + offset) < 0x400)
		return 0x400;
	return offset + server->m_dwPasvPortMin;
}

// Listens on port (or up to five ports above it), sends "226 - <entries>
// <port>" and waits up to 15 seconds for the client. Shared by both boost
// receivers, which differ in the accept loop and the receive timeout.
static SOCKET BoostListen(DWORD *port, BOOL *listening)
{
	*listening = FALSE;
	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET)
		return INVALID_SOCKET;

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((WORD)*port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	int result = bind(listener, (sockaddr *)&addr, sizeof(addr));
	for (int tries = 0; result != 0 && tries < 5; tries++)
	{
		(*port)++;
		addr.sin_port = htons((WORD)*port);
		result = bind(listener, (sockaddr *)&addr, sizeof(addr));
	}
	if (result != 0)
		return INVALID_SOCKET;   // Quirk: the socket is leaked

	*listening = TRUE;
	return listener;
}

// 0x000B773D
// Receives into two 1 MB buffers while BoostWriterThreadProc() writes the
// other one. Falls back to BoostReceiveUnbuffered() if the buffers can't be
// allocated. Quirks: if the client closes the connection before everything
// arrived, recv() keeps returning 0 and this never ends; the buffers stay
// allocated when no data connection could be made.
HRESULT CFTPConnection::BoostReceive()
{
	if (m_files.size() == 0)
		return E_FAIL;

	HRESULT hr = E_FAIL;
	if (m_clientIp[0] == 0)
	{
		Reply(425, "Can't open data connection. Boost Mode terminated.");
		return E_FAIL;
	}

	// Quirk: the pointers aren't cleared, so a failed second allocation
	// leaves a stale one for the destructor.
	if (m_pBoostBuffer[0] != NULL)
		operator delete(m_pBoostBuffer[0]);
	if (m_pBoostBuffer[1] != NULL)
		operator delete(m_pBoostBuffer[1]);
	m_pBoostBuffer[0] = (BYTE *)XPhysicalAlloc(kBoostBufferSize, MAXULONG_PTR, 0, PAGE_READWRITE);
	if (m_pBoostBuffer[0] == NULL)
		return BoostReceiveUnbuffered();
	m_pBoostBuffer[1] = (BYTE *)XPhysicalAlloc(kBoostBufferSize, MAXULONG_PTR, 0, PAGE_READWRITE);
	if (m_pBoostBuffer[1] == NULL)
	{
		XPhysicalFree(m_pBoostBuffer[0]);
		return BoostReceiveUnbuffered();
	}
	memset(m_pBoostBuffer[0], 0, kBoostBufferSize);
	memset(m_pBoostBuffer[1], 0, kBoostBufferSize);

	std::string text;
	m_bPassive = TRUE;
	DWORD port = ChooseBoostPort(this);
	CloseDataConnection();   // which clears m_bPassive again

	BOOL accepted = FALSE;
	BOOL listening;
	SOCKET listener = BoostListen(&port, &listening);
	if (!listening)
	{
		Reply(425, "Can't open data connection. Boost Mode terminated.");
		return hr;
	}
	unsigned long nonBlocking = 1;
	ioctlsocket(listener, FIONBIO, &nonBlocking);
	if (listen(listener, 0x7FFFFFFF) == 0)
	{
		StrFormat(text, "- %u %d", m_files.size(), (WORD)port);
		Reply(226, text.c_str());
		int bufferSize = 0x10000;
		setsockopt(listener, SOL_SOCKET, SO_SNDBUF, (char *)&bufferSize, 4);
		bufferSize = 0x20000;
		setsockopt(listener, SOL_SOCKET, SO_RCVBUF, (char *)&bufferSize, 4);
		DWORD deadline = GetTickCount() + 15000;
		m_dataSocket = INVALID_SOCKET;
		int addrLen = sizeof(sockaddr_in);
		do
		{
			Sleep(10);
			m_dataSocket = accept(listener, NULL, (socklen_t *)&addrLen);
		} while (m_dataSocket == INVALID_SOCKET && GetTickCount() <= deadline);
		accepted = m_dataSocket != INVALID_SOCKET;
		if (accepted)
		{
			unsigned long blocking = 0;
			ioctlsocket(m_dataSocket, FIONBIO, &blocking);
			int timeout = 5000;
			setsockopt(m_dataSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, 4);
		}
	}
	shutdown(listener, SD_BOTH);
	closesocket(listener);
	if (!accepted)
	{
		Reply(425, "Can't open data connection. Boost Mode terminated.");
		return hr;
	}

	DWORD filled = 0;
	int current = 0;
	m_nBoostBuffer = 0;
	m_bBoostReceiveDone = FALSE;
	m_hBoostEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
	ResetEvent(m_hBoostEvent);
	HANDLE writer = (HANDLE)_beginthreadex(NULL, 0, BoostWriterThread, this, 0, NULL);
	if (writer != INVALID_HANDLE_VALUE)
		SetThreadPriority(writer, GetThreadPriority(GetCurrentThread()));

	BYTE *buffer = m_pBoostBuffer[0];
	DWORD exitCode = 0;
	DWORD received = 0;
	while (!m_bAbort && m_dwBoostTotalBytes != received)
	{
		int n = recv(m_dataSocket, (char *)buffer + filled, kBoostBufferSize - filled, 0);
		if (n <= -1)
			break;
		filled += n;
		received += n;
		if (filled == kBoostBufferSize)
		{
			// hand the full buffer over and wait until the writer is off the
			// other one
			current ^= 1;
			while (m_nBoostBuffer == current)
			{
				if (WaitForSingleObject(writer, 5) == WAIT_OBJECT_0)
					goto writerGone;
				SwitchToThread();
			}
			SetEvent(m_hBoostEvent);
			buffer = m_pBoostBuffer[current];
			filled = 0;
			memset(buffer, 0, kBoostBufferSize);
		}
	}

writerGone:
	CloseDataConnection();
	if (filled > 0)
	{
		while (m_nBoostBuffer != current)
		{
			if (WaitForSingleObject(writer, 5) == WAIT_OBJECT_0)
				break;
			SwitchToThread();
		}
	}
	m_bBoostReceiveDone = TRUE;
	SetEvent(m_hBoostEvent);
	WaitForSingleObject(writer, 5000);
	do
	{
		exitCode = STILL_ACTIVE;
		GetExitCodeThread(writer, &exitCode);
	} while (exitCode == STILL_ACTIVE);

	if ((exitCode != 0 ? m_dwBoostTotalBytes + 1 : received) == m_dwBoostTotalBytes)
	{
		Reply(226, "Ending Boost Mode. Transfer Complete.");
		hr = S_OK;
	}
	else
	{
		hr = E_FAIL;
		// the writer returns the index of the entry it stopped at
		m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, m_files.size() - exitCode);
		Reply(226, "Socket error during transfer. Boost Mode terminated.");
	}

	CloseHandle(writer);
	CloseHandle(m_hBoostEvent);
	m_hBoostEvent = NULL;
	if (m_pBoostBuffer[0] != NULL)
	{
		XPhysicalFree(m_pBoostBuffer[0]);
		m_pBoostBuffer[0] = NULL;
	}
	if (m_pBoostBuffer[1] != NULL)
	{
		XPhysicalFree(m_pBoostBuffer[1]);
		m_pBoostBuffer[1] = NULL;
	}
	return hr;
}

// 0x000B5F5A
// The writer of BoostReceive(): walks m_files and the received buffers in
// step, creating folders and writing each file's bytes. Files are opened
// with OPEN_ALWAYS and preallocated (rounded up to 16 KB). Returns 0 when
// all entries are done, else the index of the entry it stopped at
// (STILL_ACTIVE becomes STILL_ACTIVE + 1 after an ABOR so it isn't mistaken
// for a running thread).
unsigned CFTPConnection::BoostWriterThreadProc()
{
	std::string dir;
	std::string path;
	int filesDone = 0;
	int index = 0;
	DWORD consumed = 0;
	BOOL failed = FALSE;

	ResolvePath(&dir, m_strCwd.c_str(), NULL);
	FtpPathToXbox(&dir, dir.c_str());
	StrAddBackslash(dir);

	HANDLE file = NULL;
	m_nBoostBuffer = 0;
	int lastBuffer = 1;
	DWORD position = 0;
	int count = (int)m_files.size();
	DWORD remaining = 0;
	int waits = 0;

	if (m_dwBoostTotalBytes != 0)
	{
		for (;;)
		{
			if (m_bAbort)
				return index == STILL_ACTIVE ? STILL_ACTIVE + 1 : index;

			if (lastBuffer != m_nBoostBuffer)
			{
				if (WaitForSingleObject(m_hBoostEvent, 2000) != WAIT_OBJECT_0)
				{
					if (waits++ == 5)
						break;
					SwitchToThread();
					if (m_dwBoostTotalBytes == consumed)
						break;
					continue;
				}
				ResetEvent(m_hBoostEvent);
				lastBuffer = m_nBoostBuffer;
				position = 0;
			}

			BYTE *buffer = m_pBoostBuffer[m_nBoostBuffer];
			BOOL bufferUsedUp = FALSE;   // ran out of data in the middle of a file
			while (position != kBoostBufferSize)
			{
				if (m_bAbort)
					return index == STILL_ACTIVE ? STILL_ACTIVE + 1 : index;

				if (file == NULL)
				{
					if (index == count)
						break;
					SFtpFileEntry &entry = m_files[index++];
					if (entry.bDirectory)
					{
						dir = entry.strName;
						StrAddBackslash(dir);
						CreateDirectoryTree(dir.c_str());
						filesDone++;
						continue;
					}

					StrFormat(path, "%s%s", dir.c_str(), entry.strName.c_str());
					if (m_pSettings->m_prefs.m_bAutoRename)
						MakeFatxPath(path.c_str(), path);
					file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					if (file == NULL || file == INVALID_HANDLE_VALUE)
					{
						// gives up; consumed can't reach the total any more
						consumed = m_dwBoostTotalBytes + 1;
						index--;
						break;
					}
					remaining = entry.dwSize;
					if (entry.dwSize == 0)
					{
						SetFilePointer(file, 0, NULL, FILE_BEGIN);
						SetEndOfFile(file);
						CloseHandle(file);
						file = NULL;
						continue;
					}
					if (entry.dwSize > 0x4000)
					{
						if (SetFilePointer(file, (entry.dwSize + 0x3FFF) & ~0x3FFF, NULL, FILE_BEGIN) != 0)
							SetEndOfFile(file);
						SetFilePointer(file, 0, NULL, FILE_BEGIN);
					}
					// (the original has a block here for skipping bytes that
					// is never reached)
				}

				DWORD chunk = kBoostBufferSize - position;
				if (remaining < chunk)
					chunk = remaining;
				remaining -= chunk;
				if (chunk != 0)
				{
					DWORD written = 0;
					if (!WriteFile(file, buffer + position, chunk, &written, NULL) || written != chunk)
					{
						SetEndOfFile(file);
						CloseHandle(file);
						file = NULL;
						failed = TRUE;
						consumed = m_dwBoostTotalBytes + 1;
						index--;
						break;
					}
					consumed += written;
					position += chunk;
					if (remaining != 0)
						continue;
				}
				else if (remaining > 0)
				{
					bufferUsedUp = TRUE;
					break;
				}

				// the file is complete
				m_pServer->AddStat(FTPSTAT_FILES_RECEIVED, 1);
				SetEndOfFile(file);
				CloseHandle(file);
				file = NULL;
			}

			if (!bufferUsedUp)
			{
				if (position != 0)
					m_pServer->AddStat(FTPSTAT_BYTES_RECEIVED, position);
				if (position != kBoostBufferSize)
					break;   // all entries done, or an error
			}

			m_nBoostBuffer ^= 1;
			if (m_bBoostReceiveDone)
				break;
			if (m_dwBoostTotalBytes == consumed)
				break;
		}

		if (file != NULL)
		{
			SetEndOfFile(file);
			CloseHandle(file);
		}
	}

	if (!failed)
	{
		// the folders and empty files after the last data
		while (index < count)
		{
			SFtpFileEntry &entry = m_files[index++];
			if (entry.bDirectory)
			{
				dir = entry.strName;
				StrAddBackslash(dir);
				CreateDirectoryTree(dir.c_str());
				continue;
			}
			if (entry.dwSize != 0)
				break;
			StrFormat(path, "%s%s", dir.c_str(), entry.strName.c_str());
			if (m_pSettings->m_prefs.m_bAutoRename)
				MakeFatxPath(path.c_str(), path);
			HANDLE empty = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (empty == NULL || empty == INVALID_HANDLE_VALUE)
			{
				index--;
				break;
			}
			CloseHandle(empty);
		}
	}

	if (index != count)
		return index;
	// Quirk: bytes that never arrived are counted as received.
	if (m_dwBoostTotalBytes > consumed)
		m_pServer->AddStat(FTPSTAT_BYTES_RECEIVED, m_dwBoostTotalBytes - consumed);
	return 0;
}

// 0x000B4A6B
// Boost mode without the big buffers: one file at a time through the 16 KB
// transfer buffer, resuming files that are already partly there. Quirks:
// resuming writes the wrong part of the block that straddles the old end
// and then never matches the written count (an endless loop of writes); a
// failed write repeats the loop without receiving; it busy-waits for the
// connection; and it returns E_FAIL even when everything arrived.
HRESULT CFTPConnection::BoostReceiveUnbuffered()
{
	if (m_files.size() == 0)
		return E_FAIL;

	HRESULT hr = E_FAIL;
	if (m_clientIp[0] == 0)
	{
		Reply(425, "Can't open data connection. Boost Mode terminated.");
		return hr;
	}

	std::string text;
	m_bPassive = TRUE;
	DWORD port = ChooseBoostPort(this);
	CloseDataConnection();

	BOOL accepted = FALSE;
	BOOL listening;
	SOCKET listener = BoostListen(&port, &listening);
	if (!listening)
	{
		Reply(425, "Can't open data connection. Boost Mode terminated.");
		return hr;
	}
	unsigned long nonBlocking = 1;
	ioctlsocket(listener, FIONBIO, &nonBlocking);
	if (listen(listener, 0x7FFFFFFF) == 0)
	{
		DWORD deadline = GetTickCount() + 15000;
		m_dataSocket = INVALID_SOCKET;
		StrFormat(text, "- %u %d", m_files.size(), (WORD)port);
		Reply(226, text.c_str());
		Sleep(5);
		int addrLen = sizeof(sockaddr_in);
		while (m_dataSocket == INVALID_SOCKET)
		{
			m_dataSocket = accept(listener, NULL, (socklen_t *)&addrLen);
			if (GetTickCount() > deadline)
				break;
		}
		accepted = m_dataSocket != INVALID_SOCKET;
		if (accepted)
		{
			unsigned long blocking = 0;
			ioctlsocket(m_dataSocket, FIONBIO, &blocking);
			int timeout = 15000;
			setsockopt(m_dataSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, 4);
		}
	}
	shutdown(listener, SD_BOTH);
	closesocket(listener);
	if (!accepted)
	{
		Reply(425, "Can't open data connection. Boost Mode terminated.");
		return hr;
	}

	BYTE *buffer = m_pTransferBuffer;
	DWORD bufferSize = 0x4000;
	BYTE stackBuffer[0x1000];
	if (buffer == NULL)
	{
		buffer = stackBuffer;
		bufferSize = sizeof(stackBuffer);
	}

	std::string dir;
	std::string path;
	ResolvePath(&dir, m_strCwd.c_str(), NULL);
	FtpPathToXbox(&dir, dir.c_str());
	if (StrRight(dir, 1) != "\\")
		dir.append("\\");

	int filesDone = 0;
	int index = 0;
	for (std::vector<SFtpFileEntry>::iterator it = m_files.begin(); it != m_files.end(); ++it, ++index)
	{
		if (m_bAbort)
			break;

		BOOL aborted = FALSE;
		if (it->bDirectory)
		{
			dir = it->strName;
			if (StrRight(dir, 1) != "\\")
				dir.append("\\");
			CreateDirectoryTree(dir.c_str());
			filesDone++;
			continue;
		}

		DWORD size = it->dwSize;
		DWORD existing = 0;
		StrFormat(path, "%s%s", dir.c_str(), it->strName.c_str());
		if (m_pSettings->m_prefs.m_bAutoRename)
			MakeFatxPath(path.c_str(), path);
		HANDLE file = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
		                          FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (file == NULL || file == INVALID_HANDLE_VALUE)
		{
			Sleep(1);
			continue;
		}
		if (size == 0)
		{
			SetEndOfFile(file);
			CloseHandle(file);
			filesDone++;
			m_pServer->AddStat(FTPSTAT_FILES_RECEIVED, 1);
			continue;
		}

		BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(file, &info))
			existing = info.nFileSizeLow;
		SetFilePointer(file, (size + 0x3FFF) & ~0x3FFF, NULL, FILE_BEGIN);
		if (GetLastError() == 0)
			SetEndOfFile(file);
		if (existing > 0 && existing <= size)
		{
			SetFilePointer(file, existing, NULL, FILE_BEGIN);
		}
		else
		{
			SetFilePointer(file, 0, NULL, FILE_BEGIN);
			existing = 0;
		}

		DWORD deadline = GetTickCount() + 15000;
		DWORD chunk = size < bufferSize ? size : bufferSize;
		int n;
		while ((n = recv(m_dataSocket, (char *)buffer, chunk, 0)) < 1)
		{
			if (GetTickCount() >= deadline)
				break;
			Sleep(1);
		}

		DWORD received = 0;
		while (n != -1 && !m_bAbort)
		{
			received += n;
			DWORD written = 0;
			DWORD done = 0;
			BOOL ok;
			if ((existing > 0 && received <= existing) || n == 0)
			{
				ok = TRUE;   // already on disk (or nothing came)
			}
			else
			{
				do
				{
					DWORD skip = 0;
					if (existing > 0 && received > existing)
					{
						skip = received - existing;
						existing = 0;
					}
					ok = WriteFile(file, buffer + skip, n - skip, &written, NULL);
					if (!ok)
						Sleep(1);
					done += written;
				} while (done != (DWORD)n);
			}

			if (size == received)
				break;
			if (ok)
			{
				DWORD want = size - received;
				n = recv(m_dataSocket, (char *)buffer, want < bufferSize ? want : bufferSize, 0);
				if (n == 0)
					break;
				if (n == -1)
				{
					int error = WSAGetLastError();
					if (error == WSAECONNRESET || error == WSAENOTCONN)
					{
						Sleep(1);
						aborted = TRUE;
						break;
					}
					n = 0;
				}
			}
			if (size <= received)
				break;
		}

		SetEndOfFile(file);
		CloseHandle(file);
		if (aborted)
		{
			hr = E_FAIL;
			m_pServer->AddStat(FTPSTAT_FAILED_RECEIVES, 1);
			break;
		}
		filesDone++;
		m_pServer->AddStat(FTPSTAT_FILES_RECEIVED, 1);
		if (size != 0)
			m_pServer->AddStat(FTPSTAT_BYTES_RECEIVED, size);
	}

	CloseDataConnection();
	Sleep(500);
	if (filesDone != (int)m_files.size() && index != (int)m_files.size())
		Reply(451, "Socket error during transfer. Boost Mode terminated.");
	else
		Reply(226, "Ending Boost Mode. Transfer Complete.");
	return hr;
}
