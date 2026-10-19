// The music players (RTTI CAudioPlayer and its subclasses): CWMAPlayer
// (WMA files and Xbox soundtracks, through XACT), CMP3Player (libmad, see
// Mp3Decoder.h), CWAVPlayer and CDDAPlayer (the MS dashboard's audio pumps,
// AudioPump.h, CdAudio.h). CMusicManager owns one of each and drives the
// current one: Open, Play, then Process every frame until it fails.
// See WmaPlayer.cpp, Mp3Player.cpp, WavPlayer.cpp, CdPlayer.cpp;
// docs/MUSIC.md.
#pragma once

#include <string>

#include "../common/Platform.h"
#include "../common/XboxKernel.h"
#include "CdAudio.h"
#include "DSound.h"

class CAppSettings;
class CAudioBuf;
class CAudioDecoder;

// The playing stream's format.
struct SSongFormat
{
	int channels;          // +0x00
	DWORD bitRate;         // +0x04 bits/s (bytes/s for WAV and CD: quirk)
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

// CAudioPlayer::GetState().
enum PlayerState
{
	PLAYER_STOPPED = 0,
	PLAYER_PAUSED = 1,
	PLAYER_PLAYING = 2,
};

// The time spent playing, from the performance counter (the same fields
// at +0x128 in every player). (Inlined in each player.)
struct SPlayClock
{
	LARGE_INTEGER last;    // +0x00 when Update last ran
	LONGLONG played;       // +0x08 counts while playing
	LARGE_INTEGER frequency;   // +0x10 0 until queried

	void Start() { QueryPerformanceCounter(&last); }
	void Update()
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		played += now.QuadPart - last.QuadPart;
		last = now;
	}
	// Quirk: whole seconds (an integer division).
	float GetSeconds() const
	{
		if (frequency.QuadPart > 0)
			return (float)(played / frequency.QuadPart);
		return 0.0f;
	}
};

// The base: every slot fails (E_FAIL, 0.0) or is pure. vtable 0x0024A9E4,
// 26 slots.
class CAudioPlayer
{
public:
	virtual ~CAudioPlayer() {}                                        // 0 (0x000129AD)
	// Set-up; path is the data folder (the WMA player's XACT sound bank).
	virtual HRESULT Init(const char * /*path*/) { return E_FAIL; }    // 1 (0x00012990)
	// Set up and able to play. (Quirk: the WAV player also says so after
	// a failed Open.)
	virtual BOOL IsReady() = 0;                                       // 2
	virtual HRESULT Open(const char * /*file*/) { return E_FAIL; }    // 3 (0x00012990)
	// An Xbox soundtrack song (XDK soundtrack and song IDs; song 0: the
	// whole soundtrack).
	virtual HRESULT OpenSoundtrackSong(DWORD /*soundtrackId*/, DWORD /*songId*/, DWORD /*index*/) { return E_FAIL; }   // 4 (0x00012998)
	// S_FALSE when already playing.
	virtual HRESULT Play() { return E_FAIL; }                         // 5 (0x000129A7)
	// S_FALSE when already stopped.
	virtual HRESULT Stop() { return E_FAIL; }                         // 6 (0x000129A7)
	// S_FALSE unless playing.
	virtual HRESULT Pause() { return E_FAIL; }                        // 7 (0x000129A7)
	virtual int GetState() = 0;                                       // 8 PlayerState
	virtual HRESULT Unknown9() { return E_FAIL; }                     // 9 (0x00012990)
	virtual void Unknown10() = 0;                                     // 10
	virtual HRESULT Unknown11() { return E_FAIL; }                    // 11 (0x00012990)
	virtual void Unknown12() = 0;                                     // 12
	// Hundredths of a dB.
	virtual HRESULT SetVolume(float /*volume*/) { return E_FAIL; }    // 13 (0x00012990)
	virtual float GetVolume() { return 0.0f; }                        // 14 (0x000129A0)
	// The song's "artist" line (MP3: "album - artist"), title and length;
	// any can be NULL. The length is ms, plus 1 for MP3. (Quirk: the WAV
	// and CD players give seconds + 1.)
	virtual HRESULT GetSongInfo(wchar_t * /*artist*/, wchar_t * /*title*/, DWORD * /*length*/) { return E_FAIL; }   // 15 (0x00012998)
	virtual HRESULT Unknown16() { return E_FAIL; }                    // 16 (0x000129A7)
	virtual HRESULT Unknown17() { return E_FAIL; }                    // 17 (0x000129A7)
	virtual HRESULT Unknown18() { return E_FAIL; }                    // 18 (0x00012990)
	virtual HRESULT Unknown19() { return E_FAIL; }                    // 19 (0x000129A7)
	virtual HRESULT Unknown20() { return E_FAIL; }                    // 20 (0x000129A7)
	// Seconds played.
	virtual float GetPosition() { return 0.0f; }                      // 21 (0x000129A0)
	virtual void Unknown22() = 0;                                     // 22
	// Keeps the song going; below 0 once it's over (or failed).
	virtual HRESULT Process() { return E_FAIL; }                      // 23 (0x000129A7)
	virtual int GetType() = 0;                                        // 24 AudioType
	// The 5.1 speaker mix bins, NULL for stereo.
	virtual void SetSpeakerConfig(const DSMIXBINS * /*mixBins*/) {}   // 25 (0x0002B8DA)

	SSongFormat m_format;                                             // +0x04
};

// A WAV file: up to 64 KB in one buffer, longer ones streamed. vtable
// 0x0024AA64, 0x168 bytes.
class CWAVPlayer : public CAudioPlayer
{
public:
	CWAVPlayer();                                         // 0x0013062D (this in esi)
	virtual ~CWAVPlayer();                                // 0x001301EB (deleting 0x00012A01)
	virtual HRESULT Init(const char *path);               // 1, 0x00130125
	virtual BOOL IsReady() { return m_bReady; }           // 2, 0x00023462
	virtual HRESULT Open(const char *file);               // 3, 0x00130218
	virtual HRESULT Play();                               // 5, 0x00130062
	virtual HRESULT Stop();                               // 6, 0x0012FF6C
	virtual HRESULT Pause();                              // 7, 0x001300FF
	virtual int GetState() { return m_nState; }           // 8, 0x00023469
	virtual void Unknown10() {}
	virtual void Unknown12() {}
	virtual HRESULT SetVolume(float volume);              // 13, 0x001301CC
	virtual float GetVolume() { return (float)m_nVolume; }   // 14, 0x000129DD
	virtual HRESULT GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length);   // 15, 0x00130184
	virtual float GetPosition();                          // 21, 0x00130136
	virtual void Unknown22() {}
	virtual HRESULT Process();                            // 23, 0x0012FFA8
	virtual int GetType() { return AUDIO_WAV; }           // 24, 0x000129FD
	virtual void SetSpeakerConfig(const DSMIXBINS * /*mixBins*/) {}   // 25, 0x0012FFA5 (ignored)

	int m_unknown14;                       // +0x014
	int m_nState;                          // +0x018 PlayerState
	int m_unknown1C;                       // +0x01C
	int m_nVolume;                         // +0x020
	wchar_t m_szArtist[64];                // +0x024 never filled
	wchar_t m_szTitle[64];                 // +0x0A4 the file's name
	DWORD m_dwLength;                      // +0x124 seconds
	SPlayClock m_clock;                    // +0x128
	CAppSettings *m_pSettings;             // +0x140
	BOOL m_bReady;                         // +0x144
	BOOL m_bActive;                        // +0x148
	BOOL m_bStarted;                       // +0x14C the buffer was started
	CAudioBuf *m_pBuffer;                  // +0x150 a CAudioBuf or a CFilePump
	WAVEFORMATEX m_wfx;                    // +0x154 (0x14 bytes read from the file)
};

// An audio CD track (the "file" is the track number). vtable 0x0024A974,
// 0x168 bytes.
class CDDAPlayer : public CAudioPlayer
{
public:
	CDDAPlayer();                                         // 0x00130AF7 (this in esi)
	virtual ~CDDAPlayer();                                // 0x0013095E (deleting 0x000129E1)
	virtual HRESULT Init(const char *path);               // 1, 0x001307D1
	virtual BOOL IsReady() { return m_bReady; }           // 2, 0x00023462
	// file: the track number (1-based) as text.
	virtual HRESULT Open(const char *file);               // 3, 0x0013098B
	virtual HRESULT Play();                               // 5, 0x0013070E
	virtual HRESULT Stop();                               // 6, 0x00130699
	virtual HRESULT Pause();                              // 7, 0x001307AB
	virtual int GetState() { return m_nState; }           // 8, 0x00023469
	virtual void Unknown10() {}
	virtual void Unknown12() {}
	virtual HRESULT SetVolume(float volume);              // 13, 0x00130878
	virtual float GetVolume() { return (float)m_nVolume; }   // 14, 0x000129DD
	virtual HRESULT GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length);   // 15, 0x00130830
	virtual float GetPosition();                          // 21, 0x001307E2
	virtual void Unknown22() {}
	virtual HRESULT Process();                            // 23, 0x00130897
	virtual int GetType() { return AUDIO_CD; }            // 24, 0x000129D9
	virtual void SetSpeakerConfig(const DSMIXBINS *mixBins);   // 25, 0x001306D2

	int m_unknown14;                       // +0x014
	int m_nState;                          // +0x018 PlayerState
	int m_nTrack;                          // +0x01C 1-based
	int m_nVolume;                         // +0x020
	wchar_t m_szArtist[64];                // +0x024 never filled
	wchar_t m_szTitle[64];                 // +0x0A4 "Track %u"
	DWORD m_dwLength;                      // +0x124 seconds
	SPlayClock m_clock;                    // +0x128
	CAppSettings *m_pSettings;             // +0x140
	BOOL m_bReady;                         // +0x144
	BOOL m_bActive;                        // +0x148
	BOOL m_bStarted;                       // +0x14C
	XCDPlayer *m_pPump;                    // +0x150
	WAVEFORMATEX m_wfx;                    // +0x154
};

// An MP3 file, decoded by CAudioDecoder (libmad). vtable 0x0024FDA4, 0x1B8
// bytes.
class CMP3Player : public CAudioPlayer
{
public:
	CMP3Player();                                         // 0x001096D3 (this in esi)
	virtual ~CMP3Player();                                // 0x001092E5 (deleting 0x00023498)
	virtual HRESULT Init(const char *path);               // 1, 0x00109197
	virtual BOOL IsReady() { return m_bReady; }           // 2, 0x00023462
	virtual HRESULT Open(const char *file);               // 3, 0x0010976A
	virtual HRESULT Play();                               // 5, 0x001097AF
	virtual HRESULT Stop();                               // 6, 0x00109078
	virtual HRESULT Pause();                              // 7, 0x001092C1
	virtual int GetState() { return m_nState; }           // 8, 0x00023469
	virtual void Unknown10() {}
	virtual void Unknown12() {}
	virtual HRESULT SetVolume(float volume);              // 13, 0x0002346D
	virtual float GetVolume() { return (float)m_nVolume; }   // 14, 0x00023490
	virtual HRESULT GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length);   // 15, 0x00109204
	virtual float GetPosition();                          // 21, 0x001091B6
	virtual void Unknown22() {}
	virtual HRESULT Process();                            // 23, 0x001090B0
	virtual int GetType() { return AUDIO_MP3; }           // 24, 0x00023494
	virtual void SetSpeakerConfig(const DSMIXBINS *mixBins);   // 25, 0x0010924C

	int m_unknown14;                       // +0x014
	int m_nState;                          // +0x018 PlayerState
	int m_nVolume;                         // +0x01C
	wchar_t m_szArtist[64];                // +0x020 "album - artist" from the ID3v1 tag
	wchar_t m_szTitle[64];                 // +0x0A0
	DWORD m_dwLength;                      // +0x120 ms
	int m_unknown124;                      // +0x124
	SPlayClock m_clock;                    // +0x128
	CAppSettings *m_pSettings;             // +0x140
	BOOL m_bReady;                         // +0x144
	BOOL m_bActive;                        // +0x148
	DSMIXBINS m_mixBins;                   // +0x14C
	DSMIXBINVOLUMEPAIR m_mixBinPairs[8];   // +0x154
	std::string m_strPath;                 // +0x194
	CAudioDecoder *m_pDecoder;             // +0x1B0
	BOOL m_bStarted;                       // +0x1B4 the decoder is producing

private:
	// The decoder for m_strPath, the volume and the tags. (this in ebx)
	HRESULT OpenDecoder();                                // 0x001095F3
	// The format from the decoder, then the ID3v1 tag (or the file's name
	// for the title). (length in eax)
	HRESULT UpdateSongInfo(wchar_t *artist, wchar_t *title, DWORD *length);   // 0x00109320
};

struct IXACTEngine;
struct IXACTSoundBank;
struct IXACTSoundCue;
struct IXACTSoundSource;

// WMA files and Xbox soundtrack songs: an XACT sound bank's "Cue1" with a
// one-entry playlist. vtable 0x0025675C, 0x1D8 bytes.
class CWMAPlayer : public CAudioPlayer
{
public:
	CWMAPlayer();                                         // 0x000D8F85 (this in esi)
	virtual ~CWMAPlayer();                                // 0x000D8A57 (this in esi; deleting 0x0002DB40)
	// The XACT engine and the sound bank file path (the data folder's).
	virtual HRESULT Init(const char *path);               // 1, 0x000D8836
	virtual BOOL IsReady() { return m_bReady; }           // 2, 0x0002DB31
	virtual HRESULT Open(const char *file);               // 3, 0x000D8DC5
	virtual HRESULT OpenSoundtrackSong(DWORD soundtrackId, DWORD songId, DWORD index);   // 4, 0x000D8C48
	virtual HRESULT Play();                               // 5, 0x000D8980
	virtual HRESULT Stop();                               // 6, 0x000D86B0
	virtual HRESULT Pause();                              // 7, 0x000D8683
	virtual int GetState() { return m_nState; }           // 8, 0x0002DB38
	virtual void Unknown10() {}
	virtual void Unknown12() {}
	// Clamped to -10000..0.
	virtual HRESULT SetVolume(float volume);              // 13, 0x000D86FE
	virtual float GetVolume() { return m_fVolume; }       // 14, 0x0002DB3C
	virtual HRESULT GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length);   // 15, 0x000D8555
	virtual float GetPosition();                          // 21, 0x000D8507
	virtual void Unknown22() {}
	virtual HRESULT Process();                            // 23, 0x000D874B
	virtual int GetType() { return AUDIO_WMA; }           // 24, 0x000111E7
	// Only while stopped for NULL (the sound source is then released).
	virtual void SetSpeakerConfig(const DSMIXBINS *mixBins);   // 25, 0x000D85BE

	int m_unknown14;                       // +0x014
	float m_fVolume;                       // +0x018
	int m_nState;                          // +0x01C PlayerState
	wchar_t m_szArtist[64];                // +0x020 never filled
	wchar_t m_szTitle[64];                 // +0x0A0
	DWORD m_dwLength;                      // +0x120 ms
	int m_unknown124;                      // +0x124
	SPlayClock m_clock;                    // +0x128
	IXACTEngine *m_pEngine;                // +0x140
	void *m_pSoundBankData;                // +0x144 the file, malloc'd
	IXACTSoundBank *m_pSoundBank;          // +0x148
	DWORD m_dwCueIndex;                    // +0x14C "Cue1"
	IXACTSoundCue *m_pCue;                 // +0x150
	IXACTSoundSource *m_pSoundSource;      // +0x154 for 5.1
	CAppSettings *m_pSettings;             // +0x158
	void *m_pfnFileIo;                     // +0x15C a callback given to XACT (0x000D85B8) TODO: verify
	int m_unknown160;                      // +0x160
	std::string m_strPath;                 // +0x164
	BOOL m_bNoFileTitle;                   // +0x180 never set: a title from the file name is always allowed
	BOOL m_bReady;                         // +0x184
	BOOL m_bStarted;                       // +0x188 the cue sent its start notification
	DSMIXBINS m_mixBins;                   // +0x18C
	DSMIXBINVOLUMEPAIR m_mixBinPairs[8];   // +0x194
	DWORD m_dwSoundtrackId;                // +0x1D4 -1 for a file

private:
	// The format, length and title of the playlist's current entry (the
	// file's name when the stream has no title). (this in eax)
	HRESULT UpdateSongInfo(wchar_t *title, DWORD *length);   // 0x000D8AD8
};

// Unused: CDDACopy (vtable 0x0024A93C, a CAppFrame listing the CD's
// tracks; 0x00130B63-0x00130C6F) has no constructor left.
