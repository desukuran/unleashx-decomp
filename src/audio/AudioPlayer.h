// The music players (RTTI CAudioPlayer and its subclasses CWMAPlayer,
// CMP3Player, CWAVPlayer, CDDAPlayer). Only the interface CMusicManager uses
// is declared; the players aren't decompiled yet. TODO: decompile
#pragma once

#include "../common/Platform.h"

// The playing stream's format.
struct SSongFormat
{
	int channels;          // +0x00
	DWORD bitRate;         // +0x04 bits/s
	DWORD sampleRate;      // +0x08 Hz
	DWORD duration;        // +0x0C seconds
};

// CAudioPlayer::GetType(), also the song records' type.
enum AudioType
{
	AUDIO_NONE = 0,
	AUDIO_WMA = 1,         // also the Xbox soundtracks
	AUDIO_MP3 = 2,
	AUDIO_WAV = 3,
	AUDIO_CD = 4,
};

// 26 virtual slots; the ones nothing here calls are placeholders.
class CAudioPlayer
{
public:
	virtual ~CAudioPlayer();                                          // slot 0
	// Set-up; the path is the data folder's sample.xsb. TODO: verify
	virtual HRESULT Init(const char *path);                           // slot 1
	virtual BOOL IsPlaying();                                         // slot 2
	virtual HRESULT Open(const char *file);                           // slot 3
	// An Xbox soundtrack song (XDK soundtrack and song IDs).
	virtual HRESULT OpenSoundtrackSong(DWORD soundtrackId, DWORD songId, DWORD index);   // slot 4
	virtual HRESULT Play();                                           // slot 5
	virtual HRESULT Stop();                                           // slot 6
	virtual HRESULT Pause();                                          // slot 7
	virtual void Unknown8();
	virtual void Unknown9();
	virtual void Unknown10();
	virtual void Unknown11();
	virtual void Unknown12();
	virtual HRESULT SetVolume(float volume);                          // slot 13 hundredths of a dB
	virtual void Unknown14();
	// The song's name (from its tags) and length (ms). TODO: verify unknown
	virtual HRESULT GetSongInfo(int unknown, wchar_t *name, DWORD *length);   // slot 15
	virtual void Unknown16();
	virtual void Unknown17();
	virtual void Unknown18();
	virtual void Unknown19();
	virtual void Unknown20();
	virtual float GetPosition();                                      // slot 21 seconds
	virtual void Unknown22();
	// Feeds the decoder; negative once the song is over (or failed).
	virtual HRESULT Process();                                        // slot 23
	virtual int GetType();                                            // slot 24 AudioType
	virtual void SetSpeakerConfig(const void *config);                // slot 25

	SSongFormat m_format;                                             // +0x04
};

class CWMAPlayer : public CAudioPlayer
{
public:
	CWMAPlayer();          // 0x000D8F85 (this in esi), 0x1D8 bytes
};

class CMP3Player : public CAudioPlayer
{
public:
	CMP3Player();          // 0x001096D3 (this in esi), 0x1B8 bytes
};

class CWAVPlayer : public CAudioPlayer
{
public:
	CWAVPlayer();          // 0x0013062D (this in esi), 0x168 bytes
};

class CDDAPlayer : public CAudioPlayer
{
public:
	CDDAPlayer();          // 0x00130AF7 (this in esi), 0x168 bytes
};

// The audio CD's table of contents (the DVD drive opened for CD audio).
struct SCdToc
{
	HANDLE hDevice;        // +0x00 INVALID_HANDLE_VALUE when closed
	int nTracks;           // +0x04
	DWORD trackSector[100];   // +0x08 each track's first sector, then the lead-out TODO: verify the size
};
extern SCdToc g_cdToc;     // 0x00278748

// Reads raw CD audio (0x930-byte sectors) from the TOC's drive, 16
// sectors at a time (also XCDPlayer's stream). TODO: decompile
struct SCddaReader
{
	BYTE *buffer;          // +0x00 0x9300 bytes of contiguous memory
	int bufferPos;         // +0x04 how much of the buffer was handed out
	SCdToc *toc;           // +0x08
	int retries;           // +0x0C
	DWORD sector;          // +0x10 the next one to read (up to the lead-out)
};
// (reader in esi, toc in ecx)
void CddaReaderOpen(SCddaReader *reader, SCdToc *toc, int retries);   // 0x0007FD6E
void CddaReaderClose(SCddaReader *reader);                            // 0x0007FD61 (reader in eax)
// Up to size bytes: the count, 0 at the lead-out, below 0 when a read
// fails. (reader in ebx)
int CddaReaderRead(SCddaReader *reader, void *buffer, int size);      // 0x0007FE11

// Opens the drive and reads the TOC. (toc in eax) TODO: decompile
HRESULT CdTocOpen(SCdToc *toc);          // 0x0007F5FB
void CdTocClose(SCdToc *toc);            // 0x0007F46D (toc in esi) TODO: decompile
// The length of a track (0-based: index nTracks - 1 is the last). Any
// output can be NULL. (track in edx, minutes in esi, seconds in edi,
// frames in ebx) TODO: decompile
BOOL CdTocGetTrackLength(SCdToc *toc, int track, DWORD *minutes, DWORD *seconds, DWORD *frames);   // 0x0007F4A4
