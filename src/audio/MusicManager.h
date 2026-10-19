// Background music: plays soundtracks from the HDD and audio CDs (see the
// CAudioPlayer subclasses CWMAPlayer, CMP3Player, CWAVPlayer, CDDAPlayer).
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

enum MusicState
{
	MUSIC_STOPPED = 0,
	MUSIC_PAUSED = 1,      // TODO: verify
	MUSIC_PLAYING = 2,
};

class CMusicManager
{
public:
	virtual ~CMusicManager();
	int GetState() const;               // 0x00105FCE MusicState
	void Stop();                        // 0x001061A5 TODO: decompile
	// Names of the current soundtrack and song, and the song length (ms).
	// Any can be NULL.
	HRESULT GetSongInfo(wchar_t *soundtrack, wchar_t *song, DWORD *length);   // 0x00106FA9 TODO: decompile
	SSongFormat *GetSongFormat();       // 0x00105FD5 TODO: decompile; NULL if nothing plays
	float GetSongPosition();            // 0x00106017 TODO: decompile; seconds

	BYTE m_unknown004[0x19C];
	BOOL m_bFromHdd;                    // +0x1A0 FALSE while playing an audio CD TODO: verify
};

extern CMusicManager *g_pMusicManager;  // 0x0027622C
