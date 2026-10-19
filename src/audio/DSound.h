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
struct IXACTWmaPlayList;
struct IXACTWmaSong;
struct XACT_RUNTIME_PARAMETERS
{
	DWORD dwMax2DHwVoices;
	DWORD dwMax3DHwVoices;
	DWORD dwMaxConcurrentStreams;
	DWORD dwMaxNotifications;
	DWORD dwInteractiveAudioLookaheadTime;
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
enum XACT_NOTIFICATION_TYPE
{
	eXACTNotification_Start = 0,
	eXACTNotification_Stop,
	eXACTNotification_Marker,
	eXACTNotification_StreamDataAvailable,
};
#define XACT_FLAG_NOTIFICATION_USE_SOUNDCUE_INDEX 0x0002
#define XACT_FLAG_NOTIFICATION_PERSIST 0x8000
#define XACT_FLAG_SOUNDCUE_AUTORELEASE 0x0001
#define XACT_FLAG_SOUNDSOURCE_2D 0x00000001
#define XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP 0x00000002
struct XACT_FILEIO_CALLBACKS
{
	BOOL(__stdcall *ReadFileCallback)(HANDLE file, void *buffer, DWORD size, DWORD *read, void *overlapped);
	BOOL(__stdcall *GetOverlappedResultCallback)(HANDLE file, void *overlapped, DWORD *transferred, BOOL wait);
};
struct XACT_PREPARE_SOUNDCUE
{
	DWORD dwFlags;
	DWORD dwCueIndex;
	IXACTSoundSource *pSoundSource;
	const void *pParameterControls;
};
// What a WMA playlist entry is.
enum XACT_WMA_PLAYLIST_ADD_TYPE
{
	eXACTWmaPlayListAdd_Directory = 1,
	eXACTWmaPlayListAdd_File,
	eXACTWmaPlayListAdd_Soundtrack,
	eXACTWmaPlayListAdd_SoundtrackSong,
};
struct XACT_WMA_PLAYLIST_ADD
{
	DWORD dwType;
	const char *pszFileName;
	DWORD dwSoundtrackId;
	DWORD dwSongId;
	DWORD dwSongIndex;
};
struct XACT_WMA_PLAYLIST_PROPERTIES
{
	DWORD dwPlaybackFlags;
	DWORD dwSongEntryCount;
	IXACTWmaSong *pFirstSong;
	IXACTWmaSong *pLastSong;
};
#pragma pack(push, 1)
struct WMAXMOFileContDesc
{
	WORD wTitleLength;
	WORD wAuthorLength;
	WORD wCopyrightLength;
	WORD wDescriptionLength;
	WORD wRatingLength;
	wchar_t *pTitle;
	wchar_t *pAuthor;
	wchar_t *pCopyright;
	wchar_t *pDescription;
	wchar_t *pRating;
};
#pragma pack(pop)
struct WMAXMOFileHeader
{
	DWORD dwVersion;
	DWORD dwSampleRate;
	DWORD dwNumChannels;
	DWORD dwDuration;   // ms
	DWORD dwBitrate;
};
struct XACT_WMASONG_DESCRIPTION
{
	WMAXMOFileContDesc Content;
	WMAXMOFileHeader Header;
};

HRESULT XACTEngineCreate(const XACT_RUNTIME_PARAMETERS *params, IXACTEngine **engine);
void XACTEngineDoWork();
void XACTEngineSetFileIOCallbacks(const XACT_FILEIO_CALLBACKS *callbacks);
ULONG IXACTEngine_Release(IXACTEngine *engine);
HRESULT IXACTEngine_CreateSoundBank(IXACTEngine *engine, void *data, DWORD size, IXACTSoundBank **bank);
HRESULT IXACTEngine_CreateSoundSource(IXACTEngine *engine, DWORD flags, IXACTSoundSource **source);
HRESULT IXACTEngine_SetMasterVolume(IXACTEngine *engine, WORD category, LONG volume);
HRESULT IXACTEngine_GlobalPause(IXACTEngine *engine, WORD category, BOOL pause);
HRESULT IXACTEngine_RegisterNotification(IXACTEngine *engine, const XACT_NOTIFICATION_DESCRIPTION *desc);
HRESULT IXACTEngine_GetNotification(IXACTEngine *engine, const XACT_NOTIFICATION_DESCRIPTION *desc,
                                    XACT_NOTIFICATION *notification);
HRESULT IXACTSoundBank_GetSoundCueIndexFromFriendlyName(IXACTSoundBank *bank, const char *name, DWORD *index);
HRESULT IXACTSoundBank_PlayEx(IXACTSoundBank *bank, const XACT_PREPARE_SOUNDCUE *prepare, IXACTSoundCue **cue);
// An inline in <xact.h>: PlayEx with {flags, cueIndex, source, NULL}.
// (UnleashX has an out-of-line copy at 0x0002DB07, flags in eax.)
inline HRESULT IXACTSoundBank_Play(IXACTSoundBank *bank, DWORD cueIndex, IXACTSoundSource *source, DWORD flags,
                                   IXACTSoundCue **cue)
{
	XACT_PREPARE_SOUNDCUE prepare;
	prepare.dwFlags = flags;
	prepare.dwCueIndex = cueIndex;
	prepare.pSoundSource = source;
	prepare.pParameterControls = NULL;
	return IXACTSoundBank_PlayEx(bank, &prepare, cue);
}
HRESULT IXACTSoundBank_Stop(IXACTSoundBank *bank, DWORD cueIndex, DWORD flags, IXACTSoundCue *cue);
HRESULT IXACTSoundBank_CreateWmaPlayList(IXACTSoundBank *bank, DWORD cueIndex, DWORD playbackFlags,
                                         IXACTWmaPlayList **playList);
ULONG IXACTSoundSource_Release(IXACTSoundSource *source);
HRESULT IXACTSoundSource_SetMixBins(IXACTSoundSource *source, const DSMIXBINS *mixBins);
HRESULT IXACTSoundSource_SetMixBinVolumes(IXACTSoundSource *source, const DSMIXBINS *mixBins);
HRESULT IXACTSoundSource_StopSoundCues(IXACTSoundSource *source);
ULONG IXACTWmaPlayList_Release(IXACTWmaPlayList *playList);
HRESULT IXACTWmaPlayList_Add(IXACTWmaPlayList *playList, const XACT_WMA_PLAYLIST_ADD *desc, IXACTWmaSong **song);
HRESULT IXACTWmaPlayList_SetCurrent(IXACTWmaPlayList *playList, IXACTWmaSong *song);
HRESULT IXACTWmaPlayList_Next(IXACTWmaPlayList *playList);
HRESULT IXACTWmaPlayList_Previous(IXACTWmaPlayList *playList);
HRESULT IXACTWmaPlayList_GetCurrentSongInfo(IXACTWmaPlayList *playList, DWORD *length, wchar_t *name, DWORD size,
                                            IXACTWmaSong **song);
HRESULT IXACTWmaPlayList_GetCurrentSongInfoEx(IXACTWmaPlayList *playList, XACT_WMASONG_DESCRIPTION *desc,
                                              IXACTWmaSong **song);
HRESULT IXACTWmaPlayList_GetProperties(IXACTWmaPlayList *playList, XACT_WMA_PLAYLIST_PROPERTIES *properties);
#endif
