// The music player's soundtrack tools ("Manage Tracks" in its START menu,
// 0x000F091F-0x000F4B99): a list of the sources (the audio CD, the Xbox
// soundtracks, the music folders), each with its own menu: play, copy
// songs to another soundtrack, edit (reorder, rename, delete songs),
// rename, delete. The Xbox soundtracks are edited in ST.DB
// (src/audio/SoundtrackDB.h). See TrackOptions.cpp, TrackEditor.cpp and
// TrackCopy.cpp; docs/MUSIC.md.
//
// Quirks: the "Audio CD" entry and an Xbox soundtrack with ID 0 (the first
// one ever made in a fresh ST.DB) have the same menu value, so that
// soundtrack opens the CD's menu; the CD's Copy is greyed out, so the CD
// ripping in CTrackCopy can't be reached (and its WMA encoder is a stub).
#pragma once

#include <string>
#include <vector>

#include "../app/Screens.h"
#include "../common/Platform.h"
#include "../ui/MenuDialogs.h"
#include "../musicplayer/AudioGUI.h"
#include "SongList.h"

class CAudioCDOption;
class CMessageBox;
class CSoundTrackOption;
class CTrackCopy;
class CTrackEditor;
class CXBFont;
class CXBFontEx;
struct IDirect3DTexture8;
struct SGamepadState;

// A menu value at or above this is a music folder: MUSIC_FOLDER_VALUE +
// its index in the music manager's list (2 and up).
#define MUSIC_FOLDER_VALUE 0x00FFFFFF

// Opens the shared on-screen keyboard for one of these screens and sets
// its open flag. (Inlined four times: 0x000F106D, 0x000F10DC, 0x000F114B,
// 0x000F11CB; maxLength in eax, 0 = 64, owner in edi.)
void OpenTrackKeyboard(const wchar_t *title, const wchar_t *text, BOOL password, int maxLength, int owner,
                       BOOL *open);
// Shows a message box, making it (with the language's button captions) on
// first use; shown is set while it's open.
void ShowTrackMessage(CMessageBox **messageBox, BOOL *shown, CXBFont *font, const char *text,
                      const wchar_t *title, int buttons);

// "Select Audio Track": the source list. 0xB8 bytes.
class CAudioTrackOption : public CAppFrame
{
public:
	CAudioTrackOption();                                  // 0x000F2A10 (this in edi)
	virtual ~CAudioTrackOption();                         // 0x000F0E67 (deleting 0x00029A27)
	virtual BOOL Init(CXBFontEx *font);                   // slot 1, 0x000F2F98
	virtual BOOL IsOpen() { return m_bOpen; }             // slot 2, 0x00029984
	virtual BOOL Render();                                // slot 3, 0x000F091F
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // slot 4, 0x000F2EC6
	// "Audio CD" is only enabled with an audio CD in the drive.
	virtual void Show();                                  // slot 5, 0x000F148C

	// The list: "Audio CD" (value 0), the Xbox soundtracks (their IDs) and
	// the music folders (MUSIC_FOLDER_VALUE + index). (this on the stack)
	void FillList();                                      // 0x000F2DE5
	// The selected entry's menu, titled with its name. (this in edi / ebx)
	void ShowCDOptions();                                 // 0x000F2974
	void ShowSoundtrackOptions();                         // 0x000F28BF

	int m_unknown04;                       // +0x04
	COptionMenu m_menu;                    // +0x08
	CAudioCDOption *m_pCDOptions;          // +0x9C
	CSoundTrackOption *m_pSoundtrackOptions;   // +0xA0
	BOOL m_bOpen;                          // +0xA4
	CXBFont *m_pFont;                      // +0xA8
	BOOL m_bInitialized;                   // +0xAC
	BOOL m_bCDOptionsOpen;                 // +0xB0
	BOOL m_bSoundtrackOptionsOpen;         // +0xB4
};

// The audio CD's menu: Play, Copy (greyed out). 0xB4 bytes.
class CAudioCDOption : public COptionMenu
{
public:
	CAudioCDOption();                                     // 0x000F2796 (this in eax)
	virtual ~CAudioCDOption();                            // 0x000F1361 (deleting 0x00029A7B)
	virtual HRESULT Create(CXBFont *font, const wchar_t *title, float x, float y, float width, float height,
	                       int maxVisible);               // slot 1, 0x000F2B99
	virtual void ProcessInput(int *action, SGamepadState **pad);   // slot 2, 0x000F42FE
	virtual void Render();                                // slot 3, 0x000F132C
	// Shown again for a new choice. (this in eax; inlined, 0x000F1312)
	void Reopen();

	int m_nAction;                         // +0x94 the option chosen
	int m_unknown98;                       // +0x98
	BOOL m_bKeyboardOpen;                  // +0x9C never set
	CMessageBox *m_pMessageBox;            // +0xA0 never made
	BOOL m_bMessageShown;                  // +0xA4 never set
	CTrackCopy *m_pTrackCopy;              // +0xA8 never made (Copy is disabled)
	BOOL m_bTrackCopyOpen;                 // +0xAC
	int m_unknownB0;                       // +0xB0 cleared only
};

// The menu of an Xbox soundtrack or a music folder: Play, Copy, Edit,
// Rename, Delete (only Play and Rename for a folder). 0xBC bytes.
class CSoundTrackOption : public COptionMenu
{
public:
	CSoundTrackOption();                                  // 0x000F2862 (this in eax)
	virtual ~CSoundTrackOption();                         // 0x000F1438 (deleting 0x00029A5F)
	virtual HRESULT Create(CXBFont *font, const wchar_t *title, float x, float y, float width, float height,
	                       int maxVisible);               // slot 1, 0x000F2D4E
	virtual void ProcessInput(int *action, SGamepadState **pad);   // slot 2, 0x000F43B4
	virtual void Render();                                // slot 3, 0x000F13F3
	// Shown for the current soundtrack: Copy, Edit and Delete only for an
	// Xbox soundtrack. (this in eax; inlined, 0x000F13A0)
	void Reopen();
	// (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x000F2C07
	// The song editor / the copy screen for a soundtrack. (this in edi)
	void ShowEditor(DWORD soundtrack);                    // 0x000F1D97
	void ShowTrackCopy(DWORD soundtrack);                 // 0x000F27D7

	int m_nAction;                         // +0x94 the option chosen
	BOOL m_bChanged;                       // +0x98 renamed or deleted: refill the list
	DWORD m_dwSoundtrack;                  // +0x9C the menu value (ID, or MUSIC_FOLDER_VALUE + index)
	BOOL m_bKeyboardOpen;                  // +0xA0 the new name
	CMessageBox *m_pMessageBox;            // +0xA4
	BOOL m_bMessageShown;                  // +0xA8
	CTrackEditor *m_pEditor;               // +0xAC
	BOOL m_bEditorOpen;                    // +0xB0
	CTrackCopy *m_pTrackCopy;              // +0xB4
	BOOL m_bTrackCopyOpen;                 // +0xB8
};

// The song editor: the soundtrack's songs over four buttons, Move Up, Move
// Down, Rename, Delete (Left/Right pick one, A uses it on the selected
// song). 0xAC bytes.
class CTrackEditor : public CAppFrame
{
public:
	CTrackEditor();                                       // 0x000F1B8C (this in esi)
	virtual ~CTrackEditor();                              // 0x000F1B5A (deleting 0x000299EF)
	virtual BOOL Init(CXBFontEx *font);                   // slot 1, 0x000F21D6
	virtual BOOL IsOpen() { return m_bOpen; }             // slot 2, 0x00029980
	virtual BOOL Render();                                // slot 3, 0x000F4B99
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // slot 4, 0x000F33C4
	virtual void Show();                                  // slot 5, 0x000F2094

	// (this in ecx, soundtrack in eax; inlined, 0x000F123A)
	void SetSoundtrack(DWORD soundtrack);
	// (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x000F3280

	float m_fHeight;                       // +0x04 the song list's box (300 at 640x480)
	float m_fWidth;                        // +0x08 400
	float m_fX;                            // +0x0C 120
	float m_fY;                            // +0x10 60
	float m_fScaleX;                       // +0x14
	float m_fScaleY;                       // +0x18
	CXBFont *m_pFont;                      // +0x1C
	BOOL m_bOpen;                          // +0x20
	CSongListControl m_songList;           // +0x24
	IDirect3DTexture8 *m_pPanelTexture;    // +0x90 "Keyboard": the button row's background
	UINT m_nButton;                        // +0x94
	DWORD m_dwSoundtrack;                  // +0x98
	BOOL m_bDeletePending;                 // +0x9C the message box is the delete question
	BOOL m_bKeyboardOpen;                  // +0xA0 the new name
	CMessageBox *m_pMessageBox;            // +0xA4
	BOOL m_bMessageShown;                  // +0xA8
};

// The copy screen: the source's songs over Select, Select All and Copy,
// then a menu of the target ("New Soundtrack..." or another Xbox
// soundtrack). The copy runs on a thread. 0x138 bytes.
class CTrackCopy : public CAppFrame
{
public:
	CTrackCopy();                                         // 0x000F1FCF (this in esi)
	virtual ~CTrackCopy();                                // 0x000F1AE1 (deleting 0x00029A0B)
	virtual BOOL Init(CXBFontEx *font);                   // slot 1, 0x000F1F64
	virtual BOOL IsOpen() { return m_bOpen; }             // slot 2, 0x00029980
	virtual BOOL Render();                                // slot 3, 0x000F4899
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // slot 4, 0x000F3953
	virtual void Show();                                  // slot 5, 0x000F1E22

	// (this in ecx, soundtrack in eax; inlined, 0x000F11BA)
	void SetSoundtrack(DWORD soundtrack);
	// (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x000F313C
	// The target menu. (this on the stack)
	void ShowTargetMenu();                                // 0x000F3016
	// Starts the copy thread into target. (this in edi)
	void StartCopy(DWORD target);                         // 0x000F38EF
	static DWORD __stdcall CopyThread(void *param);       // 0x000F38BC
	// Copies the ticked songs; TRUE if any was. (this in eax)
	BOOL CopySelected();                                  // 0x000F3752
	// One song: a copy of its WMA file (from the CD: read, "encoded" and
	// renamed), added to the target in ST.DB.
	BOOL CopySong(UINT index, const wchar_t *name, DWORD length);   // 0x000F14C9

	float m_fHeight;                       // +0x04 as CTrackEditor
	float m_fWidth;                        // +0x08
	float m_fX;                            // +0x0C
	float m_fY;                            // +0x10
	float m_fScaleX;                       // +0x14
	float m_fScaleY;                       // +0x18
	CXBFont *m_pFont;                      // +0x1C
	BOOL m_bOpen;                          // +0x20
	CSongListControl m_songList;           // +0x24
	IDirect3DTexture8 *m_pPanelTexture;    // +0x90
	UINT m_nButton;                        // +0x94 Right goes up to 3 (no button)
	DWORD m_dwSoundtrack;                  // +0x98 the source; 0 is read as the audio CD
	CMessageBox *m_pMessageBox;            // +0x9C
	BOOL m_bMessageShown;                  // +0xA0
	BOOL m_bKeyboardOpen;                  // +0xA4 the new soundtrack's name
	COptionMenu *m_pTargetMenu;            // +0xA8 "Copy"
	BOOL m_bTargetMenuOpen;                // +0xAC
	BOOL m_bNewSoundtrack;                 // +0xB0 the target was made for this copy
	std::string m_strUnusedB4;             // +0xB4
	std::vector<std::string> m_unusedD0;   // +0xD0 cleared only TODO: verify the type
	HANDLE m_hThread;                      // +0xE0
	BYTE *m_pBuffer;                       // +0xE4 0x4000 bytes, during a copy
	int m_nError;                          // +0xE8 CopyError
	int m_nProgress;                       // +0xEC percent of the song
	wchar_t m_szSongName[32];              // +0xF0 the song being copied
	int m_nTrack;                          // +0x130 songs copied so far (the CD's "Track %d")
	DWORD m_dwTarget;                      // +0x134
};

// CTrackCopy::m_nError.
enum CopyError
{
	COPY_OK = 0,
	COPY_FILE_ERROR = 1,       // a file couldn't be made, written or read; or X: is full
	COPY_DB_ERROR = 2,         // the song couldn't be added to ST.DB
	COPY_NO_DISC = 3,          // no CD (the TOC isn't open)
	COPY_BAD_TRACK = 4,        // an empty track, or a read error
};

// An unused CD copy screen: a CAppFrame that lists the audio CD's tracks
// ("1. Track 1", "03:45") and draws nothing; no constructor is left.
// (TrackCopy.cpp; vtable 0x0024A93C)
class CDDACopy : public CAppFrame
{
public:
	virtual ~CDDACopy();                                  // 0x00130C30 (this in edi; deleting 0x00012491)
	// Lists the tracks; E_FAIL (and no font) without a CD's table of
	// contents.
	virtual BOOL Init(CXBFontEx *font);                   // 1, 0x00130C6F
	virtual BOOL IsOpen() { return m_bOpen; }             // 2, 0x00130B6B
	virtual BOOL Render() { return FALSE; }               // 3, 0x00130B63
	virtual HRESULT ProcessInput(int * /*action*/, SGamepadState ** /*pad*/) { return S_OK; }   // 4, 0x00130B66
	// Opens it (with a font) at 145, 40, 350 x 400, scaled.
	virtual void Show();                                  // 5, 0x00130B6F

	std::vector<SPlayerTrack> m_tracks;    // +0x04
	BOOL m_bOpen;                          // +0x14
	CXBFontEx *m_pFont;                    // +0x18
	BYTE m_unknown1C[0x10];                // +0x1C
	float m_fX;                            // +0x2C
	float m_fY;                            // +0x30
	float m_fWidth;                        // +0x34
	float m_fHeight;                       // +0x38
	float m_fPadding;                      // +0x3C 10
	float m_fScaleX;                       // +0x40
	float m_fScaleY;                       // +0x44
};

// A CAppFrame with a COptionMenu: only its destructor and vtable are left
// (nothing makes one). 0x98 bytes.
class CTrackManager : public CAppFrame
{
public:
	virtual ~CTrackManager();                             // 0x000F0E52 (this in esi; deleting 0x00029A43)

	COptionMenu m_menu;                    // +0x04
};

// The WMA encoder for ripped CD tracks, left as a stub: it opens
// x:\tempcda.cda, builds a gain table for the peak level, then frees
// everything and deletes the file without writing x:\tempwma.wma. Quirk:
// it tests an uninitialized local (meant to be the file size's high
// word); see TrackCopy.cpp.
BOOL EncodeTrackToWma(short peak);                        // 0x000D9155
