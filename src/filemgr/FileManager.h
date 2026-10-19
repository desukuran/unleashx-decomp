// The file manager (mode MODE_FILEMANAGER): two panes (CFileFolder) side by
// side or one at a time, a context menu with the file operations, and the
// viewers it opens files in (text editor, pictures, music, video, archives,
// meshes). The operations run on threads that report through the
// FileOps.h progress state; the manager steps through collecting, asking,
// running and reporting. docs/FILE_MANAGER.md describes it.
#pragma once

#include <string>

#include "../app/Screens.h"
#include "../common/Platform.h"
#include "../ui/MenuDialogs.h"
#include "FileFolder.h"

class CXBFontEx;
class CAppSettings;
class CAppFrame;
class CFTPManager;
class CWMVideoPlayer;
struct IDirect3DTexture8;
struct SGamepadState;

// What the manager is doing (m_nState).
enum FileManagerState
{
	FMSTATE_BROWSE = 1,
	FMSTATE_COPY = 2,
	FMSTATE_MOVE = 3,
	FMSTATE_DELETE = 4,
	FMSTATE_RENAME = 5,          // the keyboard: the new name
	FMSTATE_NEW_FOLDER = 6,      // the keyboard: the folder name
	FMSTATE_CALC_SIZE = 7,
	FMSTATE_COPY_DVD = 8,
	FMSTATE_COPY_DISC = 9,       // the keyboard: where to copy the disc
	FMSTATE_ZIP_NAME = 10,       // the keyboard: the zip file
	FMSTATE_ZIP = 11,
	FMSTATE_XBE_TITLE = 13,      // the keyboard: the new title
	FMSTATE_FORMAT = 14,
};

// The steps of an operation (m_nStep).
enum FileManagerStep
{
	FMSTEP_COLLECTING = 0,       // the collect thread lists the files
	FMSTEP_CONFIRM = 1,          // "You are about to ..."
	FMSTEP_RUNNING = 2,          // the operation thread works
	FMSTEP_DONE = 3,             // the result is shown
};

// The context menu's entries (COptionMenu values).
enum FileManagerMenu
{
	FMMENU_ROOT = 0,
	FMMENU_SORT = 1,
	FMMENU_SELECT_ALL = 3,
	FMMENU_CLEAR_SELECTION = 4,
	FMMENU_INVERT_SELECTION = 5,
	FMMENU_COPY = 7,
	FMMENU_COPY_RESUME = 8,      // copy, skipping files already there with the same size
	FMMENU_MOVE = 9,
	FMMENU_DELETE = 10,
	FMMENU_RENAME = 11,
	FMMENU_NEW_FOLDER = 12,
	FMMENU_CALC_SIZE = 13,
	FMMENU_COPY_DISC = 15,
	FMMENU_QUICK_COPY_DISC = 16,
	FMMENU_ADD_TO_ZIP = 18,
	FMMENU_PATCH_XBE = 19,
	FMMENU_RENAME_XBE_TITLE = 20,
	FMMENU_NEW_TEXT = 22,
	FMMENU_OPEN_TEXT = 23,
	FMMENU_FORMAT = 25,
	FMMENU_FTP = 27,
};

// vtable 0x0024AB98 (only the deleting destructor), 0x8F0 bytes.
class CFileManager
{
public:
	CFileManager();                                       // 0x001297AC (this in edi)
	virtual ~CFileManager();                              // 0x00127A5C (deleting 0x00018FDB)
	// The font, the longest name, the background and the icons. Once.
	// (font in ecx, maxNameLength in eax, this in edx)
	HRESULT Init(CXBFontEx *font, int maxNameLength, int unused, IDirect3DTexture8 *background,
	             IDirect3DTexture8 *folderIcon, IDirect3DTexture8 *fileIcon);   // 0x001294E2
	// Opens path (resolved) in the active pane if it's a folder. (path in
	// edi, this in esi)
	void OpenPath(const char *path);                      // 0x00128C01
	// Back to browsing; lays out and refreshes. (this in eax)
	void Reset();                                         // 0x0002B4C9
	// (this in esi, texture in edi)
	void SetBackground(IDirect3DTexture8 *texture);       // 0x0002B4A1
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x0012C91D (FileManagerInput.cpp)
	HRESULT Render();                                     // 0x00127052 (this in eax)

	// The pane with the input.
	IFileFolder *ActivePane() { return m_nActivePane == 0 ? (IFileFolder *)&m_panes[0] : (IFileFolder *)&m_panes[1]; }
	IFileFolder *OtherPane() { return m_nActivePane == 0 ? (IFileFolder *)&m_panes[1] : (IFileFolder *)&m_panes[0]; }

	DWORD m_unknown004;                // +0x004
	int m_nActivePane;                 // +0x008 0 = left ("A"), 1 = right ("B")
	std::string m_strUnknown00C;       // +0x00C
	std::string m_strZipFile;          // +0x028 Add to zip's file
	DWORD m_unknown044;                // +0x044
	CFileFolder m_panes[2];            // +0x048, +0x420
	int m_nState;                      // +0x7F8 FileManagerState
	int m_nPrevState;                  // +0x7FC
	int m_nStep;                       // +0x800 FileManagerStep
	CFileFolderEventsink m_events;     // +0x804 opens the panes' files
	COptionMenu m_menu;                // +0x808 the context menu
	CMessageBox *m_pMessageBox;        // +0x89C
	CDropDown *m_pSortMenu;            // +0x8A0
	BOOL m_bMessageShown;              // +0x8A4
	BOOL m_bKeyboardOpen;              // +0x8A8
	BOOL m_bMenuOpen;                  // +0x8AC
	BOOL m_bOpen;                      // +0x8B0 BACK clears it
	BOOL m_bSideBySide;                // +0x8B4 both panes shown (else the active one, full width)
	float m_fY;                        // +0x8B8
	float m_fX;                        // +0x8BC
	float m_fWidth;                    // +0x8C0
	float m_fHeight;                   // +0x8C4
	float m_fLineHeight;               // +0x8C8
	float m_fFontScaleX;               // +0x8CC
	float m_fFontScaleY;               // +0x8D0
	int m_nMaxNameLength;              // +0x8D4
	CXBFontEx *m_pFont;                // +0x8D8
	BOOL m_bInitialized;               // +0x8DC
	BOOL m_bViewLog;                   // +0x8E0 the result box asks to view the log
	BOOL m_bSafeBorder;                // +0x8E4 <FileManager SafeBorder> (smaller frame)
	BOOL m_bDrawBorder;                // +0x8E8 frames even over a background
	IDirect3DTexture8 *m_pBackground;  // +0x8EC the skin's file manager background

private:
	// The frame for the screen (safe border or not). (this in eax)
	void SetDefaultRect();                                // 0x00014189
	// Places the panes. (this in ebx)
	void Layout();                                        // 0x00126362
	// The background picture, else a dimmed screen and a bevel box. (this
	// in esi)
	void DrawBackground();                                // 0x00126839
	// (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x0012939B
	// A result: with a log, it asks whether to view it. (this in ebx)
	void ShowResult(std::string text, const wchar_t *title);   // 0x0012A2C4
	// The text editor (g_pFileViewer), made once. (this on the stack)
	BOOL CreateTextEditor();                              // 0x00129345
	// Starts the collect thread for pane. (pane in edi)
	static BOOL StartCollect(IFileFolder *pane);          // 0x00128855
	static unsigned __stdcall CollectThread(void *param); // 0x0012824E
	// The copy/move/delete/zip thread. (FileManagerOps.cpp)
	static unsigned __stdcall OperationThread(void *param);   // 0x0012B374
	// The keyboard closed with text for m_nState.
	void OnKeyboardDone();
	// The context menu's choice.
	void OnMenu(IFileFolder *pane);
	// Enables the menu's entries for pane and shows it.
	void ShowMenu(IFileFolder *pane);
	// Copy, move and delete: steps 0 to 3.
	void StepOperation(int *action);
	void StepZip(int *action);
	void StepCalcSize(int *action);
	BOOL StepCopyDVD(int *action);
	void StepFormat();
};

// The error to report: GetLastError(), else g_dwOperationError. (0x0012631D)
DWORD GetOperationError();

// The panes' sounds (FileFolder.cpp).
extern CSound *g_pFolderSelectSound;     // 0x002761C8
extern CSound *g_pFolderScrollSound;     // 0x002761CC
extern CSound *g_pFolderDoneSound;       // 0x002761D0
extern BOOL g_bFolderSounds;             // 0x002761D4

// The file manager's viewers and their flags. 0x0027618C..0x002761D8
extern BOOL g_bFileAudioPlayer;          // 0x0027618C the music player (g_pAudioGUI) is up
extern BOOL g_bFileImageViewer;          // 0x00276190 (set, never read)
extern CWMVideoPlayer *g_pFileVideoPlayer;   // 0x00276198
extern BOOL g_bFileZipOpen;              // 0x0027619C the viewer is a CZipManager
extern BOOL g_bFileRarOpen;              // 0x002761A0 ... a CRarManager
extern BOOL g_bFileXipOpen;              // 0x002761A4 ... a CXipManager
extern BOOL g_bFileVideoPlaying;         // 0x002761A8
extern BOOL g_bFileTextEditor;           // 0x002761AC the viewer is the text editor
extern BOOL g_bFtpManagerOpen;           // 0x002761B0
extern CFTPManager *g_pFtpManager;       // 0x002761B4
extern BOOL g_bFileViewerOpen;           // 0x002761B8
extern CAppFrame *g_pFileViewer;         // 0x002761BC
extern BOOL g_bFilesCollected;           // 0x002761C0 the collect thread finished without a cancel
extern BOOL g_bCopyResume;               // 0x002761C4 "Copy (resume)": skip files already there
extern CAppSettings *g_pFileSettings;    // 0x002761D8
extern BOOL g_bDeleteViewerOnClose;      // 0x00269D30
