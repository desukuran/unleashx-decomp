// CSongListControl: the scrolling song list of the music player (the right
// stick click shows it) and of the CD/soundtrack tools (CTrackCopy,
// CTrackEditor). It lists the music manager's current soundtrack, the
// audio CD's tracks, or the songs of one Xbox soundtrack (one more each
// frame). See SongList.cpp.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"

class CXBFont;
struct IDirect3DTexture8;
struct SGamepadState;

// One line. 0x44 bytes.
struct SSongItem
{
	std::wstring name;          // +0x00 "<n>. <title>"
	std::wstring time;          // +0x1C "mm:ss", empty until known
	UINT index;                 // +0x38 the song's index in its soundtrack
	BOOL marked;                // +0x3C the playing song (player), or ticked (CTrackCopy)
	DWORD length;               // +0x40 ms
};

// 0x6C bytes.
class CSongListControl
{
public:
	CSongListControl();                                   // 0x000F12A3 (this in edi)
	virtual ~CSongListControl();                          // 0x000F1057 (deleting 0x000299D3)

	// The font and the box (skin coordinates; the size is scaled when
	// drawn). E_FAIL without a font. (this in esi, font in eax)
	HRESULT Init(CXBFont *font, float x, float y, float width, float height);   // 0x000F1D2F
	// A new box: zeros keep the current value; margins right/bottom of
	// 0 or less copy left/top. texture: the skin's <PlayList> picture;
	// NULL draws the built-in panel (with 5-pixel margins). (this in ebx,
	// texture in edx, height in xmm1)
	void SetLayout(IDirect3DTexture8 *texture, float height, float x, float y, float width, float marginLeft,
	               float marginTop, float marginRight, float marginBottom);   // 0x000F1BCE
	HRESULT Render();                                     // 0x000F3E6A (this in ebx)
	// Up/down (and the triggers, the left stick) move the selection.
	// Always returns S_OK. (this in esi, action in ebx, pad in ecx)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x000F0BEC
	// (this in esi, index in edi)
	void SetSelection(UINT index);                        // 0x000F0BB6
	// Another soundtrack (the music manager's index, or an Xbox soundtrack
	// ID) empties the list. (this in edi, soundtrack in eax)
	void SetSoundtrack(int soundtrack);                   // 0x000F0EB3
	// Marks the line of the song with this index (and fills in its time
	// if the music manager knows it). (this in edi, index in eax, marked
	// in ebx)
	void SetMarked(UINT index, BOOL marked);              // 0x000F124B
	void ClearMarks();                                    // 0x000F1273 (this in edi)
	void ToggleMark(UINT index);                          // 0x000F0B92 (this in edi, index in eax)
	// Empties the list to fill it again from ST.DB after an edit. XAPI
	// keeps the soundtrack it read last, so another soundtrack's first
	// song is read to make it read ours again. (this in ecx)
	void Reload();                                        // 0x000F0FBD
	// The selected line's text. (this in ecx)
	const wchar_t *GetSelectedName();                     // 0x000F0C97

	CXBFont *m_pFont;                      // +0x04
	BOOL m_bInitialized;                   // +0x08
	int m_nSoundtrack;                     // +0x0C -1
	float m_fHeight;                       // +0x10
	float m_fWidth;                        // +0x14
	float m_fY;                            // +0x18
	float m_fX;                            // +0x1C
	float m_fScaleX;                       // +0x20 screen/skin, set by SetLayout
	float m_fScaleY;                       // +0x24
	float m_fMarginLeft;                   // +0x28 (scaled)
	float m_fMarginTop;                    // +0x2C
	float m_fMarginRight;                  // +0x30
	float m_fMarginBottom;                 // +0x34
	float m_fFontScale;                    // +0x38 the font's scale when laid out
	BOOL m_bSkinTexture;                   // +0x3C m_pTexture is the skin's
	IDirect3DTexture8 *m_pTexture;         // +0x40 else the shared "Keyboard" texture
	UINT m_nTop;                           // +0x44 first line shown
	UINT m_nSelected;                      // +0x48
	UINT m_nLastVisible;                   // +0x4C last line that fit (0: not drawn yet)
	BOOL m_bMusicManager;                  // +0x50 list the music manager's soundtrack
	UINT m_nFilled;                        // +0x54 lines when the fill was done; -1 = refill
	UINT m_nCurrentSong;                   // +0x58 the music manager's song last seen (-1)
	std::vector<SSongItem> m_items;        // +0x5C

private:
	// Fills the list when m_nFilled isn't the line count. For an Xbox
	// soundtrack only song number next is added. (this on the stack)
	void Fill(UINT next);                                 // 0x000F2241
	// Every 2 s: the playing song's time, once the music manager knows it.
	// (this on the stack, index in eax)
	void UpdatePlayingTime(UINT index);                   // 0x000F3DA5
	// The time of the music manager's current song, when it changed.
	// (this in eax)
	void UpdateCurrentSongTime();                         // 0x000F0ED6
	void MoveUp();
	void MoveDown();
};

// The built-in panel background: the middle of a texture (0.1-0.9)
// stretched over the box, tinted by factor. (The original takes x in xmm5,
// y in xmm0; the music player has its own copy at 0x00132781, and
// CAudioGUI::DrawKey an inlined one with a factor.)
void DrawPanelTexture(IDirect3DTexture8 *texture, float x, float y, float width, float height,
                      DWORD factor = 0xFFFFFFFF);   // 0x000F0CAE
