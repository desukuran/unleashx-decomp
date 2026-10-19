// The FTP manager (the file manager's "Switch to FTP"): the left pane is
// the file manager's own (local drives), the right one first lists the
// FTP sites (CFTPServerList: FtpSrv.xml plus the Xboxes found on the
// network) and, once connected, the site's files (CFTPFileFolder over
// CDirFTP). Files are queued (CFTPQueue) and transferred on a thread; the
// FTP log (CFTPLog) shows the conversation. docs/FILE_MANAGER.md describes
// it.
#pragma once

#include <string>
#include <vector>

#include "../app/Screens.h"
#include "../app/SettingsBase.h"
#include "../net/FtpClient.h"
#include "../ui/MenuDialogs.h"
#include "../xml/XmlDocument.h"
#include "FileFolder.h"
#include "FtpLog.h"

class CDropDown;
class CXBFontEx;
struct IDirect3DTexture8;

// A site of the site list: a saved one (FtpSrv.xml) or an Xbox found on
// the network. 0x140 bytes.
struct SFTPSiteInfo
{
	// The empty site: port 21, passive. (this in esi)
	SFTPSiteInfo();                                              // 0x0001E5A9
	// (this in esi, port in eax)
	SFTPSiteInfo(const wchar_t *name, const char *host, int port, const char *user, const char *password,
	             BOOL passive, DWORD unknown100, BOOL saved, const char *remotePath, const char *localPath);   // 0x0001E60C
	// The copy constructor (0x0001EE35) and destructor (0x0001EED3) are the
	// compiler's.

	wchar_t name[51];                  // +0x000 what the list shows
	char host[101];                    // +0x066
	int port;                          // +0x0CC
	char user[21];                     // +0x0D0
	char password[21];                 // +0x0E5
	BOOL passive;                      // +0x0FC
	DWORD unknown100;                  // +0x100 the network reply's last field (SNetClient::unknown54)
	BOOL saved;                        // +0x104 in FtpSrv.xml (else found on the network)
	std::string remotePath;            // +0x108 where to start
	std::string localPath;             // +0x124 the local pane's folder to start in
};

// A file or folder waiting in the transfer queue. 0x48 bytes.
struct SFtpQueueItem
{
	SFtpQueueItem(std::string source, std::string target, BOOL isFolder, DWORD size, BOOL upload);   // 0x0001E4F0 (this in esi)
	// The copy constructor (0x0001EF0E) and destructor (0x0001EE01) are the
	// compiler's.

	std::string source;                // +0x00 an Xbox path for uploads, an FTP path for downloads
	std::string target;                // +0x1C
	BOOL isFolder;                     // +0x38
	DWORD size;                        // +0x3C
	BOOL upload;                       // +0x40
	DWORD unknown44;                   // +0x44 0
};

// The queue itself (one global).
extern std::vector<SFtpQueueItem> g_ftpQueue;   // 0x00277820

// The transfer queue pane, under the right pane. START/WHITE: the
// "Transfer / Remove / Clear Queue" menu. vtable 0x0024B6CC, 0x3C bytes.
class CFTPQueue
{
public:
	CFTPQueue();                                                 // 0x0011F223 (this in eax)
	virtual ~CFTPQueue();                                        // 0x0011FC6F (deleting 0x0001EDE5)
	// The font and the arrows (FileUp.png / FileDown.png). (this in ecx,
	// font in edx)
	HRESULT Init(CXBFont *font);                                 // 0x0011FB46
	HRESULT Render();                                            // 0x0011F79D (this in esi)
	// (this in eax)
	HRESULT ProcessInput(int *action);                           // 0x00122FFD
	// Adds a transfer unless the same one is queued. (this in ebx)
	BOOL Add(const char *source, const char *target, BOOL isFolder, DWORD size, BOOL upload);   // 0x00120C15
	// (this in edi; index in eax)
	BOOL Remove(unsigned index);                                 // 0x0011FC20
	// (this in edi)
	void Clear();                                                // 0x0011FC08
	void MoveDown();                                             // 0x0011F1E4 (this in eax)
	void MoveUp();                                               // 0x0011F209 (this in eax)

	float m_fX;                        // +0x04
	float m_fY;                        // +0x08
	float m_fWidth;                    // +0x0C
	float m_fHeight;                   // +0x10
	unsigned m_nSelected;              // +0x14
	unsigned m_nTop;                   // +0x18
	unsigned m_nVisible;               // +0x1C
	unsigned m_nCount;                 // +0x20
	BOOL m_bActive;                    // +0x24 has the input (drawn brighter)
	BOOL m_bMenuOpen;                  // +0x28
	CXBFont *m_pFont;                  // +0x2C
	CDropDown *m_pMenu;                // +0x30
	IDirect3DTexture8 *m_pUnknown34;   // +0x34 released, never set
	IDirect3DTexture8 *m_pUnknown38;   // +0x38
};

// The "start the queue" request: set by the queue's "Transfer" (and the
// pane menu's), taken by CFTPManager::ProcessInput. 0x002761DC
extern BOOL g_bFtpStartQueue;

// What CFTPFileFolder::LoadList does with m_strPath.
enum FtpNavigate
{
	FTPNAV_UP = 2,           // CDUP
	FTPNAV_CURRENT = 6,      // CWD m_strPath
	FTPNAV_SET = 8,          // SetPath()
};

// A pane listing an FTP site. vtable 0x0024B664, 0xA50 bytes.
class CFTPFileFolder : public IFileFolder
{
public:
	CFTPFileFolder();                                            // 0x001214C6 (this in ebx)
	virtual ~CFTPFileFolder();                                   // 22 (0x0011F6EA; deleting 0x0001EEF2)
	// Connects the first time, then lists the current folder.
	virtual void LoadList();                                     // 1 (0x00123860)
	virtual void Refresh();                                      // 2 (0x0011F5E1)
	virtual void CollectFiles(const char *path);                 // 6 (0x001229DF)
	virtual BOOL DeleteListed();                                 // 7 (0x001200AD)
	virtual BOOL RenameCurrent(const char *name);                // 8 (0x0012047A)
	virtual BOOL CreateFolder(const char *name);                 // 9 (0x0011F18F)
	// A opens folders (not files), B goes up, X marks.
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 11 (0x00122795)
	virtual HRESULT Render();                                    // 12 (0x00122738)
	virtual void SetPath(const char *path);                      // 16 (0x00120500)
	// The current folder, "/" in Root.
	virtual std::string GetCurrentPath();                        // 21 (0x0001E7AE)

	// Logs in and asks whether the server does boost mode. (this on the
	// stack)
	BOOL Connect(const char *host, int port, const char *user, const char *password);   // 0x00122E00
	// (this in edi)
	void Disconnect();                                           // 0x00122DDA
	// Sends a raw command; m_strReply gets the answer. (this on the stack)
	BOOL SendRawCommand(const char *command);                    // 0x00122CCD
	// Runs the queue on a thread. FALSE if one is running. (this in ebx)
	BOOL StartTransfer();                                        // 0x00120B55

	int m_nNavigate;                   // +0x3D0 FTPNAV_*
	DWORD m_unknown3D4;                // +0x3D4
	CDirFTP m_ftp;                     // +0x3D8
	DWORD m_unknown964;                // +0x964
	BOOL m_bConnecting;                // +0x968 shows "Connecting" while listing
	BOOL m_bConnected;                 // +0x96C
	std::string m_strHost;             // +0x970
	int m_nPort;                       // +0x98C
	std::string m_strUser;             // +0x990
	std::string m_strPassword;         // +0x9AC
	BOOL m_bPassive;                   // +0x9C8
	std::string m_strReply;            // +0x9CC the last error or reply, for the message boxes
	BOOL m_bError;                     // +0x9E8
	BOOL m_bUnknown9EC;                // +0x9EC 1
	BOOL m_bTransferDone;              // +0x9F0 no transfer running
	BOOL m_bBoostSupported;            // +0x9F4 SITE BOOST SUPPORTED answered 226
	std::string m_strUnknown9F8;       // +0x9F8
	std::string m_strUnknownA14;       // +0xA14 ""
	BOOL m_bClosing;                   // +0xA30 set by the destructor
	std::string m_strTransferResult;   // +0xA34 "Uploaded 3 files totaling ..." for the message box

private:
	static unsigned __stdcall LoadThread(void *param);          // 0x0011F1C8
	// The queue's work. (this on the stack)
	void RunQueue();                                             // 0x00120588
	static unsigned __stdcall TransferThread(void *param);      // 0x00120B3A
};

// The site list pane: the sites of <data>FtpSrv.xml, then (with
// <FTPClient DetectPeer>) the Xboxes answering the network scan. Every
// site is a folder item. vtable 0x0024B5F4, 0x508 bytes.
class CFTPServerList : public IFileFolder
{
public:
	CFTPServerList();                                            // 0x00122F45 (this in eax)
	virtual ~CFTPServerList();                                   // 22 (0x00120BD3; deleting 0x0001EE19)
	virtual void LoadList();                                     // 1 (0x0012448B)
	virtual void Refresh();                                      // 2 (0x0011F638)
	// A does nothing here (the manager connects).
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 11 (0x00122F2B)

	// The highlighted site, or an empty one. (this in ecx)
	SFTPSiteInfo GetSelectedSite();                              // 0x0011FB95
	// Saves a new site (a taken name gets "(2)", "(3)" ...). (this in ebx)
	void AddSite(const char *name, const char *host, int port, const char *user, const char *password,
	             BOOL passive, const char *remotePath, const char *localPath);   // 0x00124779
	// Saves the site called name, or adds it. (this in eax)
	void SaveSite(const char *name, const char *newName, const char *host, int port, const char *user,
	              const char *password, BOOL passive, const char *remotePath, const char *localPath);   // 0x00124928
	void DeleteSite(const char *name);                           // 0x00123ECD

	std::vector<SFTPSiteInfo> m_sites;      // +0x3D0
	CXmlDocument m_xml;                     // +0x3E0 FtpSrv.xml
	DWORD m_unknown4FC;                     // +0x4FC
	DWORD m_unknown500;                     // +0x500
	DWORD m_unknown504;                     // +0x504

private:
	static unsigned __stdcall LoadThread(void *param);          // 0x0011F1D6
	// Reads FtpSrv.xml and the network scan into the list. (this on the
	// stack)
	void ListSites();                                            // 0x00123F5B
	// Writes a <Site> element's attributes and saves. (element in eax,
	// this in edi)
	void WriteSite(CXmlElement *element, const char *host, int port, const char *user, const char *password,
	               BOOL passive, const char *remotePath, const char *localPath);   // 0x00124695
};

// The site editor ("FTP Site Manager"), a Control Panel style page. New
// sites start with port 21, user and password "xbox" and passive mode,
// and open the keyboard for the name. Save only checks and keeps the
// values; the manager writes them. vtable 0x0024E02C, 0x174 bytes.
class CFTPSite : public CSettingsBase
{
public:
	enum
	{
		ITEM_NAME,
		ITEM_ADDRESS,
		ITEM_PORT,
		ITEM_USER,
		ITEM_PASSWORD,
		ITEM_PASV,
		ITEM_REMOTE_PATH,
		ITEM_LOCAL_PATH,
		ITEM_SAVE_HEADER,
		ITEM_SAVE,
		ITEM_RESET,
		ITEM_CANCEL,
	};

	CFTPSite();                                                  // 0x0013DDDB (this in esi)
	virtual ~CFTPSite();                                         // 0x0013DD6C (deleting 0x00020562)
	virtual HRESULT Init(CXBFont *font);                         // 0x0013D990
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x0013E31D
	virtual void Reset();                                        // 0x0013D699

	// Checks the lines and keeps their values. (this in ebx)
	BOOL Save();                                                 // 0x0013DE98

	BOOL m_bSaved;                     // +0xB8
	BOOL m_bNewSite;                   // +0xBC the name can be edited
	BOOL m_bQuickConnect;              // +0xC0 the local path can't be set
	std::string m_strName;             // +0xC4
	std::string m_strAddress;          // +0xE0
	int m_nPort;                       // +0xFC
	std::string m_strUser;             // +0x100
	std::string m_strPassword;         // +0x11C
	BOOL m_bPassive;                   // +0x138
	std::string m_strRemotePath;       // +0x13C
	std::string m_strLocalPath;        // +0x158
};

// What the manager is doing.
enum FtpManagerState
{
	FTPSTATE_SITES = 0,          // the site list
	FTPSTATE_CONNECTING = 1,
	FTPSTATE_BROWSE = 2,         // connected
	FTPSTATE_CANCELLING = 3,
	FTPSTATE_TRANSFER = 6,
	FTPSTATE_COLLECT = 7,        // listing what to delete
	FTPSTATE_DELETE = 8,         // asking, then deleting
	FTPSTATE_RENAME = 9,         // the keyboard asks
	FTPSTATE_NEW_FOLDER = 10,
	FTPSTATE_RAW_COMMAND = 11,
	FTPSTATE_ASK_EXIT = 12,
};

// The site menu's entries.
enum FtpSiteMenu
{
	FTPSITE_CONNECT = 0,
	FTPSITE_QUICK_CONNECT = 1,
	FTPSITE_NEW = 3,
	FTPSITE_EDIT = 4,
	FTPSITE_DELETE = 5,
	FTPSITE_REFRESH = 6,
	FTPSITE_VIEW_LOG = 8,
	FTPSITE_LOCAL_BROWSER = 9,
};

// The connected menu's entries.
enum FtpRemoteMenu
{
	FTPMENU_TRANSFER = 0,
	FTPMENU_ADD_TO_QUEUE = 1,
	FTPMENU_DELETE = 3,
	FTPMENU_RENAME = 4,
	FTPMENU_CREATE_FOLDER = 5,
	FTPMENU_RAW_COMMAND = 7,
	FTPMENU_REBOOT = 8,          // ... FTPMENU_DRIVE_STATUS: SITE commands
	FTPMENU_SHUTDOWN = 9,
	FTPMENU_POWER_CYCLE = 10,
	FTPMENU_CLOSE_TRAY = 11,
	FTPMENU_EJECT_TRAY = 12,
	FTPMENU_DRIVE_STATUS = 13,
	FTPMENU_VIEW_LOG = 15,
	FTPMENU_REFRESH = 16,
	FTPMENU_DISCONNECT = 17,
};

// vtable 0x0024B6E4, 0x750 bytes. See FtpManager.cpp and
// FtpManagerInput.cpp.
class CFTPManager : public CAppFrame
{
public:
	CFTPManager();                                        // 0x00123641 (this in ebx)
	virtual ~CFTPManager();                               // 0x001213F8 (deleting 0x0001EF3D)
	// Always fails: Setup() is the real one.
	virtual BOOL Init(CXBFontEx * /*font*/) { return E_FAIL; }   // 1 (0x0011F187)
	virtual BOOL IsOpen() { return m_bOpen; }             // 2 (0x0001E86B)
	virtual BOOL Render();                                // 3 (0x00121C85)
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4 (0x00124A37)
	// Back from the file manager: the site list again.
	virtual void Show();                                  // 5 (0x0011F5A2)
	// The file manager's font, background and left pane (its look and
	// starting folder). (background in eax, local in ecx)
	void Setup(CXBFontEx *font, IDirect3DTexture8 *background, IFileFolder *local);   // 0x001219B8

	int m_nState;                      // +0x004 FtpManagerState
	HANDLE m_hUnknown008;              // +0x008 -1; closed by the destructor
	BOOL m_bRightPane;                 // +0x00C the right pane (sites or FTP) has the input
	std::string m_strUnknown010;       // +0x010
	std::string m_strUnknown02C;       // +0x02C
	IFileFolder *m_pLocal;             // +0x048 the file manager's left pane
	CFTPFileFolder *m_pRemote;         // +0x04C while connected
	DWORD m_unknown050;                // +0x050
	DWORD m_unknown054;                // +0x054
	CFTPServerList m_siteList;         // +0x058
	CFTPQueue m_queue;                 // +0x560
	COptionMenu m_remoteMenu;          // +0x59C FtpRemoteMenu
	COptionMenu m_siteMenu;            // +0x630 FtpSiteMenu
	COptionMenu *m_pMenu;              // +0x6C4 the one for the current state
	CMessageBox *m_pMessageBox;        // +0x6C8
	BOOL m_bMessageShown;              // +0x6CC
	BOOL m_bKeyboardOpen;              // +0x6D0
	BOOL m_bQueueActive;               // +0x6D4 the queue has the input
	BOOL m_bMenuOpen;                  // +0x6D8
	BOOL m_bOpen;                      // +0x6DC
	BOOL m_bQuickConnect;              // +0x6E0 connect to m_pSiteEditor's values
	BOOL m_bAskingCancel;              // +0x6E4 "cancel the current operation?" is showing
	std::string m_strRemotePath;       // +0x6E8 the site's start folder
	float m_fY;                        // +0x704
	float m_fX;                        // +0x708
	float m_fWidth;                    // +0x70C
	float m_fHeight;                   // +0x710
	float m_fLineHeight;               // +0x714
	float m_fFontScaleX;               // +0x718
	float m_fFontScaleY;               // +0x71C
	float m_fQueueHeight;              // +0x720
	DWORD m_unknown724;                // +0x724
	CXBFontEx *m_pFont;                // +0x728
	BOOL m_bInitialized;               // +0x72C
	BOOL m_bLogShown;                  // +0x730
	BOOL m_bDrawBorder;                // +0x734 frames even over the background
	BOOL m_bSafeBorder;                // +0x738 the wider margin
	IDirect3DTexture8 *m_pBackground;  // +0x73C
	CFTPSite *m_pSiteEditor;           // +0x740
	BOOL m_bSiteEditorOpen;            // +0x744
	BOOL m_bOperationRunning;          // +0x748 a transfer or delete
	DWORD m_unknown74C;                // +0x74C

private:
	// The rectangle for SafeBorder. (this in ebx)
	void LayoutWindow();                                  // 0x0001E872
	// The panes' and the queue's rectangles. (this in esi)
	void LayoutPanes();                                   // 0x0011F362
	// Who has the input: the panes' and the queue's active flags. (this
	// in eax)
	void UpdateActivePane();                              // 0x0011F2DA
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x00121871 (this in edi)
	// Connects to the highlighted site (or the quick connect values).
	void Connect();                                       // 0x00122479
	// Queues the marked items (or the highlighted one) of the active pane
	// for the other side.
	void QueueSelection();                                // 0x00120E06
	// The editor's values into FtpSrv.xml, or a quick connect.
	void FinishSiteEditor();
	void FinishKeyboard();
	BOOL HandleOperation(int *action);
	void HandleMenuChoice();
};

// The settings the FTP classes use. 0x00270FA0
extern CAppSettings *g_pFtpSettings;

// The arrows the queue draws, from the media pack. 0x00270FB4 / 0x00270FB0
extern IDirect3DTexture8 *g_pFtpUpArrow;
extern IDirect3DTexture8 *g_pFtpDownArrow;

// Runs pane->DeleteListed() on a thread. (pane in ebx)
BOOL StartDeleteThread(IFileFolder *pane);                // 0x00126296
