// A sound effect: a WAV loaded into a DirectSound buffer. TODO: decompile
#pragma once

#include "../common/Platform.h"

struct IDirectSoundBuffer
{
	HRESULT SetCurrentPosition(DWORD position);
	HRESULT SetVolume(LONG volume);
};

class CSound
{
public:
	HRESULT Play(DWORD flags);                // 0x00094A84

	BYTE m_unknown00[0x48];
	IDirectSoundBuffer *m_pBuffer;            // +0x48
};
