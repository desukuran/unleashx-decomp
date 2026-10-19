// DirectSound and XACT (XDK). On the Xbox these come from <dsound.h> and
// <xact.h>; on other hosts only what the recovered code uses is declared,
// so it can be syntax-checked.
#pragma once

#include "../common/Platform.h"

#ifdef _XBOX
#include <dsound.h>
#include <xact.h>
#else
struct IDirectSound;
struct IDirectSoundBuffer
{
	HRESULT SetCurrentPosition(DWORD position);
	HRESULT SetVolume(LONG volume);
	HRESULT Stop();
};

struct WAVEFORMATEX
{
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
};
struct GUID
{
	DWORD Data1;
	WORD Data2, Data3;
	BYTE Data4[8];
};
struct WAVEFORMATEXTENSIBLE
{
	WAVEFORMATEX Format;
	union
	{
		WORD wValidBitsPerSample;
		WORD wSamplesPerBlock;
		WORD wReserved;
	} Samples;
	DWORD dwChannelMask;
	GUID SubFormat;
};
extern const GUID KSDATAFORMAT_SUBTYPE_PCM;   // 0x0019CF90
#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

struct DSMIXBINVOLUMEPAIR
{
	DWORD dwMixBin;
	LONG lVolume;
};
struct DSMIXBINS
{
	DWORD dwMixBinCount;
	DSMIXBINVOLUMEPAIR *lpMixBinVolumePairs;
};
struct DSBUFFERDESC
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwBufferBytes;
	WAVEFORMATEX *lpwfxFormat;
	DSMIXBINS *lpMixBins;
	DWORD dwInputMixBin;
};
struct DSBPOSITIONNOTIFY
{
	DWORD dwOffset;
	HANDLE hEventNotify;
};
struct DSVOICEPROPS
{
	DWORD dwMixBinCount;
	DSMIXBINVOLUMEPAIR MixBinVolumePairs[8];
	LONG lPitch;
	LONG lI3DL2DirectVolume;
	LONG lI3DL2RoomVolume;
};

#define DSBCAPS_CTRLFREQUENCY 0x00000020
#define DSBCAPS_CTRLVOLUME 0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY 0x00000100
#define DSBPLAY_LOOPING 0x00000001
#define DSBPLAY_FROMSTART 0x00000002
#define DSBSTATUS_PLAYING 0x00000001
#define DSBSTATUS_PAUSED 0x00000002
#define DSBPAUSE_RESUME 0
#define DSBPAUSE_PAUSE 1
#define DSBLOCK_ENTIREBUFFER 0x00000002
#define DSBVOLUME_MIN (-6400)
#define DSBVOLUME_MAX 0
#define DSBSTOPEX_IMMEDIATE 0
#define XC_AUDIO_FLAGS_SURROUND 0x00000003

DWORD XGetAudioFlags();
HRESULT DirectSoundCreate(void *device, IDirectSound **directSound, void *outer);
void DirectSoundDoWork();
HRESULT DirectSoundCreateBuffer(const DSBUFFERDESC *desc, IDirectSoundBuffer **buffer);
HRESULT IDirectSound_SetMixBinHeadroom(IDirectSound *directSound, DWORD mixBinMask, DWORD headroom);
HRESULT IDirectSound_CreateSoundBuffer(IDirectSound *directSound, const DSBUFFERDESC *desc,
                                       IDirectSoundBuffer **buffer, void *outer);
ULONG IDirectSound_Release(IDirectSound *directSound);

HRESULT IDirectSoundBuffer_Lock(IDirectSoundBuffer *buffer, DWORD offset, DWORD bytes, void **data1, DWORD *bytes1,
                                void **data2, DWORD *bytes2, DWORD flags);
HRESULT IDirectSoundBuffer_Unlock(IDirectSoundBuffer *buffer, void *data1, DWORD bytes1, void *data2, DWORD bytes2);
HRESULT IDirectSoundBuffer_Play(IDirectSoundBuffer *buffer, DWORD reserved1, DWORD reserved2, DWORD flags);
HRESULT IDirectSoundBuffer_Stop(IDirectSoundBuffer *buffer);
HRESULT IDirectSoundBuffer_StopEx(IDirectSoundBuffer *buffer, LONGLONG timeStamp, DWORD flags);
HRESULT IDirectSoundBuffer_Pause(IDirectSoundBuffer *buffer, DWORD pause);
HRESULT IDirectSoundBuffer_SetCurrentPosition(IDirectSoundBuffer *buffer, DWORD position);
HRESULT IDirectSoundBuffer_GetCurrentPosition(IDirectSoundBuffer *buffer, DWORD *play, DWORD *write);
HRESULT IDirectSoundBuffer_GetStatus(IDirectSoundBuffer *buffer, DWORD *status);
HRESULT IDirectSoundBuffer_SetVolume(IDirectSoundBuffer *buffer, LONG volume);
HRESULT IDirectSoundBuffer_SetHeadroom(IDirectSoundBuffer *buffer, DWORD headroom);
HRESULT IDirectSoundBuffer_SetNotificationPositions(IDirectSoundBuffer *buffer, DWORD count,
                                                    const DSBPOSITIONNOTIFY *notifies);
HRESULT IDirectSoundBuffer_SetMixBins(IDirectSoundBuffer *buffer, const DSMIXBINS *mixBins);
HRESULT IDirectSoundBuffer_SetFormat(IDirectSoundBuffer *buffer, const WAVEFORMATEX *format);
HRESULT IDirectSoundBuffer_GetVoiceProperties(IDirectSoundBuffer *buffer, DSVOICEPROPS *props);
ULONG IDirectSoundBuffer_Release(IDirectSoundBuffer *buffer);

// The WMA decoder (XDK).
struct IXMediaObject;
struct XMEDIAPACKET
{
	void *pvBuffer;
	DWORD dwMaxSize;
	DWORD *pdwCompletedSize;
	DWORD *pdwStatus;
	HANDLE hCompletionEvent;
	LONGLONG *prtTimestamp;
};
HRESULT WmaCreateDecoder(const char *fileName, HANDLE file, BOOL async, DWORD lookaheadSize, DWORD maxPackets,
                         DWORD yieldRate, WAVEFORMATEX *format, IXMediaObject **decoder);   // 0x001A014D
HRESULT IXMediaObject_Process(IXMediaObject *object, const XMEDIAPACKET *input, const XMEDIAPACKET *output);
HRESULT IXMediaObject_Discontinuity(IXMediaObject *object);
ULONG IXMediaObject_Release(IXMediaObject *object);

// XACT.
struct IXACTEngine;
struct IXACTSoundBank;
struct IXACTSoundSource;
struct IXACTSoundCue;
struct XACT_RUNTIME_PARAMETERS
{
	DWORD dwMax2DHwVoices;
	DWORD dwMax3DHwVoices;
	DWORD dwMaxConcurrentStreams;
	DWORD dwMaxNotifications;
};
struct XACT_NOTIFICATION_DESCRIPTION
{
	WORD wType;
	WORD wFlags;
	IXACTSoundBank *pSoundBank;
	DWORD dwSoundCueIndex;
	IXACTSoundCue *pSoundCue;
	void *pvContext;
	HANDLE hEvent;
};
struct XACT_NOTIFICATION
{
	XACT_NOTIFICATION_DESCRIPTION Header;
	BYTE Data[0x30];
};
#define XACT_NOTIFICATION_TYPE_CUESTART 0
#define XACT_NOTIFICATION_TYPE_CUESTOP 1
#define XACT_NOTIFICATION_TYPE_MARKER 3
#define XACT_FLAG_NOTIFICATION_USE_SOUNDCUE_INDEX 0x0002   // TODO: verify
#define XACT_FLAG_NOTIFICATION_PERSIST 0x8000              // TODO: verify
#define XACT_FLAG_SOUNDCUE_AUTORELEASE 0x0002              // TODO: verify
#define XACT_SOUNDSOURCE_2D 1                              // TODO: verify

HRESULT XACTEngineCreate(const XACT_RUNTIME_PARAMETERS *params, IXACTEngine **engine);
void XACTEngineDoWork();
HRESULT IXACTEngine_CreateSoundBank(IXACTEngine *engine, const void *data, DWORD size, IXACTSoundBank **bank);
HRESULT IXACTEngine_CreateSoundSource(IXACTEngine *engine, DWORD flags, IXACTSoundSource **source);
HRESULT IXACTEngine_RegisterNotification(IXACTEngine *engine, const XACT_NOTIFICATION_DESCRIPTION *desc);
HRESULT IXACTEngine_GetNotification(IXACTEngine *engine, const XACT_NOTIFICATION_DESCRIPTION *desc,
                                    XACT_NOTIFICATION *notification);
HRESULT IXACTSoundBank_GetSoundCueIndexFromFriendlyName(IXACTSoundBank *bank, const char *name, DWORD *index);
#endif

// XACT internals the WMA player calls directly (XACTENG section; not in
// the symbol database). The names are reconstructed from how they're
// used. TODO: verify
ULONG XactEngineRelease(IXACTEngine *engine);                                             // 0x0022221E
HRESULT XactEngineSetMasterVolume(IXACTEngine *engine, DWORD unused, LONG volume);        // 0x002222B8
HRESULT XactEnginePause(IXACTEngine *engine, DWORD unused, BOOL pause);                   // 0x002222F7
HRESULT XactSoundBankStop(IXACTSoundBank *bank, DWORD cueIndex, DWORD flags, IXACTSoundCue *cue);   // 0x00222416
HRESULT XactSoundBankPrepare(IXACTSoundBank *bank, DWORD cueIndex, DWORD flags, IXACTSoundCue **cue);   // 0x0022244D
// The playing song's title (up to length characters).
HRESULT XactSoundCueGetSongName(IXACTSoundCue *cue, DWORD *length, wchar_t *name, DWORD size, DWORD flags);   // 0x002224A2
// A cue's playlist entry: a file, a whole soundtrack, or one song.
struct SXactPlayListEntry
{
	DWORD type;            // +0x00 2 file, 3 soundtrack, 4 soundtrack song
	DWORD idOrPath;        // +0x04 the soundtrack ID, or the path
	DWORD songId;          // +0x08
	DWORD songIndex;       // +0x0C
};
struct SXactPlayListInfo
{
	DWORD unknown00;       // +0x00
	DWORD count;           // +0x04
	DWORD firstEntry;      // +0x08
	DWORD lastEntry;       // +0x0C
};
HRESULT XactSoundCueSetPlayList(IXACTSoundCue *cue, const SXactPlayListEntry *entry, DWORD flags);   // 0x00224F08
HRESULT XactSoundCueGetPlayListInfo(IXACTSoundCue *cue, SXactPlayListInfo *info);         // 0x002252E7
// The current entry's stream format and length (0x34 bytes, see
// CWMAPlayer::UpdateSongInfo).
HRESULT XactSoundCueGetStreamInfo(IXACTSoundCue *cue, void *info, DWORD *size);           // 0x00225459
HRESULT XactSoundCueSelectEntry(IXACTSoundCue *cue, DWORD entry);                         // 0x002258B1
HRESULT XactSoundCueNextEntry(IXACTSoundCue *cue);                                        // 0x00225915
HRESULT XactSoundCuePrevEntry(IXACTSoundCue *cue);                                        // 0x00225978
ULONG XactSoundCueRelease(IXACTSoundCue *cue);                                            // 0x00225A1E
// (Before releasing it for stereo.)
HRESULT XactSoundSourceDetach(IXACTSoundSource *source);                                 // 0x00225C9F
HRESULT XactSoundSourceSetMixBins(IXACTSoundSource *source, const DSMIXBINS *mixBins);    // 0x00225DAA
// (With the player's own mix bins again.)
HRESULT XactSoundSourceApplyMixBins(IXACTSoundSource *source, const DSMIXBINS *mixBins);  // 0x00225DF0
ULONG XactSoundSourceRelease(IXACTSoundSource *source);                                   // 0x00226824
HRESULT XactSetFileIoCallbacks(void *callbacks);                                          // 0x0022339A
// IXACTSoundBank_Play, inlined: PlayEx with {1, cueIndex, source, NULL}.
HRESULT XactSoundBankPlay(IXACTSoundBank *bank, DWORD cueIndex, IXACTSoundSource *source, DWORD flags);   // 0x0002DB07
