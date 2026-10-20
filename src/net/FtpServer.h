// The built-in FTP server (CFTPServer) and its control connections
// (CFTPConnection). One global server (g_ftpServer) listens on the FTP port;
// every client gets a CFTPConnection with its own thread that reads commands
// and runs them. Data connections are opened per transfer (PORT or PASV).
//
// Besides standard FTP it speaks the Avalaunch "boost mode" (SITE BOOST),
// which streams a whole folder tree over one data connection, answers the
// "ping" broadcast other dashboards use to find FTP servers on the LAN, and
// passes SITE commands it doesn't handle itself to the dashboard (see
// src/app/FtpEvents.cpp). docs/FTP.md has the user-visible behaviour.
//
// FTP paths look like "/C/Apps/XBMC"; the first component is the drive
// letter. The root "/" lists the drives.
#pragma once

#include <list>
#include <string>
#include <vector>

#include "../app/XBApp.h"
#include "../common/Platform.h"
#include "XNet.h"

class CAppSettings;
class CFTPServer;

extern BYTE g_bFtpEnabled;            // 0x0026A62E the dashboard wants the server running

// Statistics ids passed to CFTPEventSink::OnStatChanged and
// CFTPServer::AddStat.
enum FtpStat
{
	FTPSTAT_TOTAL_CONNECTIONS = 0,
	FTPSTAT_CURRENT_USERS = 1,
	FTPSTAT_BYTES_SENT = 2,
	FTPSTAT_BYTES_RECEIVED = 3,
	FTPSTAT_FILES_SENT = 4,
	FTPSTAT_FILES_RECEIVED = 5,
	FTPSTAT_FAILED_SENDS = 6,
	FTPSTAT_FAILED_RECEIVES = 7,
};

// Callbacks from the server to the dashboard. The base class does nothing;
// CFTPEventReceiver (src/app/FtpEvents.cpp) is the one UnleashX installs.
// vtable 0x002545B4.
class CFTPEventSink
{
public:
	virtual ~CFTPEventSink() {}
	virtual void OnLogin(const char * /*user*/, const char * /*ip*/) {}      // 0x0002B8D7
	virtual void OnLogout(const char * /*user*/) {}                          // 0x0002B8DA
	virtual void OnBytesReceived(DWORD /*total*/) {}                         // 0x0002B8DA
	virtual void OnBytesSent(DWORD /*total*/) {}                             // 0x0002B8DA
	virtual void OnStatChanged(int /*stat*/, DWORD /*value*/) {}             // 0x0002B8D7
	// SITE <command> the server doesn't handle. Returns the reply code (0 =
	// not implemented) and the reply text in reply.
	virtual int OnSiteCommand(const char * /*command*/, std::string * /*reply*/) { return 0; }   // 0x000326FF
	// EXEC <name> [args]. Returns the reply code, 0 = not implemented.
	virtual int OnExec(const char * /*name*/, const char * /*args*/) { return 0; }   // 0x000326FF
};

// One entry of a boost-mode file list or of SITE MAKE_FILE_TREE. 0x2C bytes.
struct SFtpFileEntry
{
	BOOL bDirectory;          // +0x00
	std::string strName;      // +0x04 directories: full Xbox path; files: name only
	DWORD dwSize;             // +0x20
	DWORD dwAttributes;       // +0x24
	DWORD dwCreationTime;     // +0x28 low part of the FILETIME
};

// A control connection. 0x544 bytes, no vtable. Reference counted: the
// server's list holds one reference.
class CFTPConnection
{
public:
	CFTPConnection(SOCKET socket, CFTPServer *server);       // 0x000B7FD0
	~CFTPConnection();                                       // 0x000B3CF5

	LONG AddRef();                                           // 0x000B39AF
	LONG Release();                                          // 0x000B3DD4 deletes at 0
	BOOL StartThread();                                      // 0x000BAD81 created suspended
	// The peer address from getpeername().
	void SetClientAddress(const sockaddr_in *addr);          // 0x000B3AF1

	// Sends "<code> <text>\r\n" (code < 11: just the text). text is a
	// printf format for the remaining arguments.
	void Reply(int code, const char *fmt, ...);              // 0x000B45B1
	// 502, with "Command not implemented." if text is empty.
	void ReplyNotImplemented(const char *text);              // 0x000B489E

	// cwd + arg as a normalized FTP path ("/C/dir/file"): ':' removed, '\'
	// turned into '/', no trailing '/'. arg may be absolute.
	void ResolvePath(std::string *out, const char *cwd, const char *arg);   // 0x000B4046
	// "/C/dir/file" -> "C:\dir\file".
	static void FtpPathToXbox(std::string *out, const char *ftpPath);       // 0x000B4419

	unsigned ThreadProc();                                   // 0x000BAABB
	HRESULT Login();                                         // 0x000B8166
	HRESULT ReadCommand(std::string *command, std::string *arg);            // 0x000B4769
	HRESULT Dispatch(std::string command, std::string arg);  // 0x000BA451
	HRESULT Close();                                         // 0x000B46F1

	void SendWelcome();                                      // 0x000B4729
	// The Xbox path of a directory argument; returns whether it exists.
	BOOL ResolveDirPath(std::string *out, const char *arg);                 // 0x000B6848
	BOOL ResolveFilePath(std::string *out, const char *arg, BOOL mustExist);   // 0x000B6929

	// Commands (FtpConnection.cpp); the argument is passed by value.
	void CmdAbor(std::string arg);                           // 0x000B5526
	void CmdQuit(std::string arg);                           // 0x000B5843 also BYE
	void CmdCdup(std::string arg);                           // 0x000B5896 also XCUP
	void CmdCwd(std::string arg);                            // 0x000B69EE also XCWD
	void CmdDele(std::string arg);                           // 0x000B53F5
	void CmdExec(std::string arg);                           // 0x000B5A20
	void CmdMkd(std::string arg);                            // 0x000B51C7 also XMKD
	void CmdNoop(std::string arg);                           // 0x000B59FA
	// SITE TITLEIMAGE. (this in eax)
	void SiteTitleImage();                                   // 0x000B496C
	// Resumes the connection's thread (unless it has none). (this in eax)
	void Resume() { if (m_hThread != (HANDLE)-1) ResumeThread(m_hThread); }   // 0x000B3ADF
	void CmdPwd(std::string arg);                            // 0x000B5C09 also XPWD
	void CmdRest(std::string arg);                           // 0x000B5720
	void CmdRmd(std::string arg);                            // 0x000B52D5 also XRMD
	void CmdRnfr(std::string arg);                           // 0x000B564B
	void CmdRnto(std::string arg);                           // 0x000B5550
	void CmdSize(std::string arg);                           // 0x000B6DB4
	void CmdSyst(std::string arg);                           // 0x000B4A14
	void CmdRsyst(std::string arg);                          // 0x000B49EE
	void CmdSprt(std::string arg);                           // 0x000B4995
	void CmdType(std::string arg);                           // 0x000B57D4
	void CmdSite(std::string arg);                           // 0x000B9D35
	void SiteTitles(std::string arg);                        // 0x000B95F9
	void SaveTitleImage(const char *folder, DWORD titleId);  // part of SiteTitles
	void BuildFileTree(std::string root);                    // 0x000B4204

	// Transfers (FtpTransfer.cpp)
	void CloseDataConnection();                              // 0x0002F934
	BOOL OpenDataConnection(BOOL replyOnError);              // 0x000B6611
	BOOL OpenPassiveConnection();                            // 0x000B5CA5
	void SetTransferActive(BOOL active);                     // 0x000B3C00
	void CmdPasv(std::string arg);                           // 0x000B6CAE also PASW
	void CmdPort(std::string arg);                           // 0x000B6E7E
	std::string ListDirectory(BOOL namesOnly, const char *arg);   // 0x000B86E3
	HRESULT SendListing(std::string listing);                // 0x000B6F64
	void CmdList(std::string arg);                           // 0x000B91E7
	void CmdNlst(std::string arg);                           // 0x000B918A
	HRESULT SendFile(const char *path, DWORD offset);        // 0x000B7042
	void CmdRetr(std::string arg);                           // 0x000B94EA
	HRESULT ReceiveFile(std::string path, BOOL resume);      // 0x000B737D
	void CmdAppe(std::string arg);                           // 0x000B924F
	HRESULT ReceiveFileBuffered(std::string path, BOOL resume);   // 0x000B7C77
	void CmdStor(std::string arg);                           // 0x000B93A1
	void SiteBoost(std::string arg);                         // 0x000B8B49
	HRESULT BoostReceive();                                  // 0x000B773D
	unsigned BoostWriterThreadProc();                        // 0x000B5F5A
	HRESULT BoostReceiveUnbuffered();                        // 0x000B4A6B

	BYTE *m_pBoostBuffer[2];          // +0x000 two 1 MB buffers in boost mode
	int m_nBoostBuffer;               // +0x008 the buffer the writer thread works on
	BOOL m_bBoostReceiveDone;         // +0x00C
	HANDLE m_hBoostEvent;             // +0x010 set when the receiver filled a buffer
	DWORD m_unknown014;               // +0x014
	BOOL m_bAbort;                    // +0x018 ABOR
	CFTPServer *m_pServer;            // +0x01C
	std::string m_strCwd;             // +0x020 FTP path
	std::string m_strRenameFrom;      // +0x03C RNFR, as an Xbox path
	char m_recvBuffer[0x400];         // +0x058
	BYTE *m_pTransferBuffer;          // +0x458 0x4000 bytes
	BYTE m_clientIp[4];               // +0x45C
	BYTE m_clientPort[2];             // +0x460 high, low byte
	BYTE m_localIp[4];                // +0x462
	BYTE m_localDataPort[2];          // +0x466 0, server port - 1; unused
	HANDLE m_hThread;                 // +0x468
	LONG m_nRefCount;                 // +0x46C
	std::string m_strPassword;        // +0x470
	std::string m_strUser;            // +0x48C
	std::string m_strHome;            // +0x4A8 anonymous root; unused
	WORD m_wDataPort;                 // +0x4C4 from PORT
	BYTE m_bConnected;                // +0x4C6
	BYTE m_bPassive;                  // +0x4C7
	BYTE m_bCanDelete;                // +0x4C8 DELE and RMD; FALSE for anonymous users
	BYTE m_bAnonymous;                // +0x4C9 read-only
	WORD m_wPasvPort;                 // +0x4CA
	DWORD m_dwRestOffset;             // +0x4CC REST
	SOCKET m_ctrlSocket;              // +0x4D0
	SOCKET m_dataSocket;              // +0x4D4
	BYTE m_unknown4D8;                // +0x4D8 1
	BYTE m_bPriorityRaised;           // +0x4D9
	std::vector<SFtpFileEntry> m_files;   // +0x4DC boost-mode list / file tree
	DWORD m_unknown4EC[4];            // +0x4EC
	BOOL m_bBoostMode;                // +0x4FC a SITE BOOST was accepted
	int m_nActiveTransfers;           // +0x500
	std::string m_strUnknown504;      // +0x504
	std::string m_strTitleImageDir;   // +0x520 SITE TITLES writes title images here
	DWORD m_dwBoostTotalBytes;        // +0x53C sum of the boost list's file sizes
	CAppSettings *m_pSettings;        // +0x540
};

// Shared by FtpConnection.cpp and FtpTransfer.cpp.
HRESULT FtpSendAll(SOCKET s, const char *data, int length);         // 0x000B3A51
HRESULT FtpRecvSome(CFTPConnection *connection, SOCKET s);          // 0x000B39F3 into m_recvBuffer
SFtpFileEntry MakeFtpFileEntry(BOOL directory, const std::string &name, DWORD size, DWORD attributes,
                               DWORD creationTime);                  // 0x0002F8FF

// vtable 0x00258B54. The one instance is g_ftpServer.
class CFTPServer
{
public:
	CFTPServer();                                            // 0x000B64CD
	virtual ~CFTPServer();                                   // 0x000B3FB6

	HRESULT Start();                                         // 0x000BB22A
	void Stop();                                             // 0x000B3C5D
	BOOL IsRunning() const;                                  // 0x000B39A8
	unsigned ListenThreadProc();                             // 0x000BADB3

	void SetPort(int port);                                  // 0x000B3997
	void SetMaxUsers(int maxUsers);                          // 0x000B399B 1..15
	void SetWelcome(const char *text);                       // 0x000B3E13
	void SetUser(const char *user);                          // 0x000B3E55 ignored if empty
	void SetPassword(const char *password);                  // 0x000B3E20 ignored if empty
	void SetPasvPortMin(DWORD port);                         // 0x0002B8DD clamped to 0x400..0x10000
	void SetPasvPortMax(DWORD port);                         // 0x0002B8FD
	// The SYST reply; prefixed "UNIX / UX " unless it starts with "UNIX ".
	void SetSyst(const char *syst);                          // 0x0002B91D
	void SetEventSink(CFTPEventSink *sink);                  // 0x000B3907
	// Suspends the listening thread (boost mode) or resumes it.
	void Pause(BOOL pause);                                  // 0x000B390B

	BOOL TooManyUsers();                                     // 0x000B38E7
	void SetTransferActive(BOOL active);                     // 0x000B3969
	HRESULT CheckLogin(const char *user, const char *password, std::string *home);   // 0x000B3E8A

	BOOL AddConnection(CFTPConnection *connection);          // 0x0002F9A7
	// Removes the connection whose thread is thread; releases it unless keep.
	CFTPConnection *RemoveConnection(HANDLE thread, BOOL keep);   // 0x0002F9CC
	// A connection thread ending: RemoveConnection(thread, FALSE).
	void RemoveConnectionAndRelease(HANDLE thread) { RemoveConnection(thread, FALSE); }   // 0x000B45A0

	void AddStat(int stat, DWORD amount);                    // 0x0002FA15
	void NotifyLogin(const char *user, const char *ip);      // 0x0002FAAC
	void NotifyLogout(const char *user);                     // 0x0002FAE1
	int NotifySiteCommand(const char *command, std::string *reply);   // 0x0002FB2C
	int NotifyExec(const char *name, const char *args);      // 0x0002FB58

	// +0x004 (unused)
	std::list<CFTPConnection *> m_connections;   // +0x008
	SOCKET m_listenSocket;                       // +0x030
	CFTPEventSink *m_pEvents;                    // +0x034
	CXBStopWatch m_activityTimer;                // +0x038 restarted on every transfer
	int m_nPort;                                 // +0x058 21
	int m_nMaxUsers;                             // +0x05C 2
	std::string m_strWelcome;                    // +0x060 the 230 reply after login
	std::string m_strGoodbye;                    // +0x07C "Bye", the QUIT reply
	int m_unknown098;                            // +0x098
	BOOL m_bRunning;                             // +0x09C
	std::string m_strUser;                       // +0x0A0 "Xbox"
	std::string m_strPassword;                   // +0x0BC "xbox"
	DWORD m_dwBytesReceived;                     // +0x0D8
	DWORD m_dwBytesSent;                         // +0x0DC
	int m_nCurrentUsers;                         // +0x0E0
	DWORD m_dwTotalConnections;                  // +0x0E4
	DWORD m_dwFilesSent;                         // +0x0E8
	DWORD m_dwFilesReceived;                     // +0x0EC
	DWORD m_dwFailedSends;                       // +0x0F0
	DWORD m_dwFailedReceives;                    // +0x0F4
	DWORD m_dwPasvPortMin;                       // +0x0F8 0x400
	DWORD m_dwPasvPortMax;                       // +0x0FC 0xFFFF
	std::string m_strSyst;                       // +0x100
	BOOL m_bBoost;                               // +0x11C boost mode allowed (<FTP><Boost>)
	int m_nActiveTransfers;                      // +0x120
	BOOL m_bPaused;                              // +0x124
	HANDLE m_hPauseEvent;                        // +0x128
	HANDLE m_hStopEvent;                         // +0x12C
	HANDLE m_hThread;                            // +0x130
	CAppSettings *m_pSettings;                   // +0x134
	BYTE m_bStopping;                            // +0x138
	BYTE m_bAllowAnonymous;                      // +0x139
	std::string m_strAnonymousHome;              // +0x13C
};

extern CFTPServer g_ftpServer;                   // 0x002784F8

// The original passes the server in a register; callers use these.
inline BOOL IsFtpServerRunning()
{
	return g_ftpServer.IsRunning();
}
inline void StopFtpServer()
{
	g_ftpServer.Stop();
}
inline HRESULT StartFtpServer()
{
	return g_ftpServer.Start();
}
