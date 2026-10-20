// CWMAPlayer: WMA files and Xbox soundtrack songs through XACT. The data
// folder holds a sound bank whose "Cue1" plays a WMA playlist; a song is
// played by making a new (looping) playlist holding just the file, the
// soundtrack or the soundtrack song. See AudioPlayer.h.
#include "AudioPlayer.h"

#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../settings/AppSettings.h"
#include "../system/FileIO.h"

#define WMA_TEMP_COPY "Z:\\tmp.wma"

// The song a whole-soundtrack playlist is on. (Reset to 0 before it's
// compared, so the seeking it was for never happens.)
static DWORD s_dwPlayListSong;   // 0x00278D54

// 0x000D85B8
// XACT's read callback: reads nothing, and says it worked.
static BOOL __stdcall XactReadFile(HANDLE, void *, DWORD, DWORD *, void *)
{
	return TRUE;
}

// 0x000D8F85
CWMAPlayer::CWMAPlayer()
{
	m_strPath.clear();
	m_dwSoundtrackId = (DWORD)-1;
	m_bStarted = FALSE;
	m_bReady = FALSE;
	m_nState = PLAYER_STOPPED;
	memset(&m_clock, 0, sizeof(m_clock));
	m_pEngine = NULL;
	m_pSoundBankData = NULL;
	m_pSoundBank = NULL;
	m_pPlayList = NULL;
	m_szArtist[0] = 0;
	m_szTitle[0] = 0;
	m_dwLength = 0;
	m_bNoFileTitle = FALSE;
	m_unknown14 = 0;
	m_pSoundSource = NULL;
	m_fVolume = 0.0f;
	m_mixBins.dwMixBinCount = 0;
	m_mixBins.lpMixBinVolumePairs = NULL;
	memset(m_mixBinPairs, 0, sizeof(m_mixBinPairs));
	m_pSettings = CAppSettings::GetInstance();
}

// 0x000D8A57
CWMAPlayer::~CWMAPlayer()
{
	Stop();
	if (m_pPlayList != NULL)
	{
		IXACTWmaPlayList_Release(m_pPlayList);
		m_pPlayList = NULL;
	}
	if (m_pSoundBank != NULL)
		m_pSoundBank = NULL;   // (not released)
	if (m_pSoundSource != NULL)
	{
		IXACTSoundSource_Release(m_pSoundSource);
		m_pSoundSource = NULL;
	}
	if (m_pEngine != NULL)
	{
		IXACTEngine_Release(m_pEngine);
		m_pEngine = NULL;
	}
	if (m_pSoundBankData != NULL)
		free(m_pSoundBankData);
}

// 0x000D8836
HRESULT CWMAPlayer::Init(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return E_FAIL;
	QueryPerformanceFrequency(&m_clock.frequency);
	XACT_RUNTIME_PARAMETERS params;
	memset(&params, 0, sizeof(params));
	params.dwMax2DHwVoices = 0x80;
	params.dwMax3DHwVoices = 0x20;
	params.dwMaxConcurrentStreams = 0x10;
	params.dwMaxNotifications = 0;
	m_fileIo.ReadFileCallback = XDK_CAST(XACT_READFILE_CALLBACK, XactReadFile);
	XACTEngineSetFileIOCallbacks(&m_fileIo);
	if (FAILED(XACTEngineCreate(&params, &m_pEngine)))
		return E_FAIL;
	DWORD size;
	if (FAILED(XBUtil_LoadFile(path, &m_pSoundBankData, &size)))
		return E_FAIL;
	if (FAILED(IXACTEngine_CreateSoundBank(m_pEngine, m_pSoundBankData, size, &m_pSoundBank)) ||
	    FAILED(IXACTSoundBank_GetSoundCueIndexFromFriendlyName(m_pSoundBank, "Cue1", &m_dwCueIndex)))
		return E_FAIL;
	// The cue's notifications.
	XACT_NOTIFICATION_DESCRIPTION desc;
	memset(&desc, 0, sizeof(desc));
	desc.u.pSoundBank = m_pSoundBank;
	desc.dwSoundCueIndex = m_dwCueIndex;
	desc.wType = eXACTNotification_Start;
	desc.wFlags = XACT_FLAG_NOTIFICATION_PERSIST | XACT_FLAG_NOTIFICATION_USE_SOUNDCUE_INDEX;
	if (FAILED(IXACTEngine_RegisterNotification(m_pEngine, &desc)))
		return E_FAIL;
	desc.wType = eXACTNotification_Stop;
	IXACTEngine_RegisterNotification(m_pEngine, &desc);
	desc.wType = eXACTNotification_StreamDataAvailable;
	IXACTEngine_RegisterNotification(m_pEngine, &desc);
	m_bReady = TRUE;
	m_bStarted = FALSE;
	return S_OK;
}

// 0x000D8AD8
HRESULT CWMAPlayer::UpdateSongInfo(wchar_t *title, DWORD *length)
{
	if (m_pPlayList == NULL)
		return E_FAIL;
	if (title == NULL)
		return S_OK;
	memset(&m_format, 0, sizeof(m_format));
	XACT_WMASONG_DESCRIPTION info;
	memset(&info, 0, sizeof(info));
	IXACTWmaSong *song;
	IXACTWmaPlayList_GetCurrentSongInfoEx(m_pPlayList, &info, &song);
	m_format.sampleRate = info.Header.dwSampleRate;
	m_format.channels = info.Header.dwNumChannels;
	m_format.duration = info.Header.dwDuration / 1000;
	m_format.bitRate = info.Header.dwBitrate;
	if (length != NULL)
		*length = info.Header.dwDuration;
	IXACTWmaPlayList_GetCurrentSongInfo(m_pPlayList, length, title, 0x40, NULL);
	if (title[0] == 0 && !m_bNoFileTitle)
	{
		// The file's name.
		std::string name = m_strPath;
		int slash = StrReverseFind(name, "\\", -1);
		if (slash != -1)
			name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
		int dot = StrReverseFind(name, ".", -1);
		if (dot != -1)
			name = StrLeft(name, dot);
		MultiByteToWideChar(CP_ACP, 0, name.c_str(), (int)name.size(), title, 0x20);
	}
	return S_OK;
}

// 0x000D8DC5
// A file elsewhere than a hard disk drive (FileExists fails) is first
// copied to Z:\tmp.wma. (Quirk: the path's _strdup copy is never freed.)
HRESULT CWMAPlayer::Open(const char *file)
{
	m_dwSoundtrackId = (DWORD)-1;
	Stop();
	if (m_pPlayList != NULL)
	{
		IXACTWmaPlayList_Release(m_pPlayList);
		m_pPlayList = NULL;
	}
	if (FAILED(IXACTSoundBank_CreateWmaPlayList(m_pSoundBank, m_dwCueIndex, XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP,
	                                            &m_pPlayList)))
		return E_FAIL;
	m_strPath = strdup(file);
	if (!FileExists(m_strPath.c_str()))
	{
		CDirectory *directory = CreateDirObject(m_strPath.c_str());
		if (directory != NULL)
		{
			std::string temp = WMA_TEMP_COPY;
			if (directory->CopyFile(m_strPath.c_str(), temp.c_str(), FALSE))
				m_strPath = temp;
			delete directory;
		}
		if (!FileExists(m_strPath.c_str()))
			return E_FAIL;
	}
	XACT_WMA_PLAYLIST_ADD entry;
	memset(&entry, 0, sizeof(entry));
	entry.dwType = eXACTWmaPlayListAdd_File;
	entry.pszFileName = m_strPath.c_str();
	IXACTWmaPlayList_Add(m_pPlayList, &entry, NULL);
	IXACTWmaPlayList_Next(m_pPlayList);
	XACT_WMA_PLAYLIST_PROPERTIES info;
	memset(&info, 0, sizeof(info));
	IXACTWmaPlayList_GetProperties(m_pPlayList, &info);
	if (info.dwSongEntryCount == 0)
		return E_FAIL;
	memset(m_szArtist, 0, sizeof(m_szArtist));
	memset(m_szTitle, 0, sizeof(m_szTitle));
	UpdateSongInfo(m_szTitle, &m_dwLength);
	return m_dwLength != 0 ? S_OK : E_FAIL;
}

// 0x000D8C48
// songId 0: the whole soundtrack.
HRESULT CWMAPlayer::OpenSoundtrackSong(DWORD soundtrackId, DWORD songId, DWORD index)
{
	Stop();
	BOOL wholeSoundtrack = FALSE;
	s_dwPlayListSong = 0;
	if (m_pPlayList != NULL)
	{
		IXACTWmaPlayList_Release(m_pPlayList);
		m_pPlayList = NULL;
	}
	if (FAILED(IXACTSoundBank_CreateWmaPlayList(m_pSoundBank, m_dwCueIndex, XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP,
	                                            &m_pPlayList)))
		return E_FAIL;
	XACT_WMA_PLAYLIST_ADD entry;
	entry.pszFileName = NULL;
	entry.dwSoundtrackId = soundtrackId;
	entry.dwSongId = 0;
	entry.dwSongIndex = 0;
	if (songId == 0)
	{
		wholeSoundtrack = TRUE;
		entry.dwType = eXACTWmaPlayListAdd_Soundtrack;
	}
	else
	{
		entry.dwType = eXACTWmaPlayListAdd_SoundtrackSong;
		entry.dwSongId = songId;
		entry.dwSongIndex = index;
	}
	IXACTWmaPlayList_Add(m_pPlayList, &entry, NULL);
	if (s_dwPlayListSong == songId || !wholeSoundtrack)
		IXACTWmaPlayList_Next(m_pPlayList);
	XACT_WMA_PLAYLIST_PROPERTIES info;
	memset(&info, 0, sizeof(info));
	IXACTWmaPlayList_GetProperties(m_pPlayList, &info);
	if (info.dwSongEntryCount == 0)
		return E_FAIL;
	if (wholeSoundtrack && s_dwPlayListSong != songId)
	{
		// (Unreachable: both are 0 here.)
		if (songId == 0)
		{
			IXACTWmaPlayList_SetCurrent(m_pPlayList, info.pFirstSong);
			s_dwPlayListSong = 0;
		}
		else if (songId == info.dwSongEntryCount - 1)
		{
			IXACTWmaPlayList_SetCurrent(m_pPlayList, info.pLastSong);
			s_dwPlayListSong = songId;
		}
		else if (s_dwPlayListSong < songId)
		{
			do
			{
				s_dwPlayListSong++;
				IXACTWmaPlayList_Next(m_pPlayList);
			} while (s_dwPlayListSong != songId);
		}
		else if (songId < s_dwPlayListSong)
		{
			do
			{
				s_dwPlayListSong--;
				IXACTWmaPlayList_Previous(m_pPlayList);
			} while (s_dwPlayListSong != songId);
		}
	}
	m_dwSoundtrackId = soundtrackId;
	m_strPath.clear();
	memset(m_szArtist, 0, sizeof(m_szArtist));
	memset(m_szTitle, 0, sizeof(m_szTitle));
	UpdateSongInfo(m_szTitle, &m_dwLength);
	return S_OK;
}

// 0x000D8980
HRESULT CWMAPlayer::Play()
{
	if (m_nState == PLAYER_PLAYING)
		return S_FALSE;
	if (m_nState == PLAYER_STOPPED)
	{
		if (m_mixBins.dwMixBinCount != 0)
			SetSpeakerConfig(&m_mixBins);
		if (m_pSoundBank == NULL)
			return E_FAIL;
		IXACTSoundSource *source = m_mixBins.dwMixBinCount != 0 ? m_pSoundSource : NULL;
		if (FAILED(IXACTSoundBank_Play(m_pSoundBank, m_dwCueIndex, source, XACT_FLAG_SOUNDCUE_AUTORELEASE, NULL)))
			return E_FAIL;
	}
	else if (m_nState == PLAYER_PAUSED && m_pEngine != NULL)
	{
		IXACTEngine_GlobalPause(m_pEngine, 0, FALSE);
	}
	m_clock.Start();
	m_nState = PLAYER_PLAYING;
	m_bStarted = TRUE;
	return S_OK;
}

// 0x000D86B0
// Then handles the cue's leftover notifications (up to 100).
HRESULT CWMAPlayer::Stop()
{
	if (m_nState == PLAYER_STOPPED)
		return S_FALSE;
	if (m_pSoundBank != NULL)
		IXACTSoundBank_Stop(m_pSoundBank, m_dwCueIndex, 0, NULL);
	m_clock.played = 0;
	m_nState = PLAYER_STOPPED;
	for (int i = 0; i < 100; i++)
	{
		if (FAILED(Process()))
			break;
	}
	return S_OK;
}

// 0x000D8683
HRESULT CWMAPlayer::Pause()
{
	if (m_nState != PLAYER_PLAYING)
		return S_FALSE;
	if (m_pEngine != NULL)
		IXACTEngine_GlobalPause(m_pEngine, 0, TRUE);
	m_nState = PLAYER_PAUSED;
	return S_OK;
}

// 0x000D86FE
HRESULT CWMAPlayer::SetVolume(float volume)
{
	m_fVolume = volume;
	if (volume < -10000.0f)
		m_fVolume = -10000.0f;
	else if (volume > 0.0f)
		m_fVolume = 0.0f;
	if (m_pEngine != NULL && SUCCEEDED(IXACTEngine_SetMasterVolume(m_pEngine, 0, (LONG)m_fVolume)))
		return S_OK;
	return E_FAIL;
}

// 0x000D8555
// (Nothing is filled in until the length is known.)
HRESULT CWMAPlayer::GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length)
{
	if (m_pPlayList == NULL)
		return E_FAIL;
	if (m_dwLength != 0)
	{
		if (artist != NULL)
			wcscpy(artist, m_szArtist);
		if (title != NULL)
			wcscpy(title, m_szTitle);
		if (length != NULL)
			*length = m_dwLength;
	}
	return S_OK;
}

// 0x000D8507
float CWMAPlayer::GetPosition()
{
	if (!IsReady())
		return 0.0f;
	return m_clock.GetSeconds();
}

// 0x000D874B
HRESULT CWMAPlayer::Process()
{
	XACTEngineDoWork();
	if (m_pEngine == NULL)
		return E_FAIL;
	XACT_NOTIFICATION notification;
	while (IXACTEngine_GetNotification(m_pEngine, NULL, &notification) == S_OK)
	{
		if (notification.Header.dwSoundCueIndex != m_dwCueIndex || notification.Header.u.pSoundBank != m_pSoundBank)
			continue;
		if (notification.Header.wType == eXACTNotification_Start)
		{
			// (In a __try block in the original.)
			for (int i = 0; i < 500; i++)
				XACTEngineDoWork();
		}
		else if (notification.Header.wType == eXACTNotification_Stop)
		{
			return E_FAIL;
		}
		else if (notification.Header.wType == eXACTNotification_StreamDataAvailable)
		{
			m_bStarted = TRUE;
		}
	}
	if (m_bStarted && GetState() == PLAYER_PLAYING)
		m_clock.Update();
	return S_OK;
}

// 0x000D85BE
void CWMAPlayer::SetSpeakerConfig(const DSMIXBINS *mixBins)
{
	if (mixBins == NULL)
	{
		// Stereo: the sound source goes, when stopped.
		m_mixBins.dwMixBinCount = 0;
		m_mixBins.lpMixBinVolumePairs = NULL;
		if (m_pSoundSource == NULL || m_nState != PLAYER_STOPPED)
			return;
		IXACTSoundSource_StopSoundCues(m_pSoundSource);
	}
	else if (mixBins == &m_mixBins)
	{
		if (m_pSoundSource != NULL)
			IXACTSoundSource_SetMixBinVolumes(m_pSoundSource, &m_mixBins);
		return;
	}
	else
	{
		// A copy of the mix bins, set on a (new) 2D sound source.
		m_mixBins.dwMixBinCount = mixBins->dwMixBinCount;
		for (UINT i = 0; i < m_mixBins.dwMixBinCount; i++)
			m_mixBinPairs[i] = mixBins->lpMixBinVolumePairs[i];
		m_mixBins.lpMixBinVolumePairs = m_mixBinPairs;
		if (m_pSoundSource != NULL ||
		    SUCCEEDED(IXACTEngine_CreateSoundSource(m_pEngine, XACT_FLAG_SOUNDSOURCE_2D, &m_pSoundSource)))
		{
			IXACTSoundSource_SetMixBins(m_pSoundSource, &m_mixBins);
			return;
		}
		if (m_pSoundSource == NULL)
			return;
	}
	IXACTSoundSource_Release(m_pSoundSource);
	m_pSoundSource = NULL;
}
