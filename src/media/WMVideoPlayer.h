// WMV/XMV video playback (RTTI CWMVideoPlayer, 0x0009BB70-0x0009D2F0):
// skin backgrounds and previews, the intro movie, the screen saver's video
// and the file manager's video viewer. The decoding is done by two XDK
// sample classes (declared below, not reconstructed): the WMV sample's
// player for .wmv, and xmvhelper's CXMVPlayer for .xmv. See
// WMVideoPlayer.cpp.
#pragma once

#include <string>

#include "../app/Screens.h"
#include "../common/Platform.h"

class CAppSettings;
class CMessageBox;
class CXBFont;
class CXBFontEx;
struct IDirect3DTexture8;
struct IDirect3DSurface8;
struct IDirectSound;
struct SGamepadState;

// What the WMV decoder reports. 0x14 bytes.
struct SWmvVideoInfo
{
	DWORD width;           // +0x00
	DWORD height;          // +0x04
	DWORD unknown08;       // +0x08
	DWORD unknown0C;       // +0x0C
	DWORD bitRate;         // +0x10 bits/s
};

// The XDK WMV playback sample's player (WMVDEC). Library code. 0x180
// bytes.
class CWmvSamplePlayer
{
public:
	CWmvSamplePlayer();                                   // 0x0009BB31 (this in esi)
	~CWmvSamplePlayer();                                  // (deleting 0x00031BBC: Destroy, then delete)
	// The sound device and the 512 KB audio buffer. (this in esi,
	// directSound in eax)
	void Initialize(IDirectSound *directSound);           // 0x0009B819
	void Destroy();                                       // 0x0009B840 (this in eax)
	// format: the frame texture's D3DFORMAT.
	HRESULT OpenFile(const char *path, DWORD format);     // 0x0009B8D6 (this on the stack)
	void CloseFile();                                     // 0x0009B7C2
	// (this in eax, info in edx)
	HRESULT GetVideoInfo(SWmvVideoInfo *info);            // 0x0009B564
	// Decodes the next frame; 1 at the end of the file. (this in esi)
	HRESULT DecodeNext();                                 // 0x0009B589
	// TRUE when the decoded frame is due. (this in esi)
	BOOL IsFrameDue();                                    // 0x0009B737
	// Copies the decoded frame into texture. (this in ebx)
	HRESULT CopyFrame(IDirect3DTexture8 *texture);        // 0x0009B86D

	BYTE m_unknown00[4];                   // +0x000
	int m_nLoops;                          // +0x004 -1: loop
	bool m_bAudio;                         // +0x008
	bool m_bHasAudio;                      // +0x009
	BYTE m_unknown0A[0x16E];
	BOOL m_bFrameReady;                    // +0x178
	CAppSettings *m_pSettings;             // +0x17C
};

// The XDK samples' xmvhelper CXMVPlayer (Video/XMVCompare/xmvhelper.cpp),
// modified by UnleashX: the first 0x10 bytes are its additions (settings,
// the audio stream kept for the music volume, the audio and loop switches;
// a finished movie restarts when looping), and the device argument and
// member are gone. Library code otherwise. 0x98 bytes. Its packet callbacks
// are named XMVHelper_* in names.txt; OpenFileForPackets (0x0008F593) is
// linked in but unused.
class CXMVPlayer
{
public:
	CXMVPlayer();                                         // 0x0008F777
	~CXMVPlayer();                                        // (deleting 0x00031BDD)
	void Destroy();                                       // 0x0008F286
	HRESULT OpenFile(const char *path, DWORD format, BOOL allocateTextures);   // 0x0008F6DD
	HRESULT OpenMovieFromMemory(const char *path, DWORD format, BOOL allocateTextures);   // 0x0008F4DB
	void EnableOverlays();                                // 0x0008F15B
	IDirect3DTexture8 *AdvanceFrameForTexturing();        // 0x0008F21C
	IDirect3DTexture8 *AdvanceFrameForOverlays();         // 0x0008F18D
	// (this in eax, loop in cl)
	void SetLoop(BOOL loop);                              // 0x00031B85

	CAppSettings *m_pSettings;             // +0x00 (UnleashX)
	void *m_pAudioStream;                  // +0x04 IDirectSoundStream: audio stream 0, for its volume (UnleashX)
	BOOL m_bHasAudio;                      // +0x08 the file has audio streams (UnleashX)
	bool m_bAudio;                         // +0x0C play the audio (UnleashX)
	bool m_bLoop;                          // +0x0D (UnleashX)
	void *m_pXMVDecoder;                   // +0x10 XMVDecoder
	// XMVVIDEO_DESC
	DWORD m_dwWidth;                       // +0x14
	DWORD m_dwHeight;                      // +0x18
	DWORD m_dwFramesPerSecond;             // +0x1C
	DWORD m_dwAudioStreamCount;            // +0x20
	BYTE m_AudioDesc[0x14];                // +0x24 XMVAUDIO_DESC
	IDirect3DTexture8 *m_pTextures[2];     // +0x38
	DWORD m_nDecodeTextureIndex;           // +0x40
	IDirect3DTexture8 *m_pShowingTexture;  // +0x44
	IDirect3DTexture8 *m_pDecodingTexture; // +0x48
	IDirect3DTexture8 *m_pSubmittedTexture;   // +0x4C
	int m_dwCurrentFrame;                  // +0x50 -1 before the first
	BOOL m_bPlaying;                       // +0x54
	BOOL m_bOverlaysEnabled;               // +0x58
	BYTE m_loadContext[0x38];              // +0x5C LOAD_CONTEXT (file handle +0x6C, memory buffer +0x84)
	BYTE *m_physicalBuffer;                // +0x94
};

// A video. Modal (the file manager's viewer: in a window with its name and
// format, error messages, the hardware overlay) or not (drawn as a
// texture). vtable 0x0025AFD4, 0xF0 bytes.
class CWMVideoPlayer : public CAppFrame
{
public:
	CWMVideoPlayer();                                     // 0x0009D28C (this in esi)
	virtual ~CWMVideoPlayer();                            // 0x0009C821 (deleting 0x00031B9F)
	// The font (NULL: "SmallFont") and the window; once.
	virtual BOOL Init(CXBFontEx *font);                   // 1, 0x0009C951
	virtual BOOL IsOpen();                                // 2, 0x0009BF4C still playing
	virtual BOOL Render();                                // 3, 0x0009C436
	// Modal: A/B/BLACK/BACK stop, the right stick click or DISPLAY
	// toggles full screen, and the action is swallowed. Otherwise only a
	// non-looping video (the intro) can be skipped with A/START. Then the
	// next frame is decoded when due.
	virtual HRESULT ProcessInput(int *action, SGamepadState **pad);   // 4, 0x0009C26C

	// (Inlined: Init with a font.)
	void SetFont(CXBFont *font) { Init((CXBFontEx *)font); }
	// (this in eax, fullScreen in ecx)
	void SetFullScreen(BOOL fullScreen);                  // 0x0009BDF1
	// (this in eax, enable in dl)
	void SetAudio(BOOL enable);                           // 0x0009BF89
	BOOL HasAudio();                                      // 0x0009BF69 (this in eax)
	// Where a windowed (skin preview) video goes: x, y, width, height in
	// skin coordinates; a zero size uses the default. (this on the stack,
	// width in ecx, height in edx)
	void SetRect(int top, int left, int width, int height);   // 0x0009BB70
	// Opens the file and its player here; TRUE when it worked.
	BOOL Open(const char *fileName);                      // 0x0009CE46
	// Opens the file on a thread.
	void Reopen(const char *fileName);                    // 0x0009D050
	void Close();                                         // 0x0009C224 (this in edi)
	// FALSE once a file failed to open. (this in eax)
	BOOL IsValid() { return m_bOpen || !m_bReady; }       // 0x0002653E
	// (this in esi, loop in cl)
	void SetLoop(BOOL loop);                              // 0x0009C1F3

	BOOL m_bPlaying;                       // +0x04
	CMessageBox *m_pMessageBox;            // +0x08
	BOOL m_bMessageShown;                  // +0x0C
	std::string m_strFile;                 // +0x10
	BOOL m_bInitialized;                   // +0x2C
	float m_fLineHeight;                   // +0x30
	CXBFont *m_pFont;                      // +0x34
	CXBFont *m_pTitleFont;                 // +0x38 the settings' dialog font, else m_pFont
	CWmvSamplePlayer *m_pWmv;              // +0x3C
	CXMVPlayer *m_pXmv;                    // +0x40
	BOOL m_bXmv;                           // +0x44
	IDirect3DTexture8 *m_pTextures[2];     // +0x48 the frames (alternating)
	IDirect3DSurface8 *m_pSurfaces[2];     // +0x50 their surfaces, for the overlay
	UINT m_nCurrent;                       // +0x58 the frame being filled
	int m_unknown5C;                       // +0x5C
	int m_nFrames;                         // +0x60
	int m_unknown64;                       // +0x64 -1
	BOOL m_bFrameReady;                    // +0x68
	BOOL m_bOpen;                          // +0x6C a file is open
	bool m_bAudio;                         // +0x70
	bool m_bLoop;                          // +0x71
	BOOL m_bCustomRect;                    // +0x74 SetRect was given a size
	BOOL m_bYuv;                           // +0x78 frames are YUV
	float m_fWindowY;                      // +0x7C the modal window: 20, 30, 580 x 440 at 640x480
	float m_fWindowX;                      // +0x80
	float m_fWindowWidth;                  // +0x84
	float m_fWindowHeight;                 // +0x88
	BYTE m_unknown8C[0x10];                // +0x8C
	BOOL m_bFullScreen;                    // +0x9C
	BOOL m_bModal;                         // +0xA0 1 by default
	SWmvVideoInfo m_info;                  // +0xA4
	RECT m_rcSource;                       // +0xB8 the frame
	// The destination. Windowed: x, y, width, height (SetRect); modal: a
	// RECT fitted into the window (SetFullScreen(FALSE)).
	RECT m_rcDest;                         // +0xC8
	RECT m_rcScreen;                       // +0xD8 the screen area
	BOOL m_bReady;                         // +0xE8 not opening
	CAppSettings *m_pSettings;             // +0xEC

private:
	// Creates the player and opens m_strFile (also run as Reopen's
	// thread). (this on the stack)
	static unsigned __stdcall OpenThread(void *param);    // 0x0009CAA9
	// A modal message box. (this in edi)
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x0009CA13
	// Draws a frame texture as a quad (the skin's videos). (this in ebx)
	void DrawFrame(IDirect3DTexture8 *texture, int x, int y, int width, int height);   // 0x0009BFAF
	// Frees the player, the textures and surfaces before an open.
	BOOL PrepareOpen(const char *fileName);
};

// The videos open with the overlay on (the overlay is off when none is).
extern int g_nOverlayUsers;   // 0x0027765C
