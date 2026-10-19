// The FTP client: CDirFTP, the CDirectory of an FTP site in the file
// manager. It keeps one control connection and opens a data connection per
// LIST/RETR/STOR, passive (PASV) first and active (PORT) when the server
// refuses. Uploads and downloads of whole folders, deleting folder trees
// and the Avalaunch boost upload sit on top. Everything is logged to a
// CFTPLog (the file manager's FTP log window). See docs/FTP.md.
//
// Paths are FTP paths ("/C/Apps/"); names without a leading '/' are taken
// relative to the current directory.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "../system/FileIO.h"
#include "XNet.h"

class CSocket;
struct SFileListItem;

// The FTP log window (src/filemgr/FtpLog.h).
class CFTPLog;

// The parsed form of one LIST line (D. J. Bernstein's ftpparse, linked in;
// see src/thirdparty/FtpParse.h for the function).
struct ftpparse;

// An FTP site. vtable 0x0025BFAC, 0x58C bytes; it lives inside
// CFTPFileFolder (+0x3D8).
class CDirFTP : public CDirectory
{
public:
	// (this in esi, log in eax)
	explicit CDirFTP(CFTPLog *log);                                              // 0x000828A2
	virtual ~CDirFTP();                                                          // 0x0008241D (deleting 0x0003222A)

	// The listing is fetched once per directory: the handle is always 1 and
	// only one search runs at a time.
	virtual BOOL FindClose(HANDLE find);                                         // 0x0003221D
	virtual HANDLE FindFirst(const char *pattern, WIN32_FIND_DATAA *data);       // 0x00084DC4
	virtual BOOL FindNext(HANDLE find, WIN32_FIND_DATAA *data);                  // 0x00082731
	// The whole tree: files with DELE, then folders deepest first with RMD.
	virtual BOOL RemoveDirectory(const char *path);                              // 0x00084B9A
	virtual BOOL CopyFile(const char *source, const char *destination, BOOL failIfExists);   // 0x00082346
	// Uploads when source is an Xbox path ("C:\..."; flags & COPY_FTP_FOLDER:
	// a whole folder), downloads otherwise (a source ending in '/' is a
	// folder). flags == COPY_FILE_FAIL_IF_EXISTS keeps existing local files.
	virtual BOOL CopyFileEx(const char *source, const char *destination, CopyProgressRoutine progress,
	                        void *data, BOOL *cancel, DWORD flags);              // 0x000870E7
	virtual BOOL CreateDirectory(const char *path, void *security);              // 0x000832BC
	virtual BOOL DeleteFile(const char *path);                                   // 0x0008319E
	// RNFR + RNTO.
	virtual BOOL MoveFile(const char *source, const char *destination);          // 0x00082F62
	virtual int GetDriveType() { return 3; }                                     // 0x000129FD

	// Connects and logs in (empty user: anonymous). Then SYST and a REST
	// probe. TRUE when logged in.
	BOOL Connect(const char *host, int port, const char *user, const char *password);   // 0x00083830
	// QUIT (without waiting for the reply), then Reset(). (this in eax)
	void Disconnect();                                                           // 0x00083055
	// CWD to path ("CDUP" goes up; NULL or "" re-enters the current one),
	// then PWD for the new current directory.
	BOOL ChangeDir(const char *path);                                            // 0x000833EC
	// The active-mode port range. (port in eax, this in ecx)
	unsigned SetMinPort(unsigned port);                                          // 0x000822EB
	// (port in edx, this in eax)
	void SetMaxPort(unsigned port);                                              // 0x0008230D
	// SIZE path answered 213. Unused.
	BOOL RemoteFileExists(const char *path);                                     // 0x00083083
	// ChangeDir(path) and back. Unused. (this in edi)
	BOOL CheckDir(const char *path);                                             // 0x00084D64

	CFTPLog *m_pLog;                    // +0x004
	CSocket *m_pControl;                // +0x008
	BOOL m_bConnected;                  // +0x00C logged in
	std::string m_strError;             // +0x010 the last error
	char m_szReply[0x400];              // +0x02C the last reply line
	SOCKET m_dataSocket;                // +0x42C -1 = none
	SOCKET m_listenSocket;              // +0x430 active mode
	BYTE m_pasvAddress[4];              // +0x434 from the last PASV reply
	BYTE m_pasvPort[2];                 // +0x438 (high byte first)
	BYTE m_portAddress[4];              // +0x43A sent with the first PORT
	BYTE m_portPort[2];                 // +0x43E
	BOOL m_bBinary;                     // +0x440 TYPE I (else TYPE A)
	unsigned m_nMinPort;                // +0x444 1024
	unsigned m_nMaxPort;                // +0x448 0xFFFF
	unsigned m_nPort;                   // +0x44C the last PORT port
	BOOL m_bBusy;                       // +0x450 the control connection is in use
	BOOL m_bAbort;                      // +0x454 (never set)
	int m_nFindState;                   // +0x458 -1 = no search
	std::string m_strCurrentDir;        // +0x45C ends in '/'
	std::string m_strListing;           // +0x478 the current directory's LIST
	std::string m_strListLeft;          // +0x494 the lines FindNext hasn't read
	BOOL m_bReplyPending;               // +0x4B0 a command was sent and its reply not read
	DWORD m_unknown4B4[2];              // +0x4B4 (zeroed)
	DWORD m_dwStartTick;                // +0x4BC of the last transfer
	DWORD m_dwEndTick;                  // +0x4C0
	BOOL m_bUnleashXServer;             // +0x4C4 SYST says UnleashX: SITE FREESPACE works, folders need no MKD
	BOOL m_bUnixL8;                     // +0x4C8 SYST is "UNIX Type: L8"
	int m_nFilesUploaded;               // +0x4CC
	int m_nFilesDownloaded;             // +0x4D0
	DWORD m_dwUploadMs;                 // +0x4D4 time spent uploading
	DWORD m_dwDownloadMs;               // +0x4D8
	DWORD m_unknown4DC;                 // +0x4DC
	ULONGLONG m_ullBytesUploaded;       // +0x4E0
	ULONGLONG m_ullBytesDownloaded;     // +0x4E8
	std::string m_strUnknown4F0;        // +0x4F0
	int m_nControlPort;                 // +0x50C 21
	std::string m_strUser;              // +0x510 "xbox"
	std::string m_strPassword;          // +0x52C "xbox"
	BOOL m_bPassive;                    // +0x548 1; cleared when PASV fails
	std::string m_strLastCommand;       // +0x54C
	BOOL m_bBoost;                      // +0x568 use the boost upload (CFTPFileFolder sets it before each transfer)
	std::string m_strSystem;            // +0x56C the SYST reply
	BOOL m_bResume;                     // +0x588 REST works (not used)

	// Clears the transfer statistics. (this in eax)
	void ResetStats();                                                           // 0x0001E55E
	// Closes the sockets and forgets the session. (this in ebx)
	void Reset();                                                                // 0x00082381
	// The reply code of reply (0 if it has none). Logs it, keeps it as the
	// error when it's above 400, and leaves reply with the text after the
	// code. (reply in ebx, this on the stack)
	int ParseReply(std::string &reply);                                          // 0x000824AE
	// Reads a reply, skipping continuation lines (logged). NULL without a
	// connection. (this in ecx)
	const char *ReadReply();                                                     // 0x0008259A
	// Sends "command\r\n" on the control connection. Returns its length or
	// -1. closeData: close the data connection first. (this in esi)
	int SendCommand(const char *command, BOOL closeData, BOOL log);              // 0x00082E17

private:
	// Sends all of size bytes, waiting while the socket would block (until
	// cancelled). (data in ecx)
	HRESULT SendAll(SOCKET socket, const char *data, int size);                  // 0x000829D9
	// ReadReply() and ParseReply() of what came back.
	int ReadReplyCode(std::string &reply);
	// TYPE I or TYPE A when it changes. (binary in ecx, this in eax)
	void SetBinary(BOOL binary);                                                 // 0x0008412E
	// Active mode: listens on the next port. (this in esi)
	BOOL Listen();                                                               // 0x000821BA
	// Active mode: takes the server's data connection (15 seconds). (this
	// in esi)
	BOOL AcceptData();                                                           // 0x00082103
	// PASV and connect, or PORT. (this in ecx)
	BOOL OpenData();                                                             // 0x000841DC
	// (this in eax)
	void CloseData();                                                            // 0x000321F9
	// Free bytes on the server's drive of path (SITE FREESPACE, UnleashX
	// servers only), else a huge number. (path in eax, this in ecx)
	ULONGLONG GetFreeSpace(const char *path);                                    // 0x000846B7
	// LIST of the current directory. (this in eax)
	std::string List();                                                          // 0x00084777
	// The next usable entry of m_strListLeft into data.
	BOOL ParseNextEntry(WIN32_FIND_DATAA *data);
	// Deletes the files under path, collecting the folders (deepest first).
	BOOL DeleteTree(const char *path, std::vector<std::string> *folders, int *files);   // 0x0008495D
	// STOR. (this in eax)
	BOOL UploadFile(const char *local, const char *remote);                      // 0x00085045
	// RETR. (this in eax)
	BOOL DownloadFile(const char *remote, const char *local, BOOL failIfExists); // 0x00085539
	// A local folder tree, file by file (MKD for missing folders).
	BOOL UploadFolder(const char *local, const char *remote, BOOL failIfExists); // 0x00085C65
	// A local folder tree in boost mode; the normal way if that fails.
	BOOL UploadFolderBoost(const char *local, const char *remote, BOOL failIfExists);   // 0x000863E3
	// Boost steps 2 and 3: the data connection from the 226's text, then
	// the file contents. started: a file was opened.
	BOOL SendBoostFiles(const std::vector<SFileListItem> &list, const std::string &source, const char *remote,
	                    std::string &reply, BOOL *started);
	// A remote folder tree.
	BOOL DownloadFolder(const char *remote, const char *local, BOOL failIfExists);      // 0x000860FC
	// (inlined) type is an FtpLogType.
	void Log(const char *text, int type);
};

// CopyFileEx flag: the source is a folder to upload.
#define COPY_FTP_FOLDER 0x10

// The CopyFileEx progress routine of the running FTP copy (set, never
// read). 0x002776D4
extern CopyProgressRoutine g_pFtpCopyProgress;
