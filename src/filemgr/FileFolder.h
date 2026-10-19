// A pane of the file manager: the listing of one folder (or of the drives,
// "Root") with selection marks, sorting and the file operations that work
// on the marked items. IFileFolder holds the list and draws it; CFileFolder
// reads the Xbox's own drives through the virtual file layer
// (src/system/FileIO.h); CFTPFileFolder (the FTP manager) reads an FTP site.
//
// Paths inside a pane are kept FTP style: "/" is the drive list and
// "/C/Apps" is C:\Apps (see CFileFolder::ToXboxPath).
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "../system/FileIO.h"

class CXBFont;
class CXBFontEx;
class CAppSettings;
class CSound;
struct IDirect3DTexture8;
struct SGamepadState;

// What an item opens with (CFileFolderEventsink::OnOpen).
enum FolderItemType
{
	ITEM_XBE = 0,        // xbe: launched
	ITEM_AUDIO = 1,      // wav wma mp3: the music player
	ITEM_VIDEO = 2,      // wmv xmv: the video player
	ITEM_TEXT = 3,       // txt xml ini nfo cfg lst: the text editor
	ITEM_IMAGE = 4,      // bmp jpg png xbx xpr: the picture viewer
	ITEM_FOLDER = 5,
	ITEM_ARCHIVE = 6,    // zip xip rar: browsed like a folder
	ITEM_MESH = 7,       // xbg xm: the mesh viewer (CXMViewer)
	ITEM_OTHER = 8,
};

// One line of a pane. 0x78 bytes.
struct SFolderItem
{
	SFolderItem();                                               // 0x00013622 (this in esi)
	~SFolderItem();                                              // 0x00013658 (this in eax; deleting 0x00014124)
	// name: the file name (UTF-8). size: its size. date: its time (0 for
	// none). displaySize: what the size column shows instead of size
	// (0 = size). dateNumber: what the date column shows when there's no
	// date (drives: their total size). inKB: sizes as "12 KB" / "3 MB"
	// instead of plain bytes. FALSE without a name. (isFolder in eax)
	BOOL Init(const char *name, DWORD size, FILETIME date, ULONGLONG displaySize, ULONGLONG dateNumber,
	          BOOL inKB, BOOL isFolder);                         // 0x00013679
	// The name as narrow text.
	std::string GetName() const;                                 // 0x00013597

	CAppSettings *m_pSettings;         // +0x00
	int m_nType;                       // +0x04 FolderItemType
	BOOL m_bFolder;                    // +0x08
	BOOL m_bParent;                    // +0x0C ".."
	wchar_t *m_pszName;                // +0x10 new[]
	char *m_pszExtension;              // +0x14 new[], without the dot
	wchar_t m_szSize[0x14];            // +0x18
	wchar_t m_szDate[0x14];            // +0x40 date and time, or a number
	DWORD m_dwSize;                    // +0x68
	BOOL m_bMarked;                    // +0x6C
	FILETIME m_date;                   // +0x70
};

// Receives what the panes want opened. (vtable 0x0024AB1C)
class CFileFolderEventsink
{
public:
	virtual ~CFileFolderEventsink() {}                           // 0 (0x00013D03)
	// Opens path (an Xbox path) according to type. (FileManagerOpen.cpp)
	virtual void OnOpen(std::string path, int type, CXBFontEx *font);   // 1 (0x0012C480)
};

// What a pane lists.
enum FolderFilter
{
	FILTER_ALL = 0,
	FILTER_FILES = 1,        // no folders
	FILTER_FOLDERS = 2,      // no files
};

// How a pane sorts (the "Sort" drop-down).
enum FolderSort
{
	SORT_NAME = 0,
	SORT_SIZE = 1,
	SORT_TYPE = 2,           // by extension
	SORT_DATE = 3,
};

// The pane base. vtable 0x0024AC04, 0x3D0 bytes. Slots 1-5 are the
// listing source's (CFileFolder: local drives).
class IFileFolder
{
public:
	IFileFolder();                                               // 0x00013EEF (this in esi)
	virtual void AddEntry(const WIN32_FIND_DATAA *data, ULONGLONG displaySize, ULONGLONG dateNumber,
	                      BOOL inKB);                            // 0 (0x0012AA4E)
	// Reads the current folder (on a thread, see Refresh).
	virtual void LoadList() {}                                   // 1 (0x00013D20)
	// Starts LoadList(); "Checking" shows after a quarter of a second.
	virtual void Refresh() {}                                    // 2 (0x00013D20)
	// out = base + "/" + name in FTP style (name may be absolute).
	virtual void BuildPath(std::string * /*out*/, const char * /*base*/, const char * /*name*/) {}   // 3 (0x0002A7A0)
	// Adds the current folder's entries with AddEntry().
	virtual void ListEntries() {}                                // 4 (0x00013D20)
	// "/C/Apps" -> "C:\Apps".
	virtual void ToXboxPath(std::string * /*out*/, const char * /*path*/) {}   // 5 (0x0002B8D7)
	// Lists everything under path into the file operation list.
	virtual void CollectFiles(const char *path);                 // 6 (0x0012A2B4)
	// The delete thread's work: everything in the operation list. TRUE if
	// something failed.
	virtual BOOL DeleteListed();                                 // 7 (0x0012A006)
	// Renames the current item to name. (MoveFileA's result)
	virtual BOOL RenameCurrent(const char *name);                // 8 (0x00127FA0)
	// Creates the folder name in the current folder.
	virtual BOOL CreateFolder(const char *name);                 // 9 (0x00127E8E)
	virtual BOOL IsActive() { return m_bActive; }                // 10 (0x00013D21)
	// Moving, marking (X), BLACK (invert marks), B (up a level) and the
	// pointer. A is CFileFolder's.
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 11 (0x00128D9D)
	virtual HRESULT Render();                                    // 12 (0x001268A3)
	// Keeps the scroll position and refreshes (after a font change).
	virtual void Reload();                                       // 13 (0x00126691)
	// The font, the longest name to show, and the two icons (either can be
	// NULL; their size sets the icon column). (font+0x14/0x18: its scale)
	virtual HRESULT Init(CXBFont *font, int unused, int maxNameLength, IDirect3DTexture8 *folderIcon,
	                     IDirect3DTexture8 *fileIcon);           // 14 (0x00129128)
	virtual void SetRect(float x, float y, float width, float height);   // 15 (0x00013D28)
	// Opens an FTP-style path ("C/Apps" or "/C:/Apps" work too).
	virtual void SetPath(const char *path);                      // 16 (0x001280B7)
	virtual void MarkAll(BOOL mark);                             // 17 (0x00126222)
	virtual void InvertMarks();                                  // 18 (0x001261CE)
	virtual BOOL HasMarks();                                     // 19 (0x001261A6)
	// "Root" or the current folder (wide).
	virtual const wchar_t *GetTitle() { return m_szTitle; }      // 20 (0x00013DE6)
	// The current folder's Xbox path; in Root, the highlighted drive's
	// letter.
	virtual std::string GetCurrentPath();                        // 21 (0x00013DEA)
	virtual ~IFileFolder();                                      // 22 (0x00014040; deleting 0x00014108)

	// Sort by sort (again: reverse the order). (this in ecx, sort in edx)
	void SetSort(int sort);                                      // 0x001262F5
	// The item at index in the shown list, or NULL.
	SFolderItem *GetItem(int index);
	// Clears both lists (deleting the items).
	void ClearItems();

	DWORD m_unknown04;                 // +0x004
	std::vector<SFolderItem *> m_files;     // +0x008
	std::vector<SFolderItem *> m_folders;   // +0x018
	std::string m_strPath;             // +0x028 "/" or "/C/Apps"
	wchar_t m_szTitle[0x105];          // +0x044
	BOOL m_bDetails;                   // +0x250 show dates (else only sizes)
	float m_fDateWidth;                // +0x254 the date column
	CXBFont *m_pFont;                  // +0x258
	IDirect3DTexture8 *m_pFileIcon;    // +0x25C
	IDirect3DTexture8 *m_pFolderIcon;  // +0x260
	int m_nLevel;                      // +0x264 0 = Root
	int m_selectionStack[20];          // +0x268 per level, to come back to
	int m_topStack[20];                // +0x2B8
	BOOL m_bOnDisc;                    // +0x308 in D:
	float m_fIconWidth;                // +0x30C the icon column (icon + 2)
	float m_fIconHeight;               // +0x310
	BOOL m_bListed;                    // +0x314 the list is ready
	BOOL m_bNotDrawing;                // +0x318 Render() isn't walking the list
	DWORD m_dwRefreshTick;             // +0x31C
	int m_nFilter;                     // +0x320 FolderFilter
	BOOL m_bActive;                    // +0x324 the pane with the input
	int m_nTop;                        // +0x328
	int m_nBottom;                     // +0x32C
	int m_nSelected;                   // +0x330
	int m_nLineHeight;                 // +0x334
	int m_nVisible;                    // +0x338
	DWORD m_unknown33C;                // +0x33C
	float m_fFontScaleX;               // +0x340 the font's scale at Init
	float m_fFontScaleY;               // +0x344
	DWORD m_unknown348;                // +0x348
	int m_nCount;                      // +0x34C items shown
	BOOL m_bCollected;                 // +0x350 the collect thread is done
	BOOL m_bOperationDone;             // +0x354 the delete thread is done
	int m_rect[4];                     // +0x358 x, y, right, bottom for the pointer
	float m_fY;                        // +0x368
	float m_fX;                        // +0x36C
	float m_fWidth;                    // +0x370
	float m_fHeight;                   // +0x374
	int m_nMaxNameLength;              // +0x378
	int m_nSort;                       // +0x37C FolderSort
	BYTE m_bSortDescending;            // +0x380
	BYTE m_reserved381[3];
	int m_nFolderCount;                // +0x384
	int m_nFileCount;                  // +0x388
	wchar_t m_szTotalSize[15];         // +0x38C
	ULONGLONG m_ullTotalSize;          // +0x3B0 of the files listed
	SFolderItem *m_pCurrent;           // +0x3B8 the highlighted item (not "..")
	std::vector<SFolderItem *> m_items;     // +0x3BC what's shown: "..", folders, files
	DWORD m_unknown3CC;                // +0x3CC
};

// A pane on the Xbox's drives. vtable 0x0024ABA4, 0x3D8 bytes.
class CFileFolder : public IFileFolder
{
public:
	CFileFolder();                                               // 0x00128C7E (this in eax)
	virtual ~CFileFolder();                                      // 0x00014145 (deleting 0x0001416D)
	// The listing thread's work: clears, lists, sorts, totals.
	virtual void LoadList();                                     // 1 (0x0012A732)
	virtual void Refresh();                                      // 2 (0x00126647)
	virtual void BuildPath(std::string *out, const char *base, const char *name);   // 3 (0x00127B17)
	// Drives in Root ("X [Not formatted]" for unformatted memory units and
	// USB drives), else the folder through its CDirectory.
	virtual void ListEntries();                                  // 4 (0x0012A36B)
	virtual void ToXboxPath(std::string *out, const char *path); // 5 (0x00127C60)
	// IFileFolder's, plus A: enter a folder (".." goes up; D: only with a
	// readable disc) or open a file through m_pEvents.
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 11 (0x00129874)

	CDirectory *m_pDir;                // +0x3D0 for the current drive
	CFileFolderEventsink *m_pEvents;   // +0x3D4

private:
	static unsigned __stdcall LoadThread(void *param);           // 0x0012612A
};

// Set whenever a pane's list or marks change (never read). 0x00269D34
extern BOOL g_bFolderListChanged;
