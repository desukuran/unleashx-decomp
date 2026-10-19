// The music player screen (mode 9, CAudioGUI): the skin's <MusicPlayer>
// elements (buttons, state icons, level meter, texts), a song list on the
// right stick click and an options menu on START (play the audio CD,
// manage soundtracks, open a file or a folder). See AudioGUI.cpp (setup,
// layout), AudioGUIRender.cpp and AudioGUIInput.cpp; docs/MUSIC.md.
#pragma once

#include <string>
#include <vector>

#include "../app/Screens.h"
#include "../common/Platform.h"

class CAppSettings;
class CFileBrowser;
class CSongListControl;
class CXBFont;
struct IDirect3DTexture8;
struct SGamepadState;

// The player's buttons, in the order of the skin's button shapes
// (SHAPE_PREV + id) and of CSkin::m_nButtonPrev...
enum PlayerButton
{
	BUTTON_PREV = 0,           // previous song
	BUTTON_PLAY = 1,
	BUTTON_PAUSE = 2,
	BUTTON_STOP = 3,
	BUTTON_NEXT = 4,           // next song
	BUTTON_PREVTRACK = 5,      // previous soundtrack
	BUTTON_NEXTTRACK = 6,      // next soundtrack
	BUTTON_RANDOM = 7,
	BUTTON_MUTE = 8,
	BUTTON_VOLUME = 9,
	BUTTON_PROGRESS = 10,      // drawn only (no entry in m_buttons)
	BUTTON_NONE = 11,
};

// One button. 0x18 bytes.
struct SPlayerButton
{
	int id;                    // +0x00 PlayerButton
	bool enabled;              // +0x04 drawn faded and skipped when not
	bool on;                   // +0x05 random/mute: checked
	float left;                // +0x08 the built-in layout (Show)
	float top;                 // +0x0C
	float right;               // +0x10
	float bottom;              // +0x14
};

// The soundtrack manager pop-up ("Manage Soundtracks"). 0xB8 bytes.
// TODO: decompile (0x000F091F-0x000F2F98)
class CAudioTrackOption : public CAppFrame
{
public:
	CAudioTrackOption();                                  // 0x000F2A10 (this in edi)
	virtual ~CAudioTrackOption();                         // 0x000F0E67
	virtual BOOL Init(CXBFontEx *font);                   // slot 1, 0x000F2F98
	virtual BOOL IsOpen();                                // slot 2, 0x00029984
	virtual BOOL Render();                                // slot 3, 0x000F091F
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // slot 4, 0x000F2EC6
	virtual void Show();                                  // slot 5, 0x000F148C

	BYTE m_reserved04[0xB4];
};

// 0x1EC bytes.
class CAudioGUI : public CAppFrame
{
public:
	CAudioGUI();                                          // 0x00134640 (this in edi)
	virtual ~CAudioGUI();                                 // 0x0013361E (deleting 0x00011502)
	// The textures, the menu and the buttons (once). E_FAIL without
	// MPlayer.png.
	virtual BOOL Init(CXBFontEx *font);                   // 0x00134283
	virtual BOOL IsOpen() { return m_bOpen; }             // 0x00130DD5
	virtual BOOL Render();                                // 0x00134853
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x00135A99
	// Opens the screen: the buttons' states and the built-in layout.
	virtual void Show();                                  // 0x00133A5D
	// Plays a file, or with "Browse Folder" chosen in the menu the songs
	// of a folder (the file manager calls it with a music file). (this in
	// ebx)
	BOOL OpenFile(const char *path);                      // 0x00135A18

	COptionMenu m_menu;                    // +0x004 START
	BOOL m_bMenuOpen;                      // +0x098
	CAppFrame *m_pDialog;                  // +0x09C never set
	BOOL m_bDialogOpen;                    // +0x0A0
	BOOL m_bNoDisc;                        // +0x0A4 the menu pops up when a disc arrives
	CFileBrowser *m_pBrowser;              // +0x0A8
	std::string m_strBrowsePath;           // +0x0AC where the browser starts (the last file's folder)
	BOOL m_bBrowserOpen;                   // +0x0C8
	CAudioTrackOption *m_pTrackOptions;    // +0x0CC
	BOOL m_bTrackOptionsOpen;              // +0x0D0
	int m_nSavedState;                     // +0x0D4 the music before OpenFile (MusicState)
	UINT m_nSavedSoundtrack;               // +0x0D8
	UINT m_nSavedSong;                     // +0x0DC
	BOOL m_bKeepMusic;                     // +0x0E0 always 1: B doesn't restore the saved music
	CSongListControl *m_pSongList;         // +0x0E4
	BOOL m_bSongListShown;                 // +0x0E8
	int m_unknownEC;                       // +0x0EC cleared only
	int m_unknownF0;                       // +0x0F0 cleared only
	UINT m_nListSong;                      // +0x0F4 the song the list's selection follows
	BOOL m_bInfoReady;                     // +0x0F8 UpdateSongInfo may run
	wchar_t m_szChannels[10];              // +0x0FC "Stereo", "Mono", "CD Stereo"
	wchar_t m_szBitRate[10];               // +0x110 "%d KBPS"
	wchar_t m_szSampleRate[10];            // +0x124 "%.1f KHz"
	wchar_t m_szLength[10];                // +0x138 "mm:ss"
	// The built-in layout (Show): a 390x135 window at (125, 40) in skin
	// coordinates, and a 390x265 song list under it. Only the unused
	// built-in renderer (0x00135089) and the song list's defaults use it.
	float m_fScaleX;                       // +0x14C screen/skin
	float m_fScaleY;                       // +0x150
	float m_fHeight;                       // +0x154
	float m_fWidth;                        // +0x158
	float m_fY;                            // +0x15C
	float m_fX;                            // +0x160
	float m_fInfoHeight;                   // +0x164 90
	float m_fTitleY;                       // +0x168
	float m_fTitleX;                       // +0x16C
	float m_fTimeY;                        // +0x170
	float m_fTimeX;                        // +0x174
	float m_fStateY;                       // +0x178
	float m_fStateX;                       // +0x17C
	float m_fMeterY;                       // +0x180
	float m_fMeterX;                       // +0x184
	float m_fMeterHeight;                  // +0x188
	float m_fMeterWidth;                   // +0x18C
	float m_fListY;                        // +0x190
	float m_fListX;                        // +0x194
	float m_fListHeight;                   // +0x198
	float m_fListWidth;                    // +0x19C
	float m_fTitleScale;                   // +0x1A0 font scales, in quarters
	float m_fInfoScale;                    // +0x1A4
	float m_fSmallScale;                   // +0x1A8
	float m_fMargin;                       // +0x1AC 10
	std::vector<SPlayerButton> m_buttons;  // +0x1B0 BUTTON_PREV..BUTTON_VOLUME
	BOOL m_bFileOpened;                    // +0x1C0 OpenFile played something
	BOOL m_bOpen;                          // +0x1C4
	IDirect3DTexture8 *m_pKeyboardTexture; // +0x1C8 the shared "Keyboard" panel texture
	IDirect3DTexture8 *m_pButtonTexture;   // +0x1CC <media>\MPlayer.png (owned)
	CXBFont *m_pFont;                      // +0x1D0
	UINT m_nButton;                        // +0x1D4 the focused button (BUTTON_NONE)
	UINT m_nButtonPos;                     // +0x1D8 its place in the skin's button order
	int m_unknown1DC;                      // +0x1DC
	UINT m_nSong;                          // +0x1E0 the song the info is for (MUSIC_NO_SONG)
	UINT m_nShownSong;                     // +0x1E4 its index in the soundtrack
	CAppSettings *m_pSettings;             // +0x1E8

private:
	// Every second: the song list follows the soundtrack and the playing
	// song, and the format texts are refreshed. (this on the stack)
	void UpdateSongInfo();                                // 0x00133728
	// "Play Disc" only with an audio CD in the drive. (this in esi)
	void ShowMenu();                                      // 0x00133A27
	void ShowTrackOptions();                              // 0x00134225 (this in esi)
	// files: pick a music file, else a folder. (this in edi)
	void ShowBrowser(BOOL files);                         // 0x00134741
	// Left/right: the next enabled button in the skin's order. (inlined)
	void MoveFocus(BOOL right);
	void ToggleSongList();
	void ChangeVolume(BOOL up);

	// Drawing (AudioGUIRender.cpp). texture is the skin's <ButtonMap> or
	// MPlayer.png; direction the shape's (the bars' fill direction).
	void DrawButton(IDirect3DTexture8 *texture, UINT id, float x, float y, float width, float height,
	                int direction);                       // 0x0013101F
	// A key with a caption on the "Keyboard" texture. Unused. (this in
	// ebx)
	void DrawKey(float x, float y, float width, float height, const wchar_t *text, bool enabled, bool pressed,
	             bool focused);                           // 0x001333BA
};

// A list that's only ever cleared (UpdateSongInfo would fill in a
// line's time). 0x3C bytes.
struct SPlayerTrack
{
	std::wstring name;          // +0x00
	std::wstring time;          // +0x1C
	UINT unknown38;             // +0x38
};
extern std::vector<SPlayerTrack> g_playerTracks;   // 0x00277710

// The song list's soundtrack when UpdateSongInfo last reset it
// (MUSIC_NO_SONG forces a reset).
extern UINT g_nPlayerSoundtrack;           // 0x00269BC8

// The level meter: 12 bars of DirectSound's output levels (dB, -60 to 0).
void DrawLevelMeter(float x, float y, float width, float height);   // 0x00132308
// The stop (0), play (1) and pause (2) icons, lit in that state. (kind
// in eax)
void DrawStateIcon(int kind, float x, float y, float width, float height);   // 0x001325B6
