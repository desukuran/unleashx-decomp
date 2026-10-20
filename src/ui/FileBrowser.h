// The file/folder pop-up (CFileBrowser, FileBrowser.cpp): the settings
// pages' "Browse..." items and the text editor's "Open Text File". It lists
// the mounted drives, then a drive's folders (and, when choosing files, the
// files matching the filters), ".." first and folders before files.
//
// A enters a folder (or picks a file), B or ".." goes up, START picks the
// highlighted entry, BACK or BLACK cancels. "[New Folder]" asks for a name on
// the keyboard and creates it.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"

class CXBFont;
class CSound;
class CAppSettings;
struct SGamepadState;

// A list entry. 0x12C bytes.
struct SFileItem
{
	// (name in edx, this in esi)
	SFileItem(const char *name, BOOL isParent, BOOL isFolder, int unknown);   // 0x00013C39

	char name[0x104];               // +0x000 as found ("C" for a drive)
	std::wstring label;             // +0x104 what's shown: folders as "<name>"
	BOOL isParent;                  // +0x120 ".."
	BOOL isFolder;                  // +0x124
	int unknown128;                 // +0x128 always 0
};

// ".." first, then folders, then files, then "[New Folder]"; by label,
// ignoring case. (a in ecx, b in eax)
bool operator<(const SFileItem &a, const SFileItem &b);   // 0x001266FD

// 0x180 bytes. Most methods take this in a register (noted).
class CFileBrowser
{
public:
	CFileBrowser();                                       // 0x00127A32 (this in eax)
	explicit CFileBrowser(BOOL flag);                     // 0x001279FC (this in eax)
	~CFileBrowser();                                      // 0x00127013 (this in edi)
	// font: the list's (and the default caption font). Lists the drives
	// and shows it. E_FAIL without a font; nothing the second time.
	// (this in eax)
	HRESULT Create(CXBFont *font, const wchar_t *title);  // 0x0012C378
	// (this in esi)
	HRESULT Render();                                     // 0x00129B10
	// NULL or "" = "Browse Folder"; at most 40 characters. (this in eax,
	// title in esi)
	void SetTitle(const wchar_t *title);                  // 0x000113D9
	// TRUE: files are listed too (filtered) and A on one picks it; FALSE:
	// folders only, and A always enters. (this in esi, value in ecx)
	void SetSelectFiles(BOOL selectFiles);                // 0x000112BA
	// Only files ending in one of these (any case); NULL clears the list.
	// (this in eax)
	void AddFilter(const char *extension);                // 0x001281F6
	// Starts in this folder, if it exists. (this in ebx, path in esi)
	void SetStartPath(const char *path);                  // 0x0001142F
	// Opens it (rescanning the folder if asked) and lays it out. (this
	// in ebx)
	void Show(BOOL refresh);                              // 0x0001124E
	// (this in ebx, action in ecx, pad in eax)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x0012AF3F
	BOOL IsOpen() const { return m_bOpen && !m_bCancelled; }   // 0x0001123B (this in eax)
	// The chosen entry's full path ("" if cancelled). (this in eax)
	std::string GetSelectedPath();                        // 0x000112D9

	std::vector<std::string> m_filters;   // +0x000
	std::string m_strPath;                // +0x010 "" = the drive list
	float m_fY;                           // +0x02C
	float m_fX;                           // +0x030
	float m_fWidth;                       // +0x034
	float m_fHeight;                      // +0x038
	CXBFont *m_pFont;                     // +0x03C the list
	CXBFont *m_pCaptionFont;              // +0x040 the settings' dialog font, else m_pFont
	CXBFont *m_pSymbolFont;               // +0x044 XboxDings (the A/B glyphs)
	BOOL m_bOpen;                         // +0x048
	BOOL m_bCancelled;                    // +0x04C
	BOOL m_bKeyboardOpen;                 // +0x050 naming a new folder
	std::vector<SFileItem> m_items;       // +0x054
	CSound *m_pSoundMove;                 // +0x064 "Scroll"
	int m_nSelected;                      // +0x068
	int m_nTop;                           // +0x06C
	int m_nBottom;                        // +0x070 the last one shown
	int m_nVisible;                       // +0x074 0 until the first Render
	int m_selectedStack[20];              // +0x078 per folder depth, to come back to
	int m_topStack[20];                   // +0x0C8
	int m_nDepth;                         // +0x118
	BOOL m_bSelectFiles;                  // +0x11C
	BOOL m_bNewFolderItem;                // +0x120 list "[New Folder]"
	BOOL m_bFolderResult;                 // +0x124 START can pick a folder
	wchar_t m_szTitle[41];                // +0x128
	BOOL m_unknown17C;                    // +0x17C 1 from the music player's constructor, never read

private:
	void Reset();                                         // 0x001267B6 (this in esi)
	// Lists m_strPath (or the drives). (this on the stack)
	HRESULT Refresh();                                    // 0x0012ABE3
	// Keeps the selection in view. (inlined in ProcessInput)
	void ScrollToSelection();
};

// The settings object the file manager code uses. TODO: verify (set by
// CFileManager too)
extern CAppSettings *g_pFileSettings;                     // 0x002761D8
