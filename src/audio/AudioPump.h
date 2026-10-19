// The MS dashboard's audio streaming classes (RTTI CAudioBuf, CAudioPump,
// CFilePump, CWMAPump, XCDPlayer; 0x0007EFA5-0x00080ABC), which UnleashX
// reuses for its WAV and CD players. See AudioPump.cpp and CdAudio.cpp.
//
// CAudioBuf is a whole sound in one DirectSound buffer. CAudioPump streams:
// its buffer is cut into packets, each with a notification event; a thread
// refills a packet with Read() when the play cursor passes it. The
// subclasses supply Read(): a WAV file's data, a WMA decoder, or raw CD
// audio.
#pragma once

#include "../common/Platform.h"
#include "DSound.h"

// A whole sound in one buffer. 0x28 bytes.
class CAudioBuf
{
public:
	CAudioBuf();                                          // 0x00080012 (this in ecx)
	virtual ~CAudioBuf();                                 // 0x000805DE (deleting 0x00032824)
	virtual BOOL Play(BOOL loop);                         // 1, 0x00080566
	virtual void Stop();                                  // 2, 0x00080558
	// Seconds into the buffer.
	virtual float GetPosition();                          // 3, 0x000803B3
	// The buffer's length in seconds.
	virtual float GetDuration();                          // 4, 0x0008000A
	virtual BOOL IsPlaying();                             // 5, 0x00080503
	virtual void Pause(BOOL pause);                       // 6, 0x00080524
	// The block Read() filled last (for a level display); NULL here.
	virtual const BYTE *GetLastData() { return NULL; }    // 7, 0x00080007
	virtual DWORD GetPacketSize() { return 0; }           // 8, 0x00080004
	// Speaker routing. More than two mix bins switch the buffer to a
	// 6-channel (5.1) extensible format first. Fails above 2 channels.
	virtual HRESULT SetMixBins(const DSMIXBINS *mixBins); // 9, 0x00080411

	// The buffer for size bytes of this format, filled from data when
	// given. (this in eax, size in ebx)
	HRESULT Create(const WAVEFORMATEX *format, DWORD size, const void *data);   // 0x0008085E
	// The whole buffer, locked for writing / unlocked. (this in eax)
	void *Lock();                                         // 0x000805B8
	void Unlock(void *data);                              // 0x000805A2
	// Hundredths of a dB, clamped to -6400..0. (this in eax, volume in ecx)
	void SetVolume(LONG volume);                          // 0x000803EB

	BYTE m_bLoop;                          // +0x04
	BYTE m_bPaused;                        // +0x05
	DWORD m_dwPacketSize;                  // +0x08 the whole buffer here
	DWORD m_dwBytesPerSecond;              // +0x0C
	IDirectSoundBuffer *m_pBuffer;         // +0x10
	WAVEFORMATEX m_format;                 // +0x14 (0x12 bytes)
};

// CAudioPump::m_nState.
enum PumpState
{
	PUMP_STOPPED = 0,
	PUMP_STARTING = 1,    // filling the packets before playing
	PUMP_PLAYING = 2,
	PUMP_ENDING = 3,      // Read() came up short: playing out what's left
};

// A streamed sound. 0x58 bytes.
class CAudioPump : public CAudioBuf
{
public:
	CAudioPump();                                         // 0x0008082D
	virtual ~CAudioPump();                                // 0x00080789 (deleting 0x00032840)
	virtual BOOL Play(BOOL loop);                         // 1, 0x000801F0
	virtual void Stop();                                  // 2, 0x00080197
	// Seconds played, counting the buffer's wraps.
	virtual float GetPosition();                          // 3, 0x00080034
	virtual float GetDuration() { return 0.0f; }          // 4, 0x0007FFF4
	virtual BOOL IsPlaying() { return m_nState != PUMP_STOPPED; }   // 5, 0x0007FFFB
	virtual void Pause(BOOL pause);                       // 6, 0x00080152
	// Fills buffer; returns the bytes, 0 at the end, below 0 on errors.
	virtual int Read(void *buffer, int size) = 0;         // 10 (_purecall)
	// The stream ended. Nothing here.
	virtual void OnEnd() {}                               // 11, 0x00013D20

	// The packets, events, refill thread and buffer. stackSize: the
	// thread's; prefill: packets filled before playing starts. (this in
	// ebx, packets in eax)
	HRESULT Create(int packets, DWORD stackSize, const WAVEFORMATEX *format, DWORD packetSize, int prefill);   // 0x00080622

	HANDLE m_hThread;                      // +0x28
	HANDLE m_hStopEvent;                   // +0x2C
	int m_unknown30;                       // +0x30
	HANDLE *m_phPacketEvents;              // +0x34 one per packet, set when the play cursor passes it
	HANDLE m_hMutex;                       // +0x38
	DWORD m_dwLastPlayCursor;              // +0x3C
	int m_nWraps;                          // +0x40
	int m_nFilled;                         // +0x44 packets with data not yet played
	BYTE *m_pFilled;                       // +0x48 per packet
	int m_nState;                          // +0x4C PumpState
	int m_nPackets;                        // +0x50
	int m_nPrefill;                        // +0x54

private:
	static DWORD __stdcall ThreadProc(void *param);       // 0x000803A7
	// (this in eax)
	DWORD Run();                                          // 0x0008026C
	// Refills packet index; TRUE if Read() gave anything. (this in esi)
	BOOL FillPacket(int index);                           // 0x00080087
};

// A WAV file's data, streamed. 0x6C bytes.
class CFilePump : public CAudioPump
{
public:
	CFilePump();                                          // 0x00080ABC
	virtual ~CFilePump();                                 // 0x00080A9D (deleting 0x0003285C)
	// Stops and goes back to the data's start.
	virtual void Stop();                                  // 2, 0x00080603
	virtual float GetDuration() { return m_fDuration; }   // 4, 0x0007FFF0
	virtual const BYTE *GetLastData() { return m_pLastData; }   // 7, 0x0007FFEC
	virtual DWORD GetPacketSize() { return m_dwFilePacketSize; }   // 8, 0x0007FFE8
	// Quirk: reads to the end of the file, so chunks after the data play
	// as noise. Looping wraps to the data's start.
	virtual int Read(void *buffer, int size);             // 10, 0x0007FF79

	// Streams file (positioned at the data) of dataSize bytes in this
	// format: 4 packets of about 8 KB. (this in eax, format in edi)
	HRESULT Open(HANDLE file, DWORD dataSize, const WAVEFORMATEX *format);   // 0x00080A41

	HANDLE m_hFile;                        // +0x58
	const BYTE *m_pLastData;               // +0x5C
	float m_fDuration;                     // +0x60
	DWORD m_dwDataStart;                   // +0x64
	DWORD m_dwFilePacketSize;              // +0x68
};

struct IXMediaObject;

// A WMA file or soundtrack song through the XDK's WMA decoder. Nothing
// makes one (the WMA player uses XACT). 0x68 bytes.
class CWMAPump : public CAudioPump
{
public:
	virtual ~CWMAPump();                                  // 0x000809DA (deleting 0x00032878)
	virtual void Stop() { CAudioPump::Stop(); }           // 2, 0x000805FE
	virtual float GetDuration() { return m_fDuration; }   // 4, 0x0007FF14
	virtual const BYTE *GetLastData() { return m_pLastData; }   // 7, 0x0007FF10
	virtual DWORD GetPacketSize() { return 0x10000; }     // 8, 0x0007FF0A
	virtual int Read(void *buffer, int size);             // 10, 0x0007FF18

	// A file (path), or an open handle (path NULL). (this in eax, path in
	// edx)
	HRESULT OpenFile(const char *path, HANDLE file, WAVEFORMATEX *format);   // 0x000808B8
	HRESULT OpenSoundtrackSong(DWORD songId, WAVEFORMATEX *format);   // 0x00080951

	HANDLE m_hFile;                        // +0x58
	IXMediaObject *m_pDecoder;             // +0x5C
	const BYTE *m_pLastData;               // +0x60
	float m_fDuration;                     // +0x64
};
