// The MP3 decoding behind CMP3Player (RTTI CAudioDecoder and CDirectSound,
// 0x0006E184-0x0006F78A): libmad frames, dithered to 16-bit PCM (madplay's
// linear dither) and written into a looping DirectSound buffer. See
// Mp3Decoder.cpp.
#pragma once

#include "../common/Platform.h"
#include "../thirdparty/Mad.h"
#include "DSound.h"

class IFile;

// A streaming 16-bit sound buffer, written at a cursor ahead of the play
// cursor. vtable 0x0025C7EC, 0x38 bytes.
class CDirectSound
{
public:
	// bufferSize: at least two seconds' worth (and 0x14000 bytes). (this
	// on the stack, sampleRate in ecx, bitsPerSample in dx)
	CDirectSound(DWORD sampleRate, WORD channels, DWORD bufferSize, WORD bitsPerSample);   // 0x0006F63C
	virtual ~CDirectSound();                              // 0x0006F61B (deleting 0x00033C04)

	BOOL IsValid() const { return m_bValid; }             // 0x0006F1CA (this in eax)
	// (this in esi)
	void Stop();                                          // 0x0006F1CE
	// Hundredths of a dB; -6400 (mute) isn't remembered. (this in eax,
	// volume in ecx)
	void SetVolume(LONG volume);                          // 0x0006F203
	void Mute(BOOL mute);                                 // 0x0006F47F (this in eax)
	HRESULT SetMixBins(const DSMIXBINS *mixBins);         // 0x0006F224 (this in eax)
	BOOL IsPlaying();                                     // 0x0006F368 (this in eax)
	// Bytes written and not yet played. (this in esi)
	DWORD GetQueued();                                    // 0x0006F38E
	void Play();                                          // 0x0006F3EE (this in esi)
	void Pause();                                         // 0x0006F454 (this in esi)
	// Silence in the whole buffer, the write position back to 0. (this in
	// esi)
	void Clear();                                         // 0x0006F496
	void Write(const void *data, DWORD size);             // 0x0006F507
	// The play cursor (and the bytes queued). The first call after
	// Create starts at 0 instead and reports the whole buffer queued.
	// (this in eax, queued in edi)
	void GetPlayPosition(DWORD *play, DWORD *queued);     // 0x0006F78A

	DWORD m_dwWriteCursor;                 // +0x04 DirectSound's; -1 until the first GetPlayPosition
	WAVEFORMATEX m_format;                 // +0x08
	DWORD m_dwBufferSize;                  // +0x1C
	DWORD m_dwTotalWritten;                // +0x20
	BOOL m_bPlaying;                       // +0x24 (a BYTE)
	DWORD m_dwWritePos;                    // +0x28
	BOOL m_bValid;                         // +0x2C (a BYTE)
	LONG m_lVolume;                        // +0x30
	IDirectSoundBuffer *m_pBuffer;         // +0x34
};

// The Xing VBR header in the first frame (madplay's). 0x74 bytes.
struct SXingHeader
{
	DWORD flags;           // +0x00 XING_*
	DWORD frames;          // +0x04
	DWORD bytes;           // +0x08
	BYTE toc[100];         // +0x0C
	DWORD scale;           // +0x70
};
#define XING_FRAMES 0x01
#define XING_BYTES 0x02
#define XING_TOC 0x04
#define XING_SCALE 0x08

// An MP3 file playing. vtable 0x0025C7F4, 0x596C bytes.
class CAudioDecoder
{
public:
	CAudioDecoder();                                      // 0x0006E184 (this in eax)
	// Stops and closes. (Slot 0 isn't the destructor.)
	virtual void Stop();                                  // 0, 0x0006F16C
	virtual BOOL IsPlaying();                             // 1, 0x0006E4FD
	// Fills the buffer ahead and starts it.
	virtual void Start();                                 // 2, 0x0006E44D
	virtual ~CAudioDecoder();                             // 3, 0x0006ED7C (deleting 0x00033C20)
	// The ms of audio queued in the buffer (not the position).
	virtual ULONGLONG GetQueuedTime();                    // 4, 0x0006E3F1
	// Opens path: the first frame's header, the length (from a Xing
	// header or the file size), the buffers. skipId3: skip an ID3v2 tag.
	virtual BOOL Open(const char *path, BOOL skipId3);    // 5, 0x0006EDE1
	// Decodes until the buffer holds three output blocks; FALSE once
	// the song has played out (or nothing is open).
	virtual BOOL Decode();                                // 6, 0x0006E603

	void Pause();                                         // 0x0006E524 (this in eax)
	void Resume();                                        // 0x0006E4CE (this in ecx)
	// (this in esi, volume in edi)
	void SetVolume(LONG volume);                          // 0x0006E53F
	HRESULT SetMixBins(const DSMIXBINS *mixBins);         // 0x0006E557 (this in eax)

	DWORD m_dwDuration;                    // +0x0004 seconds
	DWORD m_dwBitRate;                     // +0x0008 kbps
	DWORD m_dwSampleRate;                  // +0x000C
	BOOL m_bStereo;                        // +0x0010
	int m_unknown14;                       // +0x0014 1 once open
	SXingHeader m_xing;                    // +0x0018
	int m_unknown8C;                       // +0x008C
	mad_stream m_stream;                   // +0x0090
	mad_frame m_frame;                     // +0x00D0
	mad_synth m_synth;                     // +0x2504
	mad_timer_t m_timer;                   // +0x5910 time decoded
	BYTE *m_pReadBuffer;                   // +0x5918 0x20000 bytes + the guard, contiguous memory
	BYTE *m_pOutBuffer;                    // +0x591C m_dwOutSize bytes of PCM
	BYTE *m_pOut;                          // +0x5920
	BYTE *m_pGuard;                        // +0x5924 after the last read, at the end of the file
	BYTE *m_pOutEnd;                       // +0x5928
	int m_nEof;                            // +0x592C 1 at the end of the file
	int m_nFrames;                         // +0x5930
	DWORD m_dwBytesRead;                   // +0x5934
	IFile *m_pFile;                        // +0x5938
	DWORD m_unknown593C;                   // +0x593C 0xA8B9FF7E
	BOOL m_bDraining;                      // +0x5940 the file is done; playing out
	DWORD m_dwEndPosition;                 // +0x5944 where the data ends in the buffer
	int m_unknown5948;                     // +0x5948 cleared only
	DWORD m_dwLastPlayCursor;              // +0x594C
	DWORD m_dwOutSize;                     // +0x5950 a second of output (8 KB for short files)
	bool m_bStartedPlaying;                // +0x5954
	bool m_bActive;                        // +0x5955
	bool m_bFirst;                         // +0x5956
	CDirectSound *m_pSound;                // +0x5958
	BYTE m_unknown595C[8];                 // +0x595C
	LONG m_lVolume;                        // +0x5964
	bool m_unknown5968;                    // +0x5968 cleared only
	bool m_bPaused;                        // +0x5969
	bool m_bDecoding;                      // +0x596A Decode is running

private:
	// Clears everything for a new file. (this in esi)
	void Reset();                                         // 0x0006ECDC
	// Frees libmad's state, the sound and the file. (this in eax)
	void Close();                                         // 0x0006EC95
	void FreeDecoder();                                   // 0x0006E56E
	// Decodes ahead until the buffer holds a block. (this in edi)
	BOOL Prefill();                                       // 0x0006E3A8
	// Writes the output block to the sound.
	void FlushOutput(DWORD size);
};

// The first (or Xing) frame's header; xing gets the Xing header if the
// first frame has one. -1 when no frame was found. (xing in ecx)
int FindFirstFrame(SXingHeader *xing, IFile *file, mad_header *header);   // 0x0006E281
