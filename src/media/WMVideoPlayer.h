// WMV/XMV video playback (skin backgrounds, previews, intro movie).
#pragma once

#include "../common/Platform.h"

class CXBFont;

class CWMVideoPlayer
{
public:
	CWMVideoPlayer();                                     // TODO: decompile
	virtual ~CWMVideoPlayer();
	virtual void SetFont(CXBFont *font);                  // vtable slot 1, 0x0009C951 TODO: decompile
	virtual BOOL IsPlaying();                             // slot 2, 0x0009BF4C
	virtual void Render();                                // slot 3, 0x0009C436
	virtual HRESULT Update(BOOL *busy, int unknown);      // slot 4, 0x0009C26C TODO: decompile

	void SetFullScreen(BOOL fullScreen);                  // 0x0009BDF1 TODO: decompile
	void SetAudio(BOOL enable);                           // 0x0009BF89 TODO: decompile
	BOOL HasAudio();                                      // 0x0009BF69 TODO: decompile (this in eax)
	void SetRect(int top, int left, int width, int height);   // 0x0009BB70 TODO: decompile
	BOOL Open(const char *fileName);                      // 0x0009CE46 TODO: decompile
	void Unknown9C1F3(BOOL value);                        // 0x0009C1F3 TODO: decompile (this in esi, value in cl)

	BYTE m_reserved04[0x9C];
	BOOL m_bUnknownA0;                                    // +0xA0 cleared for the intro movie
	BYTE m_reservedA4[0x4C];                              // size 0xF0
};
