// A sound in one DirectSound buffer: the XDK samples framework's CXBSound
// (XBSound.cpp, with its CWaveFile; 0x00094699-0x0009511D). Library code:
// declared, not reconstructed. No vtable; 0x4C bytes.
#pragma once

#include "../common/Platform.h"
#include "DSound.h"

class CSound
{
public:
	CSound();                                 // 0x00094699 (this in eax)
	~CSound();                                // (Destroy and delete: 0x0002B63D)
	// Loads a WAV file. (this on the stack)
	HRESULT Create(const char *file, DWORD flags);   // 0x0009511D
	void Destroy();                           // 0x00094AA3 (this in esi)
	HRESULT Play(DWORD flags);                // 0x00094A84 (this in eax)
	HRESULT Stop();                           // 0x00094A70 (this in eax)

	BYTE m_waveFile[0x28];                    // +0x00 CWaveFile
	DSBUFFERDESC m_dsbd;                      // +0x28 (0x18 bytes)
	DWORD m_dwBufferSize;                     // +0x40
	void *m_pSampleData;                      // +0x44
	IDirectSoundBuffer *m_pBuffer;            // +0x48
};

// Plays the sound from the start; NULL (or no buffer) is ignored. The
// original takes it in esi. (Sound.cpp)
void PlaySound(CSound *sound);                // 0x0009F12E
