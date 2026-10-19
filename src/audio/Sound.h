// A sound effect: a WAV loaded into a DirectSound buffer. TODO: decompile
#pragma once

#include "../common/Platform.h"

#ifndef _XBOX
struct IDirectSoundBuffer
{
	HRESULT SetCurrentPosition(DWORD position);
	HRESULT SetVolume(LONG volume);
	HRESULT Stop();
};
#define DSBPLAY_LOOPING 0x00000001
#endif

class CSound
{
public:
	virtual ~CSound();
	HRESULT Play(DWORD flags);                // 0x00094A84

	BYTE m_unknown04[0x44];
	IDirectSoundBuffer *m_pBuffer;            // +0x48
};

// Plays the sound from the start; NULL is ignored. The original takes it in esi.
void PlaySound(CSound *sound);                // 0x0009F12E TODO: decompile
